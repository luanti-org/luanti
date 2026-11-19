#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Streamed asset packages, end to end, in a real browser and a real engine.

Packages the generated build exactly as a deployment would
(util/wasm/package_demo.py), serves the versioned layout and checks that:

* a first visit downloads only `core`, and the byte count is under budget;
* choosing a game downloads only that game's package, and the engine starts;
* after that, with the network switched off, the same game starts from the
  service-worker cache, while a game that was never downloaded fails cleanly;
* the showcase songs are not downloaded at all until a mod asks for them.

Run after a WASM build:

	python3 util/wasm/test_packages_browser.py --browser "$(command -v google-chrome)"
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import tempfile
import threading
import time
from urllib.parse import urlparse

from playwright.sync_api import BrowserContext, Page, sync_playwright

from package_demo import package_demo
from serve import create_server

VERSION = "pkgtest"
FIRST_VISIT_BUDGET = 12 * 1024 * 1024


def asset_name(url: str) -> str | None:
	path = urlparse(url).path
	if "/assets/" not in path or not path.endswith(".data"):
		return None
	return path.rsplit("/", 1)[1].rsplit(".", 2)[0]


class Recorder:
	"""Which asset packages the *page* asked the network or cache for."""

	def __init__(self, context: BrowserContext) -> None:
		self.requested: list[str] = []
		self.failed: list[str] = []
		context.on("request", self._request)
		context.on("requestfailed", self._failed)

	def _request(self, request) -> None:
		name = asset_name(request.url)
		if not name:
			return
		try:
			request.frame
		except Exception:
			# A fetch made by the service worker itself (its own precache or the
			# network leg of a page request): not a page request.
			return
		self.requested.append(name)

	def _failed(self, request) -> None:
		name = asset_name(request.url)
		if name:
			self.failed.append(name)

	def mark(self) -> int:
		return len(self.requested)

	def since(self, mark: int) -> set[str]:
		return set(self.requested[mark:])


def launcher_ready(page: Page) -> None:
	page.wait_for_function(
		"Module.luantiLauncher?.getState().coreReady === true "
		"&& Module.luantiLauncher.getState().runtimeReady === true "
		"&& Module.luantiPersistence?.getState().state === 'ready'",
		timeout=120_000)
	page.wait_for_function(
		"navigator.serviceWorker.controller !== null || "
		"navigator.serviceWorker.getRegistration().then(r => r?.active?.state === 'activated')",
		timeout=120_000)


def packages(page: Page) -> dict:
	return page.evaluate("Module.luantiLauncher.getState().packages")


def choose_and_play(page: Page, game: str) -> None:
	page.select_option("#luanti-game", game)
	page.click("#luanti-play-button")


def wait_for_world(page: Page) -> None:
	page.wait_for_function(
		"['loading', 'playing'].includes(Module.luantiLauncher?.getState().phase) "
		"&& Module.luantiLauncher.getState().started",
		timeout=180_000)


