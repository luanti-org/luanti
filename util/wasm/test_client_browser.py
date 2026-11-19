#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Load the generated Luanti client and verify its browser startup contract."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import threading

from playwright.sync_api import sync_playwright

from serve import create_server


def format_page_error(error: object) -> str:
	stack = getattr(error, "stack", None)
	return str(stack or error)


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm")
	parser.add_argument("--screenshot")
	parser.add_argument("--launcher-screenshot")
	args = parser.parse_args()
	if not args.browser:
		parser.error("Google Chrome was not found")

	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "luanti.html").exists():
		parser.error(f"generated client not found under {output}")

	with create_server(output, "127.0.0.1", 0, quiet=True) as server:
		thread = threading.Thread(target=server.serve_forever, daemon=True)
		thread.start()
		try:
			with sync_playwright() as playwright:
				browser = playwright.chromium.launch(
					headless=True, executable_path=args.browser,
					args=["--no-sandbox", "--enable-features=SharedArrayBuffer"])
				page = browser.new_page(viewport={"width": 1280, "height": 720})
				messages: list[str] = []
				page_errors: list[str] = []
				page.on("console", lambda message: messages.append(
					f"{message.type}: {message.text}"))
				page.on("pageerror", lambda error: page_errors.append(
					format_page_error(error)))
				page.goto(
					f"http://127.0.0.1:{server.server_address[1]}/luanti.html"
					"?game=devtest&view=60&name=Smoke_Player",
					wait_until="domcontentloaded", timeout=60_000)
				page.wait_for_function(
					"Module.luantiPersistence?.getState().state === 'ready'",
					timeout=60_000)
				page.wait_for_function(
					"Module.luantiLauncher?.getState().runtimeReady === true",
					timeout=60_000)
				page.wait_for_function(
					"async () => (await navigator.serviceWorker.getRegistration())?.active?.state === 'activated'",
					timeout=60_000)
				prelaunch = page.evaluate("""() => ({
					launcher: Module.luantiLauncher.getState(),
					buttonDisabled: document.getElementById('luanti-play-button').disabled,
					game: document.getElementById('luanti-game').value,
					view: document.getElementById('luanti-quality').value,
					name: document.getElementById('luanti-player-name').value
				})""")
				if args.launcher_screenshot:
					page.screenshot(path=args.launcher_screenshot)
				page.click("#luanti-play-button")
				page.wait_for_function(
					"['loading', 'playing'].includes(Module.luantiLauncher?.getState().phase)",
					timeout=90_000)
				page.evaluate("""() => {
					window.luantiResponsivenessTicks = 0;
					window.luantiResponsivenessTimer = setInterval(
						() => window.luantiResponsivenessTicks++, 20);
				}""")
				page.wait_for_timeout(1000)
				state = page.evaluate("""() => ({
					crossOriginIsolated,
					persistence: Module.luantiPersistence.getState(),
					network: Module.luantiNetwork.getState(),
					launcher: Module.luantiLauncher.getState(),
					serviceWorkerControlled: !!navigator.serviceWorker.controller,
					ticks: window.luantiResponsivenessTicks,
					canvas: {
						width: Module.canvas.width,
						height: Module.canvas.height,
						clientWidth: Module.canvas.clientWidth,
						clientHeight: Module.canvas.clientHeight
					}
				})""")
				if args.screenshot:
					page.screenshot(path=args.screenshot)
				browser.close()
		finally:
			server.shutdown()

	assert not page_errors, {"pageErrors": page_errors, "console": messages[-30:]}
	assert prelaunch["launcher"]["runtimeReady"], prelaunch
	assert not prelaunch["launcher"]["started"], prelaunch
	assert not prelaunch["buttonDisabled"], prelaunch
	assert prelaunch["game"] == "devtest", prelaunch
	assert prelaunch["view"] == "60", prelaunch
	assert prelaunch["name"] == "Smoke_Player", prelaunch
	assert state["crossOriginIsolated"], state
	# Starting a new world saves it (the launcher writes world.mt first), so a
	# healthy store may already report saving/saved.
	assert state["persistence"]["state"] in ("ready", "saving", "saved"), state
	assert not state["persistence"]["fatal"] and not state["persistence"]["error"], state
	assert state["network"]["state"] == "disabled", state
	assert state["launcher"]["started"], state
	assert state["launcher"]["phase"] in ("loading", "playing"), state
	assert state["serviceWorkerControlled"], state
	assert state["ticks"] >= 20, state
	assert state["canvas"]["width"] > 0 and state["canvas"]["height"] > 0, state
	assert state["canvas"]["width"] == state["canvas"]["clientWidth"], state
	assert state["canvas"]["height"] == state["canvas"]["clientHeight"], state
	assert abs(state["canvas"]["width"] / state["canvas"]["height"] - 1280 / 720) < 0.05, state
	assert any("application worker" in message for message in messages), {
		"state": state, "console": messages[-30:]}
	print("generated Luanti browser startup passed:", state)
	print("console tail:", messages[-30:])
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
