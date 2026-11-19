#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Runtime media (the concert songs' fetch path) in the generated client.

The showcase songs are not in any asset package. A mod asks the page for them
by id (``core.web_media_request``), client/web/runtime_media.js downloads the
files, and the mod sends them on with ``core.dynamic_add_media``. This runs the
real path with a few-KB fixture pair (``client/web/media/test/``, exercised by
``games/devtest/mods/web_media_probe``) and checks:

* the startup download holds no song and the browser never requests one;
* the fixture is fetched once, written where the manifest says, handed to the
  engine, and given back; the engine logs a found sound and the AudioContext
  is ``running`` after the launch click;
* hiding and showing the page does not leave a stopped context stopped, and
  the engine says so in distinct log lines (``context suspended``);
* the embedder pause stops the audio clock and unpausing restarts it;
* in the showcase, walking into the concert arena fetches exactly one song's
  pair (and nothing else), the engine finds both files and starts them, and the
  files are given back;
* a second download while one is in flight is refused (``busy``), an id the
  build does not have is ``missing`` (logged, nothing thrown), a bad id is
  ``invalid``.

Run after a WASM build::

	python3 util/wasm/test_runtime_media_browser.py --browser "$(command -v google-chrome)"
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import threading
import time

from playwright.sync_api import Browser, Page, sync_playwright

from serve import create_server
from test_first_playable import grab_pointer, send_chat
from test_webgl_context import BASE_QUERY, format_page_error

CONFIG = "/home/web_user/.luanti/minetest.conf"
CONFIG_TEXT = (
	"# written by util/wasm/test_runtime_media_browser.py\n"
	"web_media_probe = true\n"
	"default_privs = interact, shout\n"
)
FETCHED = "/games/devtest/mods/web_media_probe/fetched"
FIXTURE_FILES = ("web_media_probe_tone_l.ogg", "web_media_probe_tone_r.ogg")

SET_HIDDEN = """hidden => {
	Object.defineProperty(document, "hidden", {configurable: true, get: () => hidden});
	Object.defineProperty(document, "visibilityState", {configurable: true,
		get: () => hidden ? "hidden" : "visible"});
	document.dispatchEvent(new Event("visibilitychange"));
}"""
AUDIO_STATE = "Module.luantiAudioState()"
SUSPEND_CONTEXTS = """() => Promise.all(Object.values(AL.contexts)
	.map(c => c.audioCtx.suspend())).then(() => Module.luantiAudioState())"""


def wait_for_line(console: list[str], text: str, timeout: float, page: Page) -> str:
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		for line in console:
			if text in line:
				return line
		page.wait_for_timeout(250)
	interesting = [line for line in console
		if not ("Loopback UDP" in line or "Frame clock" in line)]
	raise AssertionError(f"never saw {text!r}; console tail: {interesting[-30:]}")


def open_game(browser: Browser, base: str, requests: list[str], console: list[str],
		errors: list[str]) -> Page:
	page = browser.new_page(viewport={"width": 1280, "height": 720})
	page.on("console", lambda m: console.append(f"{m.type}: {m.text}"))
	page.on("pageerror", lambda e: errors.append(format_page_error(e)))
	page.on("request", lambda r: requests.append(r.url))
	page.goto(base + "/luanti.html" + BASE_QUERY.replace("&autostart=1", ""),
		wait_until="domcontentloaded", timeout=60_000)
	page.wait_for_function(
		"""() => {
			const s = Module.luantiLauncher?.getState();
			return s && s.runtimeReady && s.coreReady && !s.started
				&& !document.getElementById("luanti-play-button").disabled;
		}""", timeout=300_000)
	# The engine reads its configuration once, in main(), which runs on Start.
	page.evaluate("text => FS.writeFile('%s', text)" % CONFIG, CONFIG_TEXT)
	page.click("#luanti-play-button")  # the gesture that unlocks audio
	page.wait_for_function(
		"Module.luantiLauncher?.getState().phase === 'playing'", timeout=300_000)
	return page


def scenario_startup(page: Page, requests: list[str]) -> None:
	manifest = page.evaluate("() => fetch('asset-manifest.json').then(r => r.json())")
	assert not any("song" in name for name in manifest["packages"]), \
		f"a song package is in the startup manifest: {sorted(manifest['packages'])}"
	assert "songs/around_happy_hard" in manifest["media"], "the songs are not runtime media"
	assert "test/tone" in manifest["media"], "the fixture is not in the manifest"
	package_bytes = sum(p["bytes"] for p in manifest["packages"].values())
	songs = sum(m["bytes"] for k, m in manifest["media"].items() if k.startswith("songs/"))
	assert songs > 20 * 2**20 and package_bytes < songs, \
		f"the packages ({package_bytes}) are not smaller than the songs ({songs})"
	print(f"startup: packages are {package_bytes / 2**20:.1f} MiB, "
		f"songs ({songs / 2**20:.1f} MiB) are runtime media")


