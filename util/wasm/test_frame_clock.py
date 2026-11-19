#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""The engine frame clock in the generated client.

The WASM client used to step the world by a constant 16 ms and never sleep.
It now uses FpsControl::limit() like the native client. Scenarios (each loads
the game into a world):

* ``clock``: the first measured ``dtime`` values are logged at actionstream,
  at least one of them is not the old constant 16 ms, and on an idle page the
  engine does not run faster than ``fps_max``.
* ``hidden``: with ``document.hidden`` true the present rate drops to
  ``fps_max_unfocused`` and the browser main thread still answers the launcher.
* ``pause``: ``Module.luantiLauncher.setPaused(true)`` stops the frame loop
  without stopping the page, and resuming does not hand the paused time to the
  simulation as one huge step.

Run after a WASM build::

	python3 util/wasm/test_frame_clock.py --browser "$(command -v google-chrome)"
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import threading

from playwright.sync_api import Browser, Page, sync_playwright

from serve import create_server
from test_webgl_context import BASE_QUERY, format_page_error

# Both caps sit below what software rendering manages on a busy machine
# (5-9 frames/s), so each one is the thing that limits the rate, not the cost
# of a frame.
FPS_MAX = 5
FPS_MAX_UNFOCUSED = 2
CONFIG = "/home/web_user/.luanti/minetest.conf"
CONFIG_TEXT = (
	"# written by util/wasm/test_frame_clock.py\n"
	f"fps_max = {FPS_MAX}\n"
	f"fps_max_unfocused = {FPS_MAX_UNFOCUSED}\n"
)
# Allow for timer granularity and a burst after a stall when counting frames.
RATE_SLACK = 1.25

CLOCK_LINE = re.compile(r"Frame clock: dtime=([0-9.eE+-]+) ms")

SET_HIDDEN = """hidden => {
	Object.defineProperty(document, "hidden", {configurable: true, get: () => hidden});
	document.dispatchEvent(new Event("visibilitychange"));
}"""


def open_game(browser: Browser, base: str, extra: str,
		console: list[str], errors: list[str]) -> Page:
	"""Start a world with the caps above in minetest.conf.

	The engine reads its configuration once, in main(); the launcher only calls
	main() when Start is pressed, so the file is written just before that.
	"""
	page = browser.new_page(viewport={"width": 1280, "height": 720})
	page.on("console", lambda m: console.append(f"{m.type}: {m.text}"))
	page.on("pageerror", lambda e: errors.append(format_page_error(e)))
	page.goto(base + "/luanti.html" + BASE_QUERY.replace("&autostart=1", "") + extra,
		wait_until="domcontentloaded", timeout=60_000)
	page.wait_for_function(
		"""() => {
			const s = Module.luantiLauncher?.getState();
			return s && s.runtimeReady && s.coreReady && !s.started
				&& !document.getElementById("luanti-play-button").disabled;
		}""", timeout=300_000)
	page.evaluate("text => FS.writeFile('%s', text)" % CONFIG, CONFIG_TEXT)
	page.click("#luanti-play-button")
	page.wait_for_function(
		"Module.luantiLauncher?.getState().phase === 'playing'", timeout=300_000)
	for _ in range(600):
		if any("World ready; entering game loop" in line for line in console):
			return page
		page.wait_for_timeout(500)
	raise AssertionError("the engine never reported a ready world")


def frames(page: Page) -> int:
	clock = page.evaluate("Module.luantiLauncher.getFrameClock()")
	assert clock is not None, "the engine exports no frame clock"
	return int(clock["frames"])


def rate(page: Page, seconds: float) -> float:
	before = frames(page)
	page.wait_for_timeout(int(seconds * 1000))
	return (frames(page) - before) / seconds


