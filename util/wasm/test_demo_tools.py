#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Unit tests for the local server and deployment packager."""

from __future__ import annotations

import http.client
import json
from pathlib import Path
import tempfile
import threading
import unittest

from build_packages import (
	PackageError, build_all, build_package, collect_media, load_config, read_index,
	stamp_service_worker)
import bench_startup
import memory_import
import size_report
from package_demo import REQUIRED_OUTPUTS, package_demo
from serve import create_server

REPO = Path(__file__).resolve().parents[2]


MEDIA_CONFIG = {"songs": {"src": "media", "url": "media/songs", "dest": "/g/songs",
	"group": "^lw_([a-z0-9_]+)_[lr]\\.ogg$", "maxSetBytes": 100}}


def make_build(root: Path, media: bool = False) -> Path:
	"""A fake generated build: shell files, two asset packages, a manifest."""
	tree = root / "tree"
	(tree / "core").mkdir(parents=True)
	(tree / "game").mkdir()
	if media:
		(tree / "media").mkdir()
		(tree / "media/lw_one_l.ogg").write_bytes(b"OggS left")
		(tree / "media/lw_one_r.ogg").write_bytes(b"OggS right")
		(tree / "media/README.md").write_text("not media")
	(tree / "core/a.txt").write_text("core a")
	(tree / "game/b.txt").write_text("game b")
	(tree / "worker.js").write_text('var BUILD_ID = "__LUANTI_BUILD_ID__";\n')
	source = root / "bin"
	source.mkdir()
	for index, name in enumerate(REQUIRED_OUTPUTS):
		(source / name).write_bytes(f"artifact {index}\n".encode())
	config = {
		"schema": 1,
		"packages": {
			"core": {"eager": True, "mounts": [{"src": "core", "dest": "/core"}]},
			"game": {"mounts": [{"src": "game", "dest": "/game"}]},
		},
		"games": {"demo": {"packages": ["game"]}},
		"shell": ["luanti.html", "luanti.js", "luanti.wasm"],
		"verified": ["luanti.wasm"],
		"serviceWorker": "worker.js",
	}
	if media:
		config["media"] = MEDIA_CONFIG
	build_all(tree, config, source, shell_dir=source)
	if media:
		# What the CMake build does: copy the media directory next to the page.
		(source / "media/songs").mkdir(parents=True)
		for path in (tree / "media").glob("*.ogg"):
			(source / "media/songs" / path.name).write_bytes(path.read_bytes())
	# The stamped worker replaces the placeholder copy in the fake build.
	return source


