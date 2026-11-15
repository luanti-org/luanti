#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""WebGL context creation and loss handling in the generated client.

Scenarios (each loads the game into a world):

* ``lose``: after the first frames, force ``WEBGL_lose_context.loseContext()``.
  The page must show the recoverable overlay, keep the engine responsive (no
  page errors, no crash), flush persistence, and the reload button must bring
  the launcher back.
* ``webgl1``: ``?webgl=1`` forces the WebGL1 fallback; the engine must still
  reach the world and report a version-1 context.
* ``render``: the render mode actually chosen matches ``--render``, the canvas
  shows a rendered world (not a blank or single-colour frame), frames keep
  coming after the viewport is resized and the engine's backing store follows
  the new size, and pointer lock engages on a click.

``--render proxy|offscreen|auto`` runs every scenario in that render mode
(``?render=``); without it the build default is used. In offscreen mode the
context is lost through the engine (the page cannot reach it) and is never
restored, because the worker never runs the event that would allow it.

Run after a WASM build::

	python3 util/wasm/test_webgl_context.py --browser "$(command -v google-chrome)"
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import threading
import time

from playwright.sync_api import Browser, Page, sync_playwright

from serve import create_server

BASE_QUERY = "?game=devtest&view=60&name=Gl_Player&autostart=1"


def format_page_error(error: object) -> str:
	return str(getattr(error, "stack", None) or error)


def open_game(browser: Browser, base: str, extra: str,
		console: list[str], errors: list[str]) -> Page:
	page = browser.new_page(viewport={"width": 1280, "height": 720})
	page.on("console", lambda m: console.append(f"{m.type}: {m.text}"))
	page.on("pageerror", lambda e: errors.append(format_page_error(e)))
	page.goto(base + "/luanti.html" + BASE_QUERY + extra,
		wait_until="domcontentloaded", timeout=60_000)
	page.wait_for_function(
		"Module.luantiLauncher?.getState().phase === 'playing'", timeout=300_000)
	deadline = time.monotonic() + 300
	while time.monotonic() < deadline:
		if any("World ready; entering game loop" in line for line in console):
			return page
		page.wait_for_timeout(500)
	raise AssertionError("the engine never reported a ready world")


def scenario_lose(browser: Browser, base: str, extra: str) -> None:
	console: list[str] = []
	errors: list[str] = []
	page = open_game(browser, base, extra, console, errors)
	try:
		info = page.evaluate("Module.luantiWebGLInfo")
		assert info and info["version"] in (1, 2), info
		page.wait_for_timeout(2000)

		page.evaluate("""() => {
			window.glTicks = 0;
			setInterval(() => window.glTicks++, 20);
		}""")
		offscreen = info.get("render") == "offscreen"
		lost = page.evaluate("Module.luantiLauncher.debugLoseContext()")
		assert lost, "WEBGL_lose_context is unavailable"

		page.wait_for_selector("#luanti-gl-lost", state="visible", timeout=10_000)
		# Persistence flush enables the reload button.
		page.wait_for_function(
			"!document.getElementById('luanti-gl-lost-reload').disabled",
			timeout=60_000)
		assert page.evaluate("Module.luantiLauncher.getGLState().lost") == 1

		# The page must stay responsive while the context is gone.
		page.wait_for_timeout(1500)
		ticks = page.evaluate("window.glTicks")
		assert ticks >= 20, ticks

		# Restore: the overlay must stay (stale GL state) and say so. Only the
		# proxied context can be restored: offscreen, the worker never runs
		# the context-lost event whose preventDefault() would allow it.
		if not offscreen:
			page.evaluate("window.luantiLoseContextExt.restoreContext()")
			page.wait_for_function(
				"Module.luantiLauncher.getGLState().restored >= 1", timeout=10_000)
		assert page.is_visible("#luanti-gl-lost")
		assert not errors, {"pageErrors": errors, "console": console[-30:]}
		assert any("WebGL context lost" in line for line in console), console[-30:]

		# The button reloads the page. Wait for the navigation itself, or the
		# check below can still see the old page; a marker on the old window
		# proves it is gone (?autostart=1 restarts the game, so "started" says
		# nothing).
		page.evaluate("window.luantiBeforeReload = true")
		with page.expect_navigation(timeout=120_000):
			page.click("#luanti-gl-lost-reload")
		page.wait_for_function(
			"Module.luantiLauncher?.getState().runtimeReady === true",
			timeout=120_000)
		assert not page.evaluate("window.luantiBeforeReload === true")
		print("context loss: overlay, responsiveness and reload passed")
	finally:
		page.close()


