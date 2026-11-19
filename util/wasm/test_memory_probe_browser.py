#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Check the shared-memory ceiling probe (client/web/startup.js) in a real browser.

A page script replaces WebAssembly.Memory with one that throws, like Chrome's
"RangeError: WebAssembly.Memory(): could not allocate memory", whenever the
requested maximum is above a limit. That is what a phone or a 32-bit browser
does to the shipping 2 GiB + 64 KiB ceiling. The generated client must then:

* reserve a smaller ceiling and hand that memory to the unmodified
  luanti.wasm (there is only one binary), reaching the launcher with Play
  enabled, and, with --play, enter a world and run in it;
* or, when no ceiling is granted, stop before the engine starts and say why
  in the launcher instead of leaving a spinner.

The unit tests of the probe itself are util/wasm/test_startup.mjs.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import threading

from playwright.sync_api import Page, sync_playwright

from serve import create_server

MIB = 1048576
PAGE = 65536
SHIPPING_MAXIMUM = 2147549184

# Runs before any page script. `limit` is the largest `maximum`, in bytes, the
# fake browser grants; larger requests fail the way Chrome's allocator does.
LIMIT_MEMORY = """(limit) => {
	const Real = WebAssembly.Memory;
	const requests = [];
	function Limited(descriptor) {
		requests.push({maximum: descriptor && descriptor.maximum * 65536,
			shared: !!(descriptor && descriptor.shared)});
		if (descriptor && descriptor.maximum * 65536 > limit)
			throw new RangeError("WebAssembly.Memory(): could not allocate memory");
		return new Real(descriptor);
	}
	Limited.prototype = Real.prototype;
	WebAssembly.Memory = Limited;
	window.__memoryRequests = requests;
}"""

STATE = """() => ({
	startup: Module.luantiStartup ? {
		fatal: Module.luantiStartup.fatal,
		memory: Module.luantiStartup.memory,
		heap: Module.luantiStartup.heapBytes(),
	} : null,
	launcher: Module.luantiLauncher ? Module.luantiLauncher.getState() : null,
	playDisabled: document.getElementById('luanti-play-button').disabled,
	playLabel: document.getElementById('luanti-play-label').textContent,
	error: (el => el.hidden ? '' : el.textContent)(document.getElementById('luanti-launch-error')),
	loadingPhase: document.getElementById('luanti-loading-phase').textContent,
	requests: window.__memoryRequests || [],
})"""


# A browser without WebAssembly SIMD: validate() rejects everything, so the
# probe module in startup.js does not validate.
NO_SIMD = "() => { WebAssembly.validate = () => false; }"


def open_page(browser, base: str, query: str, limit: int | None, console: list[str],
		errors: list[str], viewport: dict | None = None, init: str = "") -> Page:
	context = browser.new_context(viewport=viewport or {"width": 1280, "height": 720})
	page = context.new_page()
	page.on("console", lambda message: console.append(f"{message.type}: {message.text}"))
	page.on("pageerror", lambda error: errors.append(str(getattr(error, "stack", None) or error)))
	if limit is not None:
		page.add_init_script(f"({LIMIT_MEMORY})({limit})")
	if init:
		page.add_init_script(f"({init})()")
	page.goto(f"{base}/luanti.html?{query}", wait_until="domcontentloaded", timeout=60_000)
	return page


def wait_for_launcher(page: Page) -> None:
	page.wait_for_function(
		"Module.luantiLauncher?.getState().runtimeReady === true && "
		"Module.luantiLauncher.getState().coreReady === true && "
		"!document.getElementById('luanti-play-button').disabled", timeout=120_000)


def check_default(browser, base: str, args) -> dict | None:
	console: list[str] = []
	errors: list[str] = []
	page = open_page(browser, base, "game=devtest", None, console, errors)
	wait_for_launcher(page)
	state = page.evaluate(STATE)
	build = page.evaluate("Module.luantiBuild || null")
	page.context.close()
	memory = state["startup"]["memory"]
	assert not errors, errors
	assert memory["state"] == "reserved" and not memory["reduced"], state
	assert memory["maximum"] == SHIPPING_MAXIMUM, state
	assert len(memory["attempts"]) == 1, state
	print(f"default: reserved {memory['maximum'] // MIB} MiB in "
		f"{memory['attempts'][0]['ms']:.1f} ms (build: {build})")
	return build


def check_simd_refusal(browser, base: str, args) -> None:
	"""A SIMD build on a browser without SIMD must say so before reserving anything."""
	console: list[str] = []
	errors: list[str] = []
	page = open_page(browser, base, "game=devtest", None, console, errors,
		{"width": 390, "height": 760}, init=NO_SIMD)
	# Count memories the page asks for: a refused SIMD build must ask for none.
	page.wait_for_function("Module.luantiStartup?.fatal", timeout=60_000)
	page.wait_for_function(
		"!document.getElementById('luanti-launch-error').hidden", timeout=10_000)
	state = page.evaluate(STATE)
	page.context.close()
	assert state["startup"]["fatal"]["code"] == "simd", state
	assert "needs WebAssembly SIMD" in state["error"], state
	assert state["playDisabled"] and not state["launcher"]["runtimeReady"], state
	assert state["startup"]["memory"]["state"] == "skipped", state
	assert len(errors) == 1 and "needs WebAssembly SIMD" in errors[0], errors
	print("SIMD build without SIMD: refused before any memory was reserved:", state["error"][:90])