def scenario_fetch(page: Page, console: list[str], requests: list[str]) -> None:
	wait_for_line(console, "web_media_probe: media delivered", 120, page)
	wait_for_line(console, "web_media_probe: playing 2 source(s)", 30, page)
	wait_for_line(console, "[media] test/tone: ready", 5, page)
	found = wait_for_line(console, 'Audio: first sound request reached OpenAL: '
		'"web_media_probe_tone_l"', 30, page)
	assert "(found)" in found, f"the engine did not find the fetched sound: {found}"
	assert "AudioContext running" in found, f"audio was not unlocked by the launch click: {found}"

	assert page.evaluate(AUDIO_STATE) == "running", page.evaluate(AUDIO_STATE)
	fixture = [u for u in requests if "/media/test/" in u]
	assert len(fixture) == len(FIXTURE_FILES), f"fixture requests: {fixture}"
	songs = [u for u in requests if "/media/songs/" in u or "lw_amps_" in u]
	assert not songs, f"a song was requested without anyone asking: {songs}"
	assert not any("songs" in u for u in requests if u.endswith(".data")), requests

	# Handed to the engine, then given back.
	wait_for_line(console, "[media] test/tone: released", 15, page)
	for name in FIXTURE_FILES:
		exists = page.evaluate("path => FS.analyzePath(path).exists", f"{FETCHED}/{name}")
		assert not exists, f"{name} is still in the filesystem after release"
	print(f"fetch: fixture fetched once ({len(fixture)} files), delivered, released; "
		f"engine found the sound, AudioContext running")


def scenario_policy(page: Page, console: list[str], requests: list[str]) -> None:
	# One download at a time, and a mod cannot name anything but a manifest id.
	result = page.evaluate("""() => {
		const media = Module.luantiRuntimeMedia;
		return {
			bad: [media.request("../x"), media.request("songs"), media.request("A/b"),
				media.request(42), media.request("songs/a/b")],
			first: media.request("test/tone"),
			second: media.request("songs/around_happy_hard"),
			same: media.request("test/tone"),
		};
	}""")
	assert result["bad"] == ["invalid"] * 5, result
	assert result["first"] == "loading", result
	assert result["second"] == "busy", f"a second download was allowed: {result}"
	assert result["same"] == "loading", result
	page.wait_for_function("Module.luantiRuntimeMedia.state('test/tone') === 'ready'", timeout=30_000)
	songs = [u for u in requests if "/media/songs/" in u]
	assert not songs, f"the refused request still downloaded: {songs}"
	assert page.evaluate("Module.luantiRuntimeMedia.release('test/tone')") is True
	assert page.evaluate("Module.luantiRuntimeMedia.release('test/tone')") is False

	# An id this build does not ship: logged, and an answer, not an exception.
	assert page.evaluate("Module.luantiRuntimeMedia.request('test/nope')") == "loading"
	page.wait_for_function("Module.luantiRuntimeMedia.state('test/nope') === 'missing'", timeout=15_000)
	wait_for_line(console, "[media] test/nope: file missing", 5, page)
	assert page.evaluate("Module.luantiRuntimeMedia.request('test/nope')") == "missing"
	print("policy: one download at a time, bad ids invalid, a missing id is logged and answered")


def scenario_visibility(page: Page, console: list[str]) -> None:
	# The browser stopped the context while the tab was hidden (headless Chrome
	# never does, so stop it the way a browser would).
	page.evaluate(SET_HIDDEN, True)
	state = page.evaluate(SUSPEND_CONTEXTS)
	assert state == "suspended", state
	wait_for_line(console, "Audio: context suspended", 20, page)
	page.evaluate(SET_HIDDEN, False)
	page.wait_for_function(f"{AUDIO_STATE} === 'running'", timeout=10_000)
	wait_for_line(console, "Audio: context resumed", 20, page)
	wait_for_line(console, "[audio] visibilitychange: resuming OpenAL context", 5, page)
	print("visibility: hiding then showing the page leaves the context running; "
		"'context suspended' and 'context resumed' are logged")


