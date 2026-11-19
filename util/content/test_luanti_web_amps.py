#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Checks for the stage amps and the songs they play (no engine required)."""

from __future__ import annotations

import pathlib
import re
import struct
import subprocess
import sys

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parents[1]
GAME = ROOT / "games/luanti_web"
AMPS = GAME / "mods/lw_amps"
# The songs are runtime media: served from client/web/media/songs/ next to the
# page and fetched when a listener first needs them
# (client/web/runtime_media.js), not part of the game or of any asset package.
SOUNDS = ROOT / "client/web/media/songs"
# What is left in the game's own songs/ folder: the README for the slot (and
# where a native install puts the files by hand).
GAME_SONGS = AMPS / "songs"
sys.path.insert(0, str(ROOT / "util/wasm"))
import build_packages  # noqa: E402

# The songs are the one large thing in the web pack. The budget in
# wasm_porting.md was raised for them; this keeps them from growing past it
# unnoticed. It is a runtime-media cap: no visitor pays it before the menu.
SONG_BUDGET = 30 * 1024 * 1024
# The inverse: what the game itself carries of them. A README or two, never a mix.
GAME_SONGS_BUDGET = 64 * 1024


def assert_true(cond: bool, message: str) -> None:
	if not cond:
		raise SystemExit(message)


def test_lua_logic() -> None:
	script = SCRIPT_DIR / "test_luanti_web_amps.lua"
	candidates = ("lua5.1", "lua", "luajit")
	last_err = None
	for binary in candidates:
		try:
			result = subprocess.run(
				[binary, str(script)],
				cwd=str(ROOT),
				check=False,
				capture_output=True,
				text=True,
			)
		except FileNotFoundError as err:
			last_err = err
			continue
		if result.returncode != 0:
			sys.stderr.write(result.stdout)
			sys.stderr.write(result.stderr)
			raise SystemExit(f"{binary} {script.name} failed")
		assert_true("stage amp checks ok" in result.stdout,
			"lua checks did not print the ok line")
		return
	raise SystemExit(f"no lua interpreter found ({last_err})")


def registered_songs() -> dict[str, float]:
	"""Song name -> length, read out of lw_amps/init.lua."""
	text = (AMPS / "init.lua").read_text(encoding="utf-8")
	songs = {name: float(length) for name, length in
		re.findall(r'\{"([a-z0-9_]+)",\s*([0-9.]+),', text)}
	assert_true(songs, "no songs found in lw_amps/init.lua")
	return songs


def vorbis_info(path: pathlib.Path) -> tuple[int, int, float]:
	"""(channels, sample rate, duration) from the Ogg/Vorbis headers."""
	data = path.read_bytes()
	assert_true(data[:4] == b"OggS", f"{path.name} is not an Ogg bitstream")
	ident = data.find(b"\x01vorbis")
	assert_true(0 <= ident < 256, f"{path.name} is not Vorbis")
	channels = data[ident + 11]
	rate = struct.unpack_from("<I", data, ident + 12)[0]
	# The last page's granule position is the sample count of the stream.
	last = data.rfind(b"OggS")
	granule = struct.unpack_from("<q", data, last + 6)[0]
	return channels, rate, granule / rate


def test_song_files() -> None:
	songs = registered_songs()
	total = 0
	expected = set()
	for name, length in songs.items():
		pair = []
		for side in ("l", "r"):
			path = SOUNDS / f"lw_amps_{name}_{side}.ogg"
			expected.add(path.name)
			assert_true(path.is_file(), f"missing {path.relative_to(ROOT)}")
			channels, rate, duration = vorbis_info(path)
			# Luanti only places a mono sound in the world; a stereo file
			# would play with no position at all.
			assert_true(channels == 1, f"{path.name} has {channels} channels, not 1")
			assert_true(rate == 44100, f"{path.name} is {rate} Hz, not 44100")
			assert_true(abs(duration - length) < 0.01,
				f"{path.name} lasts {duration:.3f}s but is registered as {length}s")
			pair.append(path.stat().st_size)
			total += path.stat().st_size
		# Not identical: the two channels are different signals. Similar,
		# though, or one side is a different cut of the song.
		assert_true(abs(pair[0] - pair[1]) <= 0.05 * max(pair),
			f"the {name} pair differs in size by more than 5%")
	extra = {p.name for p in SOUNDS.glob("*.ogg")} - expected
	assert_true(not extra, f"unregistered sound files: {sorted(extra)}")
	assert_true(total <= SONG_BUDGET,
		f"the songs are {total / 2**20:.1f} MiB; the budget is {SONG_BUDGET / 2**20:.0f} MiB")
	assert_true((SOUNDS / "README.md").is_file(), "client/web/media/songs has no README")
	# Kept out of the media every client is sent on join (see the top of
	# lw_amps/init.lua): no song may sit under a media folder of any mod.
	for folder in ("sounds", "media", "textures", "models"):
		stray = sorted(p.name for p in (GAME / "mods").glob(f"*/{folder}/**/lw_amps_*.ogg"))
		assert_true(not stray, f"song files in a {folder}/ folder are sent on join: {stray}")


