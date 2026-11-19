#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors

"""Browser acceptance test for the launcher's world library.

Drives the generated client through the launcher UI:

  1. two worlds are created for the same game with the New world dialog;
  2. one is played (its map generated and saved, a few nodes dug when the
     browser's mouse input allows) and gets a thumbnail;
  3. after a reload it is exported (.luantiworld download), deleted, and
     imported back from the file, and its map is byte-for-byte the same;
  4. a path-traversing and a malformed archive are refused with a readable
     error and leave storage untouched;
  5. a ?world= share link shows the archive and imports only on confirm.

Run after a WASM build:

  python3 util/wasm/test_worlds_browser.py --browser "$(command -v google-chrome)"
  python3 util/wasm/test_worlds_browser.py --engine firefox

The archive validation itself is covered in detail by test_worlds.mjs.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import tempfile
import threading
from urllib.parse import urlsplit
import zipfile

from playwright.sync_api import Page, sync_playwright

from serve import create_server
from test_first_playable import (CONFIG, DIG_ITEMS, PLACE_ITEMS, WORLDS, digest, dig_nodes,
	format_page_error, grab_pointer, send_chat, sync_filesystem, wait_for_launcher,
	wait_for_world)

GAME = "devtest"
CONFIG_TEXT = (
	"# written by util/wasm/test_worlds_browser.py\n"
	"default_privs = interact, shout, give, fly, fast\n"
)


def wait_for_library(page: Page) -> None:
	wait_for_launcher(page)
	page.wait_for_selector("#luanti-world-list[aria-busy=false]", timeout=60_000)


def world_names(page: Page) -> list[str]:
	return page.eval_on_selector_all(".world-entry", "rows => rows.map(r => r.dataset.world)")


def world_dirs(page: Page) -> list[str]:
	return page.evaluate("""dir => FS.readdir(dir).filter(n => n !== "." && n !== "..").sort()""", WORLDS)


def create_world(page: Page, name: str, seed: str) -> None:
	page.click("#luanti-new-world")
	page.wait_for_selector("#luanti-dialog[open]")
	page.fill("#luanti-new-world-name", name)
	page.fill("#luanti-new-world-seed", seed)
	page.select_option("#luanti-new-world-game", GAME)
	page.click("#luanti-dialog-confirm")
	page.wait_for_selector("#luanti-dialog", state="hidden")
	page.wait_for_selector(f'.world-entry[data-world="{name}"].is-selected')


def world_action(page: Page, name: str, action: str) -> None:
	row = f'.world-entry[data-world="{name}"]'
	page.click(f"{row} summary")
	page.click(f'{row} button[data-action="{action}"]')


def import_file(page: Page, path: Path) -> None:
	page.set_input_files("#luanti-import-file", str(path))


def run(page: Page, base: str, console: list[str], scratch: Path, engine: str,
		play: bool) -> dict:
	page.goto(f"{base}?game={GAME}&view=60", wait_until="domcontentloaded", timeout=120_000)
	wait_for_library(page)
	assert world_names(page) == [], world_names(page)
	page.evaluate("text => FS.writeFile('%s', text)" % CONFIG, CONFIG_TEXT)

	# 1. Two named worlds for one game.
	create_world(page, "Alpha", "42")
	create_world(page, "Beta", "seed words")
	assert sorted(world_names(page)) == ["Alpha", "Beta"], world_names(page)
	meta = page.evaluate("FS.readFile('%s/Alpha/map_meta.txt', {encoding: 'utf8'})" % WORLDS)
	assert meta.startswith("seed = 42\n"), meta
	# The dialog refuses a duplicate name and keeps itself open.
	page.click("#luanti-new-world")
	page.fill("#luanti-new-world-name", "Alpha")
	page.click("#luanti-dialog-confirm")
	page.wait_for_selector("#luanti-dialog-error:not([hidden])")
	assert "already exists" in page.inner_text("#luanti-dialog-error")
	page.click("#luanti-dialog-cancel")

	# 2. Play Alpha and change it (or, with --no-play, give it a synthetic map).
	page.click('.world-entry[data-world="Alpha"] .world-pick')
	dug = None
	if play:
		sync_filesystem(page)
		page.click("#luanti-play-button")
		wait_for_world(page, console)
		assert page.evaluate("Module.luantiLauncher.getState().world") == "Alpha"
		page.wait_for_timeout(5000)
		grab_pointer(page)
		send_chat(page, console, "/clearinv", "clears singleplayer's inventory")
		# dig_nodes selects hotbar slot 2, so slot 1 needs something first.
		send_chat(page, console, f"/giveme {PLACE_ITEMS[GAME]} 1")
		send_chat(page, console, f"/giveme {DIG_ITEMS[GAME]}")
		# Best effort: digging is part of the first-playable gate and is not
		# what this test is about. The round trip below compares map.sqlite byte
		# for byte, so a generated map is enough to prove it.
		dug = dig_nodes(page, console)
		thumb = f"{WORLDS}/Alpha/web_thumbnail.jpg"
		page.wait_for_function("p => FS.analyzePath(p).exists", arg=thumb, timeout=60_000)
		sync_filesystem(page)
		page.wait_for_timeout(1500)  # let the engine's own periodic save land
		sync_filesystem(page)
		before = digest(page, f"{WORLDS}/Alpha/map.sqlite")
		map_bytes = page.evaluate("p => FS.stat(p).size", f"{WORLDS}/Alpha/map.sqlite")
		assert map_bytes > 64 * 1024, f"map.sqlite has only {map_bytes} bytes; no blocks were saved"
	else:
		page.evaluate("""p => {
			const bytes = new Uint8Array(1 << 20);
			for (let i = 0; i < bytes.length; i += 65536)
				crypto.getRandomValues(bytes.subarray(i, i + 65536));
			FS.writeFile(p + "/map.sqlite", bytes);
			FS.writeFile(p + "/web_thumbnail.jpg", new Uint8Array([0xff, 0xd8, 0xff, 0xd9]));
		}""", f"{WORLDS}/Alpha")
		sync_filesystem(page)
		before = digest(page, f"{WORLDS}/Alpha/map.sqlite")
		map_bytes = 1 << 20

	# 3. Reload: export, delete, import.
	page.reload(wait_until="domcontentloaded", timeout=120_000)
	wait_for_library(page)
	assert sorted(world_names(page)) == ["Alpha", "Beta"]
	if play:
		assert page.locator('.world-entry[data-world="Alpha"] img').count() == 1, "thumbnail shown"
	assert digest(page, f"{WORLDS}/Alpha/map.sqlite") == before
	# Use the Blob download path in every browser; the save picker would
	# need a real user to answer it.
	page.evaluate("delete window.showSaveFilePicker")
	with page.expect_download(timeout=60_000) as download_info:
		world_action(page, "Alpha", "export")
	download = download_info.value
	assert download.suggested_filename == "Alpha.luantiworld", download.suggested_filename
	archive = scratch / "Alpha.luantiworld"
	download.save_as(str(archive))
	with zipfile.ZipFile(archive) as zf:
		assert zf.testzip() is None
		names = set(zf.namelist())
		assert {"luanti-world.json", "world/world.mt", "world/map.sqlite"} <= names, sorted(names)

	world_action(page, "Alpha", "delete")
	page.wait_for_selector("#luanti-dialog[open]")
	page.click("#luanti-dialog-confirm")
	page.wait_for_selector("#luanti-dialog", state="hidden")
	page.wait_for_function("() => !document.querySelector('.world-entry[data-world=\"Alpha\"]')")
	assert world_dirs(page) == ["Beta"], world_dirs(page)

	import_file(page, archive)
	page.wait_for_selector("#luanti-dialog[open]")
	assert page.input_value("#luanti-import-name") == "Alpha"
	assert "Development Test" in page.inner_text("#luanti-dialog-body")
	page.click("#luanti-dialog-confirm")
	page.wait_for_selector("#luanti-dialog", state="hidden")
	page.wait_for_selector('.world-entry[data-world="Alpha"].is-selected')
	assert digest(page, f"{WORLDS}/Alpha/map.sqlite") == before, "map changed in the round trip"

	# 4. Hostile archives are refused before anything is written.
	stored = world_dirs(page)
	evil = scratch / "evil.luantiworld"
	with zipfile.ZipFile(evil, "w") as zf:
		zf.writestr("world.mt", "gameid = devtest\n")
		zf.writestr("../../evil.lua", "print('escaped')\n")
	import_file(page, evil)
	page.wait_for_selector("#luanti-launch-error:not([hidden])")
	assert "points outside the world folder" in page.inner_text("#luanti-launch-error")
	broken = scratch / "broken.luantiworld"
	broken.write_bytes(b"this is not a zip archive" * 10)
	import_file(page, broken)
	page.wait_for_function(
		"() => document.getElementById('luanti-launch-error').textContent.includes('not a zip file')")
	assert world_dirs(page) == stored, world_dirs(page)
	assert not page.evaluate("FS.analyzePath('/home/web_user/evil.lua').exists")

	# 5. A share link shows the world and waits for confirmation.
	# Match the path only: the page URL itself ends with the same text in its query.
	page.route(lambda url: urlsplit(url).path == "/shared/alpha.luantiworld", lambda route: route.fulfill(
		status=200, body=archive.read_bytes(), headers={"Content-Type": "application/zip"}))
	page.goto(f"{base}?game={GAME}&world=/shared/alpha.luantiworld",
		wait_until="domcontentloaded", timeout=120_000)
	wait_for_library(page)
	page.wait_for_selector("#luanti-dialog[open]", timeout=60_000)
	assert page.inner_text("#luanti-dialog-title") == "Open shared world?"
	assert page.input_value("#luanti-import-name") == "Alpha (2)"
	assert world_dirs(page) == stored, "nothing is imported before confirming"
	page.click("#luanti-dialog-confirm")
	page.wait_for_selector('.world-entry[data-world="Alpha (2)"].is-selected')
	assert "world=" not in page.url, page.url
	assert digest(page, f"{WORLDS}/Alpha (2)/map.sqlite") == before

	# Everything above survives a reload.
	sync_filesystem(page)
	page.reload(wait_until="domcontentloaded", timeout=120_000)
	wait_for_library(page)
	final = sorted(world_names(page))
	assert final == ["Alpha", "Alpha (2)", "Beta"], final
	storage = page.evaluate("Module.luantiWorlds.storage()")
	return {"engine": engine, "dug": dug, "mapBytes": map_bytes, "archiveBytes": archive.stat().st_size,
		"worlds": final, "storage": storage}


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--browser", default=None,
		help="browser executable (default: Playwright's own build for --engine)")
	parser.add_argument("--engine", default="chromium", choices=("chromium", "firefox"))
	parser.add_argument("--build", default="build-wasm")
	parser.add_argument("--headed", action="store_true")
	parser.add_argument("--no-play", action="store_true",
		help="skip entering the world (hosts without WebGL); a synthetic map is exported instead")
	args = parser.parse_args()

	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "luanti.html").exists():
		parser.error(f"generated client not found under {output}")

	console: list[str] = []
	page_errors: list[str] = []
	with tempfile.TemporaryDirectory() as scratch, \
			create_server(output, "127.0.0.1", 0, quiet=True) as server:
		thread = threading.Thread(target=server.serve_forever, daemon=True)
		thread.start()
		try:
			with sync_playwright() as playwright:
				launch: dict[str, object] = {"headless": not args.headed}
				if args.browser:
					launch["executable_path"] = args.browser
				if args.engine == "chromium":
					launch["args"] = ["--no-sandbox", "--use-gl=angle", "--use-angle=swiftshader"]
				else:
					# Headless Firefox on a GPU-less host blocks WebGL unless forced.
					launch["firefox_user_prefs"] = {"webgl.force-enabled": True,
						"webgl.disabled": False}
				browser = getattr(playwright, args.engine).launch(**launch)
				# page.route() cannot see requests a service worker makes,
				# and the share link step serves its archive through a route.
				context = browser.new_context(viewport={"width": 1280, "height": 720},
					accept_downloads=True, service_workers="block")
				page = context.new_page()
				page.on("console", lambda message: console.append(message.text))
				page.on("pageerror", lambda error: page_errors.append(format_page_error(error)))
				base = f"http://127.0.0.1:{server.server_address[1]}/luanti.html"
				try:
					result = run(page, base, console, Path(scratch), args.engine,
						not args.no_play)
				except BaseException:
					print("console tail at failure:\n" + "\n".join(line[:200] for line in console[-40:]))
					raise
				finally:
					browser.close()
		finally:
			server.shutdown()

	assert not page_errors, {"pageErrors": page_errors}
	print(f"world library test passed: {result}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