def scenario_pause(page: Page) -> None:
	assert page.evaluate("Module.luantiLauncher.setPaused(true)") is True
	page.wait_for_function(f"{AUDIO_STATE} === 'suspended'", timeout=10_000)
	# A gesture or a visibility change must not wake a paused page.
	page.evaluate(SET_HIDDEN, True)
	page.evaluate(SET_HIDDEN, False)
	page.mouse.click(10, 10)
	page.wait_for_timeout(500)
	assert page.evaluate(AUDIO_STATE) == "suspended", "the pause was undone by a click"
	page.evaluate("Module.luantiLauncher.setPaused(false)")
	page.wait_for_function(f"{AUDIO_STATE} === 'running'", timeout=10_000)
	print("pause: the audio clock stops with the engine and restarts on unpause")


SHOWCASE_CONFIG_TEXT = (
	"# written by util/wasm/test_runtime_media_browser.py\n"
	"default_privs = interact, shout, teleport, give, fly\n"
	"name = \n"
)
SONG_FILES = ("lw_amps_around_happy_hard_l.ogg", "lw_amps_around_happy_hard_r.ogg")
SONG_DIR = "/games/luanti_web/mods/lw_amps/songs"


def scenario_showcase(browser: Browser, base: str) -> None:
	"""The real thing: the showcase's concert arena plays its song after the fetch."""
	console: list[str] = []
	errors: list[str] = []
	requests: list[str] = []
	page = browser.new_page(viewport={"width": 1280, "height": 720})
	page.on("console", lambda m: console.append(f"{m.type}: {m.text}"))
	page.on("pageerror", lambda e: errors.append(format_page_error(e)))
	page.on("request", lambda r: requests.append(r.url))
	try:
		page.goto(base + "/luanti.html?game=luanti_web&view=60&name=Gl_Player",
			wait_until="domcontentloaded", timeout=60_000)
		page.wait_for_function(
			"""() => {
				const s = Module.luantiLauncher?.getState();
				return s && s.runtimeReady && s.coreReady && !s.started
					&& !document.getElementById("luanti-play-button").disabled;
			}""", timeout=300_000)
		page.evaluate("text => FS.writeFile('%s', text)" % CONFIG, SHOWCASE_CONFIG_TEXT)
		page.click("#luanti-play-button")
		page.wait_for_function(
			"Module.luantiLauncher?.getState().phase === 'playing'", timeout=300_000)
		wait_for_line(console, "World ready; entering game loop", 300, page)
		page.wait_for_timeout(6000)
		# Spawn is not the arena: nothing was fetched on the way in.
		assert not [u for u in requests if "/media/songs/" in u], \
			f"a song was requested before anyone went near a stage: {requests}"
		assert not page.evaluate("FS.analyzePath('%s/%s').exists" % (SONG_DIR, SONG_FILES[0]))

		grab_pointer(page)
		send_chat(page, console, "/maps concert_arena", "Travelling to Concert arena")
		wait_for_line(console, "[media] songs/around_happy_hard: fetching", 300, page)
		wait_for_line(console, "[media] songs/around_happy_hard: ready", 180, page)
		wait_for_line(console, "[media] songs/around_happy_hard: released", 120, page)
		found = wait_for_line(console, 'Audio: first sound request reached OpenAL: '
			'"lw_amps_around_happy_hard_', 60, page)
		assert "(found)" in found, found
		songs = [u.rsplit("/", 1)[1] for u in requests if "/media/songs/" in u or "lw_amps_" in u]
		assert sorted(songs) == sorted(SONG_FILES), \
			f"expected exactly the default mix's pair, got {songs}"
		assert page.evaluate(AUDIO_STATE) == "running", page.evaluate(AUDIO_STATE)
		for name in SONG_FILES:
			assert not page.evaluate("FS.analyzePath('%s/%s').exists" % (SONG_DIR, name)), \
				f"{name} was not given back"
		assert not errors, {"pageErrors": errors, "console": console[-30:]}
		print(f"showcase: the concert arena fetched {len(songs)} files "
			f"(and nothing else), the engine found and played the song, "
			f"AudioContext running")
	finally:
		page.close()


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm")
	parser.add_argument("--no-showcase", action="store_true",
		help="skip the concert-arena run with the real songs (the slow part)")
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
						"--enable-unsafe-swiftshader",
						"--autoplay-policy=document-user-activation-required"])
				console: list[str] = []
				errors: list[str] = []
				requests: list[str] = []
				page = open_game(browser, base, requests, console, errors)
				try:
					scenario_startup(page, requests)
					scenario_fetch(page, console, requests)
					scenario_policy(page, console, requests)
					scenario_visibility(page, console)
					scenario_pause(page)
					assert not errors, {"pageErrors": errors, "console": console[-30:]}
				finally:
					page.close()
				if not args.no_showcase:
					scenario_showcase(browser, base)
				browser.close()
		finally:
			server.shutdown()
	print("runtime media checks ok")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
