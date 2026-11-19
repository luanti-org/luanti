#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Browser check that the web client picks touch or mouse input correctly.

  * A touch-emulated session (hasTouch + isMobile) must show the engine's touch
    HUD in luanti_web, and a touch must dig or place a node.
  * A desktop session (no touch points) must stay on mouse look and pointer
    lock, with no touch HUD.

The engine logs "Touch controls enabled" when TouchControls is created, so the
log is the observable for "the HUD is on screen".
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import threading

from playwright.sync_api import BrowserContext, Page, sync_playwright

from serve import create_server
from test_first_playable import (
	CONFIG, CONFIG_TEXT, count, format_page_error, send_chat, sync_filesystem,
	wait_for_launcher, wait_for_world)

HUD_LINE = "Touch controls enabled"
GAME = "luanti_web"
PLACE_ITEM = "lw_nodes:stone_brick"


def start_world(page: Page, url: str, console: list[str]) -> None:
	page.goto(url, wait_until="domcontentloaded", timeout=120_000)
	wait_for_launcher(page)
	# Seed the config (privs for /giveme) and reload so the engine reads it.
	page.evaluate("text => FS.writeFile('%s', text)" % CONFIG, CONFIG_TEXT)
	sync_filesystem(page)
	page.reload(wait_until="domcontentloaded", timeout=120_000)
	wait_for_launcher(page)
	page.click("#luanti-play-button")
	wait_for_world(page, console)
	page.wait_for_timeout(5000)


def touch_hold(page: Page, x: int, y: int, ms: int) -> None:
	cdp = page.context.new_cdp_session(page)
	cdp.send("Input.dispatchTouchEvent",
		{"type": "touchStart", "touchPoints": [{"x": x, "y": y, "id": 1}]})
	page.wait_for_timeout(ms)
	cdp.send("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})


def touch_swipe(page: Page, x: int, y: int, dy: int) -> None:
	"""Drag one finger vertically, which turns the view (touch look)."""
	cdp = page.context.new_cdp_session(page)
	cdp.send("Input.dispatchTouchEvent",
		{"type": "touchStart", "touchPoints": [{"x": x, "y": y, "id": 2}]})
	for step in range(1, 9):
		cdp.send("Input.dispatchTouchEvent", {"type": "touchMove",
			"touchPoints": [{"x": x, "y": y + dy * step // 8, "id": 2}]})
		page.wait_for_timeout(30)
	cdp.send("Input.dispatchTouchEvent", {"type": "touchEnd", "touchPoints": []})
	page.wait_for_timeout(200)


def run_touch(context: BrowserContext, url: str, console: list[str]) -> dict:
	page = context.new_page()
	page.on("console", lambda message: console.append(message.text))
	start_world(page, url, console)
	probe = page.evaluate("Module.luantiInput")
	assert probe and probe["maxTouchPoints"] > 0 and probe["coarsePointer"], probe
	# touch_controls = "auto" for the web client means the HUD appears on the
	# first real touch even if the pointer media query lied.
	box = page.evaluate("""() => { const r = Module.canvas.getBoundingClientRect();
		return {w: r.width, h: r.height}; }""")
	cx, cy = int(box["w"] / 2), int(box["h"] / 2)
	# "auto" switches to the HUD on the first real touch, not before it.
	for _ in range(10):
		page.touchscreen.tap(cx, cy)
		page.wait_for_timeout(500)
		if any(HUD_LINE in line for line in console):
			break
	assert any(HUD_LINE in line for line in console), \
		"touch HUD was not created after a touch on a coarse-pointer device"
	start = len(console)
	# Get something to place/dig: chat needs a keyboard, which the emulated
	# device still has.
	send_chat(page, console, "/clearinv", "clears singleplayer's inventory")
	send_chat(page, console, f"/giveme {PLACE_ITEM} 40")
	start = len(console)
	acted = 0
	for attempt in range(30):
		# Look toward the ground; the swipe direction alternates so a wrong
		# guess about the drag-to-look sign cannot starve the test.
		touch_swipe(page, cx + 120, cy, 90 if attempt % 6 < 3 else -90)
		page.touchscreen.tap(cx, cy)
		page.wait_for_timeout(400)
		touch_hold(page, cx, cy, 900)
		page.wait_for_timeout(600)
		acted = count(console, start, "digs ") + count(console, start, "places node")
		if acted:
			break
	if not acted:
		print("pointer lock:", page.evaluate("!!document.pointerLockElement"))
		print("\n".join(l[:200] for l in console[start:] if "Loopback" not in l
			and "Frame clock" not in l)[-3000:])
	assert acted, "a touch never dug or placed a node"
	page.close()
	return {"acted": acted}


def run_desktop(context: BrowserContext, url: str, console: list[str]) -> dict:
	page = context.new_page()
	page.on("console", lambda message: console.append(message.text))
	start_world(page, url, console)
	probe = page.evaluate("Module.luantiInput")
	assert probe and probe["maxTouchPoints"] == 0 and not probe["coarsePointer"], probe
	page.click("#canvas", position={"x": 640, "y": 360})
	page.wait_for_function(
		"document.pointerLockElement === Module.canvas", timeout=30_000)
	assert not any(HUD_LINE in line for line in console), \
		"desktop session showed the touch HUD"
	page.close()
	return {"pointerLock": True}


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm")
	args = parser.parse_args()
	if not args.browser:
		parser.error("no browser executable was found; pass --browser")
	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "luanti.html").exists():
		parser.error(f"generated client not found under {output}")

	results = {}
	errors: list[str] = []
	with create_server(output, "127.0.0.1", 0, quiet=True) as server:
		threading.Thread(target=server.serve_forever, daemon=True).start()
		url = (f"http://127.0.0.1:{server.server_address[1]}/luanti.html"
			f"?game={GAME}&view=60")
		try:
			with sync_playwright() as playwright:
				browser = playwright.chromium.launch(
					headless=True, executable_path=args.browser,
					args=["--no-sandbox", "--use-gl=angle", "--use-angle=swiftshader"])
				for name, options, runner in (
					("desktop", {"viewport": {"width": 1280, "height": 720}},
						run_desktop),
					("touch", {"viewport": {"width": 844, "height": 390},
						"has_touch": True, "is_mobile": True,
						"device_scale_factor": 1}, run_touch)):
					console: list[str] = []
					context = browser.new_context(**options)
					context.on("page", lambda p: p.on("pageerror",
						lambda e: errors.append(format_page_error(e))))
					try:
						results[name] = runner(context, url, console)
					except BaseException:
						print(f"[{name}] console tail:\n" + "\n".join(
							line[:200] for line in console[-30:]))
						raise
					finally:
						context.close()
				browser.close()
		finally:
			server.shutdown()
	assert not errors, errors
	print("touch input check passed:", results)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