class DemoToolsTest(unittest.TestCase):
	def test_package_is_versioned_and_content_addressed(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = make_build(root)
			destination = root / "site"
			manifest = package_demo(source, destination, "abc123")
			stored = json.loads((destination / "version.json").read_text())
			self.assertEqual(stored, manifest)
			self.assertEqual(stored["entrypoint"], "releases/abc123/luanti.html")
			headers = (destination / "_headers").read_text()
			self.assertIn("immutable", headers)
			self.assertIn("/releases/:version/service-worker.js\n  Cache-Control: no-cache", headers)
			self.assertIn("/assets/*\n  Cache-Control: public, max-age=31536000, immutable", headers)
			self.assertIn(
				"/releases/:version/asset-manifest.json\n  Cache-Control: no-cache", headers)
			self.assertNotIn("luanti.data", headers)
			for name in REQUIRED_OUTPUTS:
				self.assertEqual(
					(source / name).read_bytes(),
					(destination / "releases" / "abc123" / name).read_bytes())

			# Packages are shared between releases, so their urls point at /assets/.
			release = json.loads(
				(destination / "releases/abc123/asset-manifest.json").read_text())
			for package in release["packages"].values():
				self.assertTrue(package["url"].startswith("../../assets/"), package["url"])
				stored_file = destination / "assets" / Path(package["url"]).name
				self.assertEqual(stored_file.stat().st_size, package["bytes"])

	def test_runtime_media_is_listed_hashed_and_published(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = make_build(root, media=True)
			built = json.loads((source / "asset-manifest.json").read_text())
			self.assertEqual(list(built["media"]), ["songs/one"])
			entry = built["media"]["songs/one"]
			self.assertEqual(entry["dest"], "/g/songs")
			self.assertEqual([f["name"] for f in entry["files"]], ["lw_one_l.ogg", "lw_one_r.ogg"])
			self.assertEqual(entry["bytes"], len(b"OggS left") + len(b"OggS right"))
			self.assertEqual(entry["files"][0]["url"], "media/songs/lw_one_l.ogg")

			package_demo(source, root / "site", "abc123")
			release = json.loads((root / "site/releases/abc123/asset-manifest.json").read_text())
			for item in release["media"]["songs/one"]["files"]:
				self.assertRegex(item["url"], r"^\.\./\.\./assets/lw_one_[lr]\.[0-9a-f]{12}\.ogg$")
				published = root / "site/assets" / Path(item["url"]).name
				self.assertEqual(published.stat().st_size, item["bytes"])
			stored = json.loads((root / "site/version.json").read_text())
			self.assertIn(Path(release["media"]["songs/one"]["files"][0]["url"]).name, stored["assets"])

			# A file that changed after the manifest was written is refused.
			(source / "media/songs/lw_one_l.ogg").write_bytes(b"OggS tampered")
			with self.assertRaises(ValueError):
				package_demo(source, root / "site2", "abc123")

	def test_runtime_media_is_checked_when_the_manifest_is_built(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			make_build(root, media=True)
			tree = root / "tree"
			config = {"media": MEDIA_CONFIG}
			(tree / "media/lw_two_l.ogg").write_bytes(b"x" * 200)
			with self.assertRaises(PackageError):  # larger than maxSetBytes
				collect_media(tree, config)
			(tree / "media/lw_two_l.ogg").unlink()
			(tree / "media/stray.ogg").write_bytes(b"x")
			with self.assertRaises(PackageError):  # not part of any set
				collect_media(tree, config)
			(tree / "media/stray.ogg").unlink()
			# The config itself: an id needs exactly one capture group.
			base = {"schema": 1, "packages": {}, "games": {}}
			path = root / "packages.json"
			path.write_text(json.dumps(dict(base, media={"songs": dict(
				MEDIA_CONFIG["songs"], group="^(a)(b)$")})))
			with self.assertRaises(PackageError):
				load_config(path)
			path.write_text(json.dumps(dict(base, media=MEDIA_CONFIG)))
			self.assertIn("songs", load_config(path)["media"])

	def test_size_report_counts_the_release_and_the_core_package(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = make_build(root)
			(source / "luanti.wasm").write_bytes(b"\0asm" + b"A" * 50_000)
			package_demo(source, root / "site", "abc123")
			report = size_report.collect(root / "site/releases/abc123", root / "site/assets",
				quality=4)
			wasm = report["files"]["luanti.wasm"]
			self.assertEqual(wasm["raw"], 50_004)
			self.assertLess(wasm["gzip"], wasm["raw"])
			if size_report.brotli:
				self.assertLess(wasm["brotli"], wasm["raw"])
			self.assertEqual(set(report["packages"]), {"core", "game"})
			self.assertTrue(report["packages"]["core"]["eager"])
			self.assertFalse(report["packages"]["game"]["eager"])
			# First visit = every shell file but the manifest, plus core.
			expected = sum(v["raw"] for k, v in report["files"].items()
				if k != "asset-manifest.json") + report["packages"]["core"]["raw"]
			self.assertEqual(report["firstVisit"]["raw"], expected)
			table = size_report.render(report)
			self.assertIn("luanti.wasm", table)
			self.assertIn("package core (core)", table)
			self.assertIn("first visit", table)
			json.dumps(report)  # the CI artifact is this, serialised

	def test_size_report_skips_only_large_optional_packages(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = make_build(root)
			package_demo(source, root / "site", "abc123")
			limit = size_report.COMPRESS_PACKAGE_LIMIT
			try:
				size_report.COMPRESS_PACKAGE_LIMIT = 1
				report = size_report.collect(root / "site/releases/abc123", root / "site/assets",
					quality=1)
			finally:
				size_report.COMPRESS_PACKAGE_LIMIT = limit
			self.assertIsNone(report["packages"]["game"]["gzip"])
			self.assertGreater(report["packages"]["game"]["raw"], 0)
			# core is what every visitor downloads: measured however large it is,
			# so the first-visit total stays a real number.
			self.assertIsNotNone(report["packages"]["core"]["gzip"])
			self.assertIsNotNone(report["firstVisit"]["gzip"])
			self.assertIn("n/a", size_report.render(report))

	def test_size_report_without_brotli_has_no_brotli_totals(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			package_demo(make_build(root), root / "site", "abc123")
			original = size_report.brotli
			size_report.brotli = None
			try:
				report = size_report.collect(root / "site/releases/abc123", root / "site/assets")
			finally:
				size_report.brotli = original
			# A column with a missing entry has no total rather than a wrong one.
			self.assertIsNone(report["firstVisit"]["brotli"])
			self.assertIsNotNone(report["firstVisit"]["gzip"])
			self.assertIsNone(report["brotliQuality"])
			self.assertIn("brotli column unavailable", size_report.render(report))

	def test_wasm_opt_trial_reports_a_missing_tool_instead_of_failing(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = make_build(root)
			package_demo(source, root / "site", "abc123")
			original = size_report.find_wasm_opt
			size_report.find_wasm_opt = lambda: None
			try:
				report = size_report.collect(root / "site/releases/abc123",
					root / "site/assets", quality=1, trial=True)
			finally:
				size_report.find_wasm_opt = original
			self.assertIn("not found", report["wasmOpt"]["error"])
			self.assertIn("wasm-opt trial: wasm-opt not found", size_report.render(report))

	def test_wasm_opt_trial_runs_the_tool_on_a_copy(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			wasm = root / "luanti.wasm"
			wasm.write_bytes(b"\0asm" + b"B" * 1000)
			tool = root / "fake-wasm-opt"
			# Stands in for Binaryen: writes a smaller module to the -o target.
			tool.write_text("#!/bin/sh\n"
				"while [ $# -gt 0 ]; do\n"
				"  if [ \"$1\" = -o ]; then out=$2; fi\n"
				"  shift\n"
				"done\n"
				"exec python3 -c 'import sys; open(sys.argv[1], \"wb\").write(bytes([0, 97, 115, 109]) + b\"C\" * 400)' \"$out\"\n")
			tool.chmod(0o755)
			results = size_report.wasm_opt_trial(wasm, str(tool), ("-O3",), False, 1)
			self.assertEqual(len(results), 1)
			self.assertTrue(results[0]["ok"], results)
			self.assertEqual(results[0]["raw"], 404)
			self.assertEqual(wasm.stat().st_size, 1004)  # the input is never touched
			failing = root / "failing-wasm-opt"
			failing.write_text("#!/bin/sh\necho 'Fatal: error validating input' >&2\nexit 1\n")
			failing.chmod(0o755)
			results = size_report.wasm_opt_trial(wasm, str(failing), ("-O3",), False, 1)
			self.assertFalse(results[0]["ok"])
			self.assertIn("error validating input", results[0]["error"])

	def test_startup_comparison_adopts_only_on_size_and_startup(self) -> None:
		def report(label: str, wasm: int, totals: list[float]) -> dict:
			summary = {name: bench_startup.median_and_spread(totals)
				for name in bench_startup.METRICS}
			return {"label": label, "wasmBytes": wasm, "jsBytes": 400_000, "summary": summary}

		base = report("default", 10_000_000, [4000, 4200, 4100])
		# Smaller and clearly faster (-2 s against a 200 ms spread): adopt.
		table, adopt = bench_startup.compare(base, report("candidate", 9_000_000, [2000, 2100, 2050]))
		self.assertTrue(adopt, table)
		self.assertIn("adopt candidate", table)
		# Smaller, but the startup change is inside the spread: noise, reject.
		table, adopt = bench_startup.compare(base, report("candidate", 9_000_000, [4050, 4150, 4000]))
		self.assertFalse(adopt, table)
		# Faster, but the binary grew: reject.
		table, adopt = bench_startup.compare(base, report("candidate", 11_000_000, [2000, 2100, 2050]))
		self.assertFalse(adopt, table)
		self.assertIn("reject candidate, keep default", table)

	def test_memory_import_is_read_from_the_binary(self) -> None:
		def leb(value: int) -> bytes:
			out = bytearray()
			while True:
				byte = value & 0x7F
				value >>= 7
				out.append(byte | (0x80 if value else 0))
				if not value:
					return bytes(out)

		def name(text: str) -> bytes:
			return leb(len(text)) + text.encode()

		imports = (
			name("env") + name("f") + b"\x00" + leb(0)                  # function, type 0
			+ name("env") + name("g") + b"\x03\x7f\x01"                 # mutable i32 global
			+ name("a") + name("a") + b"\x02\x03" + leb(4096) + leb(32769)  # shared memory
		)
		section = leb(3) + imports
		module = b"\0asm\x01\0\0\0" + b"\x02" + leb(len(section)) + section
		self.assertEqual(memory_import.memory_imports(module), [
			{"module": "a", "name": "a", "min": 4096, "max": 32769, "shared": True,
				"memory64": False}])
		# The rule startup.js relies on: provided.max <= declared max and
		# provided.min >= declared min. 1 GiB and 512 MiB (in pages) fit.
		declared = memory_import.memory_imports(module)[0]
		for ceiling in (16384, 8192):
			self.assertTrue(declared["min"] <= 4096 and ceiling <= declared["max"])
		with self.assertRaises(memory_import.WasmError):
			memory_import.memory_imports(b"not wasm at all")
		# A module that creates its own memory has nothing to report.
		self.assertEqual(memory_import.memory_imports(b"\0asm\x01\0\0\0"), [])

	def test_package_refuses_a_tampered_asset(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = make_build(root)
			victim = next((source / "assets").glob("core.*.data"))
			victim.write_bytes(victim.read_bytes()[:-1] + b"X")
			with self.assertRaises(ValueError):
				package_demo(source, root / "site", "abc123")

	def test_packages_are_deterministic_and_round_trip(self) -> None:
		config = load_config(REPO / "util/wasm/packages.json")
		first, count = build_package(REPO, config["packages"]["flower_island"])
		second, _ = build_package(REPO, config["packages"]["flower_island"])
		self.assertEqual(first, second)
		index = read_index(first)
		self.assertEqual(len(index), count)
		for entry in index:
			source = REPO / "games/flower_island" / entry["p"].removeprefix("/games/flower_island/")
			self.assertEqual(first[entry["s"]:entry["e"]], source.read_bytes())

	def test_package_config_covers_every_game_and_shell_file(self) -> None:
		config = load_config(REPO / "util/wasm/packages.json")
		# Every game offered by the launcher has a package.
		shell = (REPO / "client/web/shell.html").read_text()
		import re
		offered = set(re.findall(r'<option value="([a-z_]+)"[^>]*>', shell.split('id="luanti-game"')[1]
			.split("</select>")[0]))
		self.assertEqual(offered, set(config["games"]), "launcher games vs packages.json")
		# Songs are in no package at all: they are runtime media, fetched when
		# a listener first needs them (client/web/runtime_media.js).
		for name, spec in config["packages"].items():
			data, _ = build_package(REPO, spec)
			self.assertFalse(any("/lw_amps/songs/" in e["p"] and e["p"].endswith(".ogg")
				for e in read_index(data)), f"package {name} carries a song")
		self.assertEqual(config["games"]["luanti_web"].get("background", []), [])
		media = collect_media(REPO, config)
		songs = {name.split("/", 1)[1] for name in media if name.startswith("songs/")}
		on_disk = {p.name.removeprefix("lw_amps_").rsplit("_", 1)[0]
			for p in (REPO / "client/web/media/songs").glob("*.ogg")}
		self.assertEqual(songs, on_disk, "every song file is in the runtime media manifest")
		self.assertEqual({f["name"] for n, m in media.items() if n.startswith("songs/")
			for f in m["files"]}, {p.name for p in (REPO / "client/web/media/songs").glob("*.ogg")})
		# core is what every visitor downloads before the first frame.
		core, _ = build_package(REPO, config["packages"]["core"])
		self.assertLessEqual(len(core), 12 * 1024 * 1024, "first-visit budget (wasm_porting.md)")

	def test_service_worker_stamping(self) -> None:
		source = (REPO / "client/web/service-worker.js").read_text()
		stamped = stamp_service_worker(source, "abc")
		self.assertIn('var BUILD_ID = "abc";', stamped)
		self.assertNotIn("__LUANTI_BUILD_ID__", stamped)
		with self.assertRaises(PackageError):
			stamp_service_worker("no placeholder", "abc")

	def test_missing_asset_manifest_is_reported(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = make_build(root)
			(source / "asset-manifest.json").unlink()
			with self.assertRaises(FileNotFoundError):
				package_demo(source, root / "site", "abc123")

	def test_server_sends_isolation_headers_and_wasm_type(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "probe.wasm").write_bytes(b"\0asm")
			(root / "assets").mkdir()
			(root / "assets/core.0123456789ab.data").write_bytes(b"x")
			with create_server(root, "127.0.0.1", 0, quiet=True) as server:
				thread = threading.Thread(target=server.serve_forever, daemon=True)
				thread.start()
				try:
					connection = http.client.HTTPConnection(
						"127.0.0.1", server.server_address[1], timeout=5)
					connection.request("GET", "/probe.wasm")
					response = connection.getresponse()
					self.assertEqual(response.status, 200)
					self.assertEqual(response.getheader("Content-Type"), "application/wasm")
					self.assertEqual(
						response.getheader("Cross-Origin-Opener-Policy"), "same-origin")
					self.assertEqual(
						response.getheader("Cross-Origin-Embedder-Policy"), "require-corp")
					self.assertEqual(
						response.getheader("Cross-Origin-Resource-Policy"), "cross-origin")
					response.read()
					connection.request("GET", "/assets/core.0123456789ab.data")
					response = connection.getresponse()
					self.assertEqual(
						response.getheader("Cache-Control"),
						"public, max-age=31536000, immutable")
					response.read()
					connection.request("GET", "/probe.wasm")
					response = connection.getresponse()
					self.assertEqual(response.getheader("Cache-Control"), "no-store")
					response.read()
					connection.close()
				finally:
					server.shutdown()


if __name__ == "__main__":
	unittest.main()