def run(playwright, args, site: Path, port: int) -> None:
	browser = playwright.chromium.launch(
		headless=True, executable_path=args.browser,
		args=["--no-sandbox", "--enable-features=SharedArrayBuffer"])
	context = browser.new_context(viewport={"width": 1280, "height": 720})
	recorder = Recorder(context)
	url = f"http://127.0.0.1:{port}/releases/{VERSION}/luanti.html?view=60&name=Pkg_Player"
	errors: list[str] = []

	def new_page() -> Page:
		page = context.new_page()
		page.on("pageerror", lambda error: errors.append(str(error)))
		return page

	manifest = json.loads((site / "releases" / VERSION / "asset-manifest.json").read_text())
	sizes = {name: p["bytes"] for name, p in manifest["packages"].items()}
	try:
		# 1. First visit: engine data only.
		page = new_page()
		page.goto(url, wait_until="domcontentloaded", timeout=120_000)
		launcher_ready(page)
		assert recorder.since(0) == {"core"}, recorder.requested
		assert sizes["core"] <= FIRST_VISIT_BUDGET, sizes["core"]
		state = packages(page)
		assert set(state) == {"core"} and state["core"]["state"] == "loaded", state
		print(f"first visit downloads only core: {sizes['core'] / 1048576:.1f} MiB")

		# 2. A game: only its own package, then the engine starts.
		mark = recorder.mark()
		choose_and_play(page, "flower_island")
		wait_for_world(page)
		assert recorder.since(mark) == {"flower_island"}, recorder.requested[mark:]
		assert packages(page)["flower_island"]["state"] == "loaded"
		page.close()

		# 3. Offline: the cached game starts without the network.
		context.set_offline(True)
		page = new_page()
		page.goto(url, wait_until="domcontentloaded", timeout=120_000)
		launcher_ready(page)
		choose_and_play(page, "flower_island")
		wait_for_world(page)
		assert not recorder.failed, f"asset requests failed offline: {recorder.failed}"
		page.close()

		# 4. Offline, a game that was never downloaded fails cleanly and the
		# launcher comes back instead of hanging on a black canvas.
		page = new_page()
		page.goto(url, wait_until="domcontentloaded", timeout=120_000)
		launcher_ready(page)
		choose_and_play(page, "devtest")
		page.wait_for_function(
			"document.getElementById('luanti-launch-error').hidden === false",
			timeout=60_000)
		message = page.inner_text("#luanti-launch-error")
		assert "Could not download Development Test" in message, message
		assert page.evaluate("document.getElementById('luanti-launcher').hidden") is False
		assert not page.evaluate("Module.luantiLauncher.getState().started")
		print(f"uncached game offline: {message!r}")
		page.close()
		context.set_offline(False)

		# 5. The showcase: the songs are neither in its package nor streamed in
		# the background. Nobody walks up to the concert arena here, so no song
		# is requested at all (test_runtime_media_browser.py covers the fetch).
		page = new_page()
		page.goto(url, wait_until="domcontentloaded", timeout=120_000)
		launcher_ready(page)
		mark = recorder.mark()
		song_requests: list[str] = []
		page.on("request", lambda r: song_requests.append(r.url)
			if "/lw_amps_" in r.url or "/media/songs/" in r.url else None)
		choose_and_play(page, "luanti_web")
		page.wait_for_function(
			"Module.luantiLauncher.getState().phase === 'playing'", timeout=240_000)
		page.wait_for_timeout(5_000)
		order = [name for name in recorder.requested[mark:]]
		assert order[:1] == ["luanti_web"] and "devtest" not in order, order
		assert not any("song" in name for name in order), order
		assert not song_requests, f"a song was downloaded without anyone asking: {song_requests}"
		loaded = page.evaluate("Object.keys(Module.luantiLauncher.getState().packages)")
		assert not any("song" in name for name in loaded), loaded
		songs = page.evaluate(
			"FS.analyzePath('/games/luanti_web/mods/lw_amps/songs/lw_amps_around_happy_hard_l.ogg').exists")
		assert not songs, "a song is in the filesystem before anything asked for it"
		page.close()

		assert not errors, errors
	finally:
		context.close()
		browser.close()


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm")
	args = parser.parse_args()
	if not args.browser:
		parser.error("Google Chrome was not found")
	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "asset-manifest.json").exists():
		parser.error(f"generated client not found under {output}")

	with tempfile.TemporaryDirectory(prefix="luanti-pkg-test-") as temporary:
		site = Path(temporary) / "site"
		package_demo(output, site, VERSION)
		with create_server(site, "127.0.0.1", 0, quiet=True) as server:
			threading.Thread(target=server.serve_forever, daemon=True).start()
			started = time.monotonic()
			try:
				with sync_playwright() as playwright:
					run(playwright, args, site, server.server_address[1])
			finally:
				server.shutdown()
	print(f"streamed package tests passed in {time.monotonic() - started:.0f}s")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