def scenario_webgl1(browser: Browser, base: str, extra: str) -> None:
	console: list[str] = []
	errors: list[str] = []
	page = open_game(browser, base, "&webgl=1" + extra, console, errors)
	try:
		info = page.evaluate("Module.luantiWebGLInfo")
		assert info and info["version"] == 1, info
		page.wait_for_timeout(3000)
		assert not errors, {"pageErrors": errors, "console": console[-30:]}
		print("WebGL1 fallback passed:", info)
	finally:
		page.close()


# Distinct colours in a coarse sample of the canvas, read from a screenshot so
# it works whichever thread owns the context. A blank or uniformly cleared
# frame has one or two.
def canvas_colours(page: Page) -> int:
	png = page.locator("#canvas").screenshot()
	return page.evaluate("""async bytes => {
		const blob = new Blob([new Uint8Array(bytes)], {type: "image/png"});
		const bitmap = await createImageBitmap(blob);
		const c = new OffscreenCanvas(64, 36);
		const g = c.getContext("2d");
		g.drawImage(bitmap, 0, 0, 64, 36);
		const data = g.getImageData(0, 0, 64, 36).data;
		const seen = new Set();
		for (let i = 0; i < data.length; i += 4)
			seen.add((data[i] >> 4) << 8 | (data[i + 1] >> 4) << 4 | data[i + 2] >> 4);
		return seen.size;
	}""", list(png))


def scenario_render(browser: Browser, base: str, extra: str,
		expected: str | None, screenshots: Path | None) -> None:
	console: list[str] = []
	errors: list[str] = []
	page = open_game(browser, base, extra, console, errors)
	try:
		info = page.evaluate("Module.luantiWebGLInfo")
		assert info and info.get("render") in ("proxy", "offscreen"), info
		if expected:
			assert info["render"] == expected, info
		page.wait_for_timeout(3000)
		colours = canvas_colours(page)
		assert colours >= 8, f"canvas looks blank ({colours} colours): {info}"
		if screenshots:
			page.screenshot(path=str(screenshots / f"render-{info['render']}.png"))

		# Resize: frames keep coming and the backing store follows the box.
		page.set_viewport_size({"width": 960, "height": 600})
		page.wait_for_function("""() => {
			const c = Module.canvas;
			return Math.abs(c.width - c.clientWidth) <= 1 &&
				Math.abs(c.height - c.clientHeight) <= 1 && c.clientWidth < 1000;
		}""", timeout=30_000)
		before = page.evaluate("Module.luantiPerf.cursor()")
		page.wait_for_timeout(3000)
		after = page.evaluate("Module.luantiPerf.cursor()")
		assert after > before, f"no frames after resize ({before} -> {after})"
		size = page.evaluate("[Module.canvas.width, Module.canvas.height]")
		assert canvas_colours(page) >= 8, "canvas blank after resize"

		page.click("#canvas", position={"x": 480, "y": 300})
		page.wait_for_function(
			"document.pointerLockElement === Module.canvas", timeout=30_000)
		assert not errors, {"pageErrors": errors, "console": console[-30:]}
		print(f"render mode {info['render']}: {colours} colours, "
			f"{after - before} frames after resize to {size}, pointer lock ok")
	finally:
		page.close()


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm")
	parser.add_argument("--scenario", choices=["lose", "webgl1", "render", "all"],
		default="all")
	parser.add_argument("--screenshot-dir",
		help="save the render scenario's frame here")
	parser.add_argument("--render", choices=["proxy", "offscreen", "auto"],
		help="render mode for every scenario (default: the build default)")
	args = parser.parse_args()
	if not args.browser:
		parser.error("Google Chrome was not found")

	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "luanti.html").exists():
		parser.error(f"generated client not found under {output}")

	screenshots = Path(args.screenshot_dir) if args.screenshot_dir else None
	if screenshots:
		screenshots.mkdir(parents=True, exist_ok=True)

	with create_server(output, "127.0.0.1", 0, quiet=True) as server:
		threading.Thread(target=server.serve_forever, daemon=True).start()
		base = f"http://127.0.0.1:{server.server_address[1]}"
		try:
			with sync_playwright() as playwright:
				browser = playwright.chromium.launch(
					headless=True, executable_path=args.browser,
					args=["--no-sandbox", "--enable-features=SharedArrayBuffer",
						"--use-gl=angle", "--use-angle=swiftshader",
						"--enable-unsafe-swiftshader"])
				extra = f"&render={args.render}" if args.render else ""
				# "auto" may legitimately pick either mode.
				expected = args.render if args.render != "auto" else None
				if args.scenario in ("render", "all"):
					scenario_render(browser, base, extra, expected, screenshots)
				if args.scenario in ("lose", "all"):
					scenario_lose(browser, base, extra)
				if args.scenario in ("webgl1", "all"):
					scenario_webgl1(browser, base, extra)
				browser.close()
		finally:
			server.shutdown()
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