def test_songs_stay_out_of_the_preload() -> None:
	"""The inverse of the song budget: nothing the game ships is a mix."""
	assert_true(GAME_SONGS.is_dir() and (GAME_SONGS / "README.md").is_file(),
		"lw_amps/songs lost its README (the slot where a native install puts the files)")
	preloaded = [p for p in GAME_SONGS.rglob("*") if p.is_file() and not p.name.startswith(".")]
	stray = sorted(p.name for p in preloaded if p.suffix != ".md")
	assert_true(not stray, f"lw_amps/songs ships audio with the game: {stray}")
	size = sum(p.stat().st_size for p in preloaded)
	assert_true(size <= GAME_SONGS_BUDGET,
		f"lw_amps/songs is {size} bytes; the game may carry {GAME_SONGS_BUDGET} of it")

	# Nothing in any asset package either (the file list, not a build).
	config = build_packages.load_config(ROOT / "util/wasm/packages.json")
	for name, spec in config["packages"].items():
		for mount in spec["mounts"]:
			for destination, _ in build_packages.collect_files(ROOT, mount):
				assert_true("/lw_amps/songs/" not in destination or destination.endswith(".md"),
					f"package {name} carries a song file: {destination}")

	# Every song is served as runtime media under the id the mod asks for.
	media = build_packages.collect_media(ROOT, config)
	for name in registered_songs():
		entry = media.get(f"songs/{name}")
		assert_true(entry is not None, f"{name} is not in the runtime media manifest")
		assert_true(sorted(f["name"] for f in entry["files"]) ==
			[f"lw_amps_{name}_l.ogg", f"lw_amps_{name}_r.ogg"],
			f"{name} is not exactly its left and right file")
		assert_true(entry["dest"] == "/games/luanti_web/mods/lw_amps/songs",
			f"{name} would be written to {entry['dest']}, not where lw_amps reads it")
	songs = {i for i in media if i.startswith("songs/")}
	assert_true(songs == {f"songs/{n}" for n in registered_songs()},
		"the runtime media manifest has songs lw_amps does not register")
	text = (AMPS / "init.lua").read_text(encoding="utf-8")
	assert_true('web_media = "songs/" .. take[1]' in text,
		"lw_amps does not ask for its songs as runtime media")
	total = sum(e["bytes"] for i, e in media.items() if i.startswith("songs/"))
	assert_true(total <= SONG_BUDGET,
		f"the songs are {total / 2**20:.1f} MiB of runtime media; the cap is {SONG_BUDGET / 2**20:.0f} MiB")


def test_routine_follows_the_songs() -> None:
	"""The Around dance map names only songs the amps can play."""
	songs = registered_songs()
	text = (GAME / "routines/lace_waterfall_around.lua").read_text(encoding="utf-8")
	block = re.search(r"songs\s*=\s*\{(.*?)\}", text, re.S)
	assert_true(block is not None, "the Around routine names no songs")
	named = re.findall(r'"([a-z0-9_]+)"', block.group(1))
	assert_true(named, "the Around routine's song list is empty")
	for name in named:
		assert_true(name in songs, f"the Around routine names {name}, which lw_amps lacks")
	assert_true("bpm = 175" in text, "the Around routine is not at 175 bpm")


def main() -> int:
	test_lua_logic()
	test_song_files()
	test_songs_stay_out_of_the_preload()
	test_routine_follows_the_songs()
	print("stage amp checks ok")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
