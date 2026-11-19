#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Measure what a packaged Luanti WASM release costs to download.

package_demo.py calls this after packaging and prints the table into the CI
log; it can also be run on its own against a packaged release::

	python3 util/wasm/size_report.py --release wasm-demo/releases/<version> \\
		--assets wasm-demo/assets --json wasm-size/size-report.json

Every row has the raw size and what a good static host sends over the wire
(gzip -9 and Brotli quality 11). Hosts such as Cloudflare Pages compress for
you at a lower Brotli quality, so the Brotli column is the best case for the
file, not a promise. The budget that fails a build is a separate check; this
module only produces the bytes it would compare (wasm_porting.md, "Content
Budget").

--wasm-opt-trial re-runs Binaryen on a copy of luanti.wasm and reports the
size difference. It never changes the release: the service worker verifies
luanti.wasm against asset-manifest.json, so shipping a different binary would
mean regenerating the manifest and the worker. Whether it is worth that is
what the trial numbers are for.
"""

from __future__ import annotations

import argparse
import gzip
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

try:
	import brotli  # type: ignore[import-not-found]
except ImportError:  # the report degrades to raw + gzip
	brotli = None

# The engine files every visitor pays for before the first frame.
ENGINE_FILES = ("luanti.wasm", "luanti.js")
# Optional packages larger than this are streamed content (songs are already
# Ogg): their raw size is reported, but compressing them says nothing. The
# eager `core` package is always compressed.
COMPRESS_PACKAGE_LIMIT = 4 * 1024 * 1024
# Binaryen needs the features the Emscripten link enabled; the final binary
# no longer carries a target_features section to read them from (emcc runs
# wasm-opt --strip-target-features). These are the flags of emcc's own
# wasm-opt step, copied from a verbose Release link.
WASM_OPT_FEATURES = (
	"--mvp-features", "--enable-threads", "--enable-bulk-memory", "--enable-multivalue",
	"--enable-mutable-globals", "--enable-sign-ext",
)


def human(size: int | None) -> str:
	if size is None:
		return "n/a"
	return f"{size / 1048576:.2f} MiB" if size >= 1048576 else f"{size / 1024:.1f} KiB"


def compress(data: bytes, quality: int) -> dict[str, int | None]:
	return {
		"raw": len(data),
		"gzip": len(gzip.compress(data, compresslevel=9, mtime=0)),
		"brotli": len(brotli.compress(data, quality=quality)) if brotli else None,
	}


def find_wasm_opt() -> str | None:
	"""wasm-opt from PATH, else the active Emscripten SDK's own copy."""
	found = shutil.which("wasm-opt")
	if found:
		return found
	for variable in ("EMSDK", "EMSDK_DIR"):
		root = os.environ.get(variable)
		if root:
			candidate = Path(root) / "upstream" / "bin" / "wasm-opt"
			if candidate.is_file():
				return str(candidate)
	return None


def wasm_opt_trial(wasm: Path, tool: str, levels: tuple[str, ...], simd: bool,
		quality: int) -> list[dict]:
	"""Re-optimise a copy of `wasm` at each level; return what each produced."""
	results = []
	features = WASM_OPT_FEATURES + (("--enable-simd",) if simd else ())
	with tempfile.TemporaryDirectory() as temporary:
		for level in levels:
			target = Path(temporary) / f"luanti{level}.wasm"
			started = time.monotonic()
			run = subprocess.run([tool, level, *features, str(wasm), "-o", str(target)],
				capture_output=True, text=True)
			seconds = time.monotonic() - started
			if run.returncode != 0 or not target.is_file():
				results.append({"level": level, "ok": False, "seconds": round(seconds, 1),
					"error": (run.stderr or run.stdout).strip().splitlines()[-1][:300]
						if (run.stderr or run.stdout).strip() else f"exit {run.returncode}"})
				continue
			results.append({"level": level, "ok": True, "seconds": round(seconds, 1),
				**compress(target.read_bytes(), quality)})
	return results


def collect(release: Path, assets: Path, quality: int = 11, trial: bool = False,
		simd: bool = False, levels: tuple[str, ...] = ("-O3", "-Os")) -> dict:
	"""Return the size report for a packaged release as plain data."""
	files: dict[str, dict] = {}
	for path in sorted(release.iterdir()):
		if path.is_file():
			files[path.name] = compress(path.read_bytes(), quality)
	packages: dict[str, dict] = {}
	manifest_path = release / "asset-manifest.json"
	if manifest_path.is_file():
		manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
		for name, package in manifest["packages"].items():
			path = (release / package["url"]).resolve()
			if not path.is_file():
				path = assets / Path(package["url"]).name
			size = path.stat().st_size
			eager = bool(package.get("eager"))
			# `core` is what every visitor downloads, so it is always measured
			# whatever its size; only large optional packages are skipped.
			packages[name] = compress(path.read_bytes(), quality) \
				if eager or size <= COMPRESS_PACKAGE_LIMIT \
				else {"raw": size, "gzip": None, "brotli": None}
			packages[name]["eager"] = eager

	def total(rows: list[dict], key: str) -> int | None:
		values = [row[key] for row in rows]
		return None if any(value is None for value in values) else sum(values)

	first_visit = [files[name] for name in files if name != "asset-manifest.json"]
	if "core" in packages:
		first_visit.append(packages["core"])
	report: dict = {
		"brotliQuality": quality if brotli else None,
		"files": files,
		"packages": packages,
		"firstVisit": {key: total(first_visit, key) for key in ("raw", "gzip", "brotli")},
	}
	if trial:
		tool = find_wasm_opt()
		wasm = release / "luanti.wasm"
		if tool is None:
			report["wasmOpt"] = {"error": "wasm-opt not found (looked on PATH and under $EMSDK/upstream/bin)"}
		elif not wasm.is_file():
			report["wasmOpt"] = {"error": "luanti.wasm is not in the release"}
		else:
			report["wasmOpt"] = {"tool": tool, "levels": wasm_opt_trial(wasm, tool, levels, simd, quality)}
	return report


def percent(before: int | None, after: int | None) -> str:
	if not before or after is None:
		return ""
	return f"{(after - before) / before * 100:+.1f}%"


def render(report: dict) -> str:
	lines = []
	lines.append(f"{'':34s}{'raw':>12s}{'gzip -9':>12s}{'brotli ' + str(report['brotliQuality'] or '-'):>12s}")

	def row(label: str, sizes: dict) -> None:
		lines.append(f"{label:34s}{human(sizes['raw']):>12s}{human(sizes['gzip']):>12s}"
			f"{human(sizes['brotli']):>12s}")

	for name in ENGINE_FILES:
		if name in report["files"]:
			row(name, report["files"][name])
	for name, sizes in report["files"].items():
		if name not in ENGINE_FILES and name != "asset-manifest.json":
			row(name, sizes)
	for name, sizes in report["packages"].items():
		row(f"package {name}" + (" (core)" if sizes.get("eager") else ""), sizes)
	row("first visit (shell + core)", report["firstVisit"])
	opt = report.get("wasmOpt")
	if opt:
		lines.append("")
		if "error" in opt:
			lines.append(f"wasm-opt trial: {opt['error']}")
		else:
			base = report["files"].get("luanti.wasm", {})
			lines.append(f"wasm-opt trial on a copy of luanti.wasm ({opt['tool']}); the release is unchanged:")
			for result in opt["levels"]:
				if not result["ok"]:
					lines.append(f"  {result['level']:4s} failed: {result['error']}")
					continue
				row(f"  luanti.wasm after {result['level']}", result)
				lines.append(f"{'':34s}{percent(base.get('raw'), result['raw']):>12s}"
					f"{percent(base.get('gzip'), result['gzip']):>12s}"
					f"{percent(base.get('brotli'), result['brotli']):>12s}   ({result['seconds']} s)")
	if report["brotliQuality"] is None:
		lines.append("")
		lines.append("brotli column unavailable: python3 -m pip install brotli")
	return "\n".join(lines)


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--release", help="a packaged releases/<version> directory")
	parser.add_argument("--assets", help="the packaged assets/ directory")
	parser.add_argument("--json", help="also write the report here")
	parser.add_argument("--from-json", metavar="PATH",
		help="print the table for a report written earlier instead of measuring")
	parser.add_argument("--brotli-quality", type=int, default=11)
	parser.add_argument("--wasm-opt-trial", action="store_true")
	parser.add_argument("--simd", action="store_true", help="the build was linked with -msimd128")
	args = parser.parse_args()
	if args.from_json:
		print(render(json.loads(Path(args.from_json).read_text(encoding="utf-8"))))
		return 0
	if not args.release or not args.assets:
		parser.error("--release and --assets are required unless --from-json is given")
	report = collect(Path(args.release), Path(args.assets), args.brotli_quality,
		args.wasm_opt_trial, args.simd)
	print(render(report))
	if args.json:
		target = Path(args.json)
		target.parent.mkdir(parents=True, exist_ok=True)
		target.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
	return 0


if __name__ == "__main__":
	sys.exit(main())