def scenario_clock(page: Page, console: list[str]) -> None:
	dtimes = [float(m.group(1)) for line in console
		if (m := CLOCK_LINE.search(line))]
	assert dtimes, f"no 'Frame clock' lines at actionstream: {console[-30:]}"
	assert any(abs(d - 16.0) > 1.0 for d in dtimes), \
		f"every logged dtime is the old constant 16 ms: {dtimes}"

	visible = rate(page, 6)
	assert visible <= FPS_MAX * RATE_SLACK, \
		f"{visible:.1f} frames/s on an idle page exceeds fps_max={FPS_MAX}"
	assert visible >= FPS_MAX * 0.5, f"{visible:.1f} frames/s: the loop is stalled"
	print(f"clock: dtimes {[round(d, 1) for d in dtimes]} ms, "
		f"{visible:.1f} frames/s (fps_max {FPS_MAX})")


def scenario_hidden(page: Page) -> None:
	# Main-thread liveness: a timer on the page keeps firing and the launcher
	# answers while the tab is hidden.
	page.evaluate("""() => {
		window.mainTicks = 0;
		setInterval(() => window.mainTicks++, 50);
	}""")
	page.evaluate(SET_HIDDEN, True)
	try:
		page.wait_for_timeout(1500)  # let the cap settle
		ticks = page.evaluate("window.mainTicks")
		hidden = rate(page, 8)
		assert hidden <= FPS_MAX_UNFOCUSED * RATE_SLACK + 0.5, \
			f"hidden tab runs at {hidden:.1f} frames/s, cap is {FPS_MAX_UNFOCUSED}"
		assert page.evaluate("window.mainTicks") - ticks >= 40, \
			"the browser main thread stalled while hidden"
		assert page.evaluate("Module.luantiLauncher.getState().phase") == "playing"
	finally:
		page.evaluate(SET_HIDDEN, False)
	page.wait_for_timeout(1500)
	shown = rate(page, 6)
	assert shown > hidden, f"visible {shown:.1f} <= hidden {hidden:.1f} frames/s"
	print(f"hidden: {hidden:.1f} frames/s hidden (cap {FPS_MAX_UNFOCUSED}), "
		f"{shown:.1f} frames/s visible")


def scenario_pause(page: Page) -> None:
	assert page.evaluate("Module.luantiLauncher.setPaused(true)") is True
	page.wait_for_timeout(500)  # the loop reaches its pause point
	stopped = frames(page)
	page.wait_for_timeout(1500)
	assert frames(page) - stopped <= 1, "frames kept coming while paused"
	assert page.evaluate("Module.luantiLauncher.getState().runtimeReady") is True

	page.evaluate("Module.luantiLauncher.setPaused(false)")
	page.wait_for_function(
		f"Module.luantiLauncher.getFrameClock().frames > {stopped + 3}",
		timeout=15_000)
	last = page.evaluate("Module.luantiLauncher.getFrameClock().lastDtimeMs")
	assert last < 1000, f"paused time leaked into dtime: {last:.0f} ms"
	print(f"pause: stopped at {stopped} frames, resumed with dtime {last:.1f} ms")


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm")
	parser.add_argument("--scenario", choices=["clock", "hidden", "pause", "all"],
		default="all")
	parser.add_argument("--render", choices=["proxy", "offscreen", "auto"],
		help="render mode (default: the build default)")
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
					args=["--no-sandbox", "--enable-features=SharedArrayBuffer",
						"--use-gl=angle", "--use-angle=swiftshader",
						"--enable-unsafe-swiftshader"])
				console: list[str] = []
				errors: list[str] = []
				extra = f"&render={args.render}" if args.render else ""
				page = open_game(browser, base, extra, console, errors)
				try:
					page.wait_for_timeout(2000)
					if args.scenario in ("clock", "all"):
						scenario_clock(page, console)
					if args.scenario in ("hidden", "all"):
						scenario_hidden(page)
					if args.scenario in ("pause", "all"):
						scenario_pause(page)
					assert not errors, {"pageErrors": errors, "console": console[-30:]}
				finally:
					page.close()
				browser.close()
		finally:
			server.shutdown()
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