def check_fallback(browser, base: str, args, limit_mib: int) -> None:
	console: list[str] = []
	errors: list[str] = []
	page = open_page(browser, base, "game=luanti_web&view=60", limit_mib * MIB, console, errors)
	wait_for_launcher(page)
	menu = page.evaluate(STATE)
	startup = menu["startup"]
	memory = startup["memory"]
	assert not errors, {"errors": errors, "console": console[-30:]}
	assert startup["fatal"] is None, menu
	assert memory["state"] == "reserved" and memory["reduced"], menu
	assert memory["maximum"] <= limit_mib * MIB, menu
	# The probed memory is the one in use: one successful shared reservation
	# at the chosen ceiling, from the page, and none above it after the probe.
	granted = [r for r in menu["requests"] if r["maximum"] <= limit_mib * MIB]
	assert [r["maximum"] for r in granted] == [memory["maximum"]], menu
	assert all(r["shared"] for r in menu["requests"]), menu
	assert not menu["playDisabled"], menu
	assert menu["startup"]["heap"] >= memory["initial"], menu
	assert menu["startup"]["heap"] <= memory["maximum"], menu
	print(f"limit {limit_mib} MiB: menu reached on a {memory['maximum'] // MIB} MiB ceiling "
		f"after {len(memory['attempts'])} attempt(s): "
		+ ", ".join(f"{a['maximum'] // MIB} MiB {'ok' if a['ok'] else 'refused'}"
			for a in memory["attempts"]))

	if args.play:
		page.click("#luanti-play-button")
		page.wait_for_function(
			"Module.luantiLauncher?.getState().phase === 'playing'", timeout=300_000)
		page.wait_for_timeout(args.settle * 1000)
		playing = page.evaluate(STATE)
		heap = playing["startup"]["heap"]
		assert not errors, {"errors": errors, "console": console[-30:]}
		assert heap <= playing["startup"]["memory"]["maximum"], playing
		aborted = [line for line in console if "Aborted(" in line or "out of memory" in line.lower()]
		assert not aborted, aborted
		print(f"limit {limit_mib} MiB: world entered, heap {heap // MIB} MiB of "
			f"{playing['startup']['memory']['maximum'] // MIB} MiB")
	page.context.close()


def check_refused(browser, base: str, args) -> None:
	console: list[str] = []
	errors: list[str] = []
	# 128 MiB is below the 256 MiB the binary needs to start, so every ceiling
	# fails. Phone-sized: the browsers that land here are phones, and the
	# launcher form is taller than their screen, so the reason has to be
	# scrolled into view.
	page = open_page(browser, base, "game=devtest", 128 * MIB, console, errors,
		{"width": 390, "height": 760})
	page.wait_for_function("Module.luantiStartup?.fatal", timeout=60_000)
	page.wait_for_function(
		"!document.getElementById('luanti-launch-error').hidden", timeout=10_000)
	page.wait_for_timeout(1000)  # scrollIntoView
	state = page.evaluate(STATE)
	in_view = page.evaluate("""() => {
		const r = document.getElementById('luanti-launch-error').getBoundingClientRect();
		return r.top >= 0 && r.bottom <= innerHeight;
	}""")
	page.context.close()
	assert in_view, "the startup error is below the fold on a phone-sized viewport"
	assert state["startup"]["fatal"]["code"] == "memory", state
	assert state["startup"]["memory"]["state"] == "failed", state
	assert "would not reserve shared WebAssembly memory" in state["error"], state
	assert state["playDisabled"], state
	assert state["playLabel"] == "Can't start on this browser", state
	assert state["loadingPhase"] == "Cannot start", state
	assert not state["launcher"]["runtimeReady"], state
	# The one uncaught error is startup.js ending luanti.js on purpose.
	assert len(errors) == 1 and "would not reserve shared WebAssembly memory" in errors[0], errors
	print("no ceiling granted: engine not started, launcher says:", state["error"])


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm")
	parser.add_argument("--play", action=argparse.BooleanOptionalAction, default=True,
		help="also enter a world on the reduced ceilings (default on)")
	parser.add_argument("--limits", default="1024,512",
		help="comma-separated MiB limits the fake browser grants (default 1024,512)")
	parser.add_argument("--settle", type=int, default=5,
		help="seconds to play before reading the heap")
	args = parser.parse_args()
	if not args.browser:
		parser.error("Google Chrome was not found")
	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "luanti.html").exists():
		parser.error(f"generated client not found under {output}")

	with create_server(output, "127.0.0.1", 0, quiet=True) as server:
		threading.Thread(target=server.serve_forever, daemon=True).start()
		base = f"http://127.0.0.1:{server.server_address[1]}"
		try:
			with sync_playwright() as playwright:
				browser = playwright.chromium.launch(
					headless=True, executable_path=args.browser,
					args=["--no-sandbox", "--use-gl=angle", "--use-angle=swiftshader",
						"--enable-unsafe-swiftshader"])
				try:
					build = check_default(browser, base, args)
					if build and build.get("simd"):
						check_simd_refusal(browser, base, args)
					else:
						print("SIMD refusal: skipped (not a SIMD build; test_startup.mjs covers the logic)")
					for limit in (int(value) for value in args.limits.split(",")):
						check_fallback(browser, base, args, limit)
					check_refused(browser, base, args)
				finally:
					browser.close()
		finally:
			server.shutdown()
	print("memory probe browser test passed")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
