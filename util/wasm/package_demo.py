#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Package one coherent, cache-safe Luanti WASM deployment."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
from pathlib import Path
import re
import shutil

import size_report


# Per-release files. The asset packages are not here: they are
# content-addressed, shared by every release and published under /assets/.
REQUIRED_OUTPUTS = (
	"luanti.html", "luanti.js", "luanti.wasm",
	"launcher.js", "launcher.css", "launcher-config.js", "theater.js", "videotexture.js",
	"manifest.webmanifest", "service-worker.js", "luanti-web.svg",
)
ASSET_NAME_PATTERN = re.compile(r"^assets/[A-Za-z0-9_-]+\.[0-9a-f]{12}\.data$")
VERSION_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")


def digest(path: Path) -> str:
	hash_value = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			hash_value.update(chunk)
	return hash_value.hexdigest()


def package_demo(source: Path, destination: Path, version: str) -> dict[str, object]:
	if not VERSION_PATTERN.fullmatch(version):
		raise ValueError("version must contain only letters, numbers, dots, dashes, or underscores")
	if destination.exists():
		raise FileExistsError(f"destination already exists: {destination}")

	missing = [name for name in (*REQUIRED_OUTPUTS, "asset-manifest.json")
		if not (source / name).is_file()]
	if missing:
		raise FileNotFoundError(f"missing generated WASM outputs: {', '.join(missing)}")

	source_manifest = json.loads((source / "asset-manifest.json").read_text(encoding="utf-8"))
	for name, package in source_manifest["packages"].items():
		if not ASSET_NAME_PATTERN.fullmatch(package["url"]):
			raise ValueError(f"package {name} has an unexpected url: {package['url']}")
		asset = source / package["url"]
		if not asset.is_file():
			raise FileNotFoundError(f"missing asset package {asset}")
		if asset.stat().st_size != package["bytes"] or digest(asset) != package["sha256"]:
			raise ValueError(f"asset package {asset.name} does not match asset-manifest.json")

	# The service worker precaches exactly the manifest's shell, so a file it
	# lists that the release does not ship would make every install fail.
	unshipped = [name for name in source_manifest["shell"] if name not in REQUIRED_OUTPUTS]
	if unshipped:
		raise ValueError("asset-manifest.json lists shell files the release does not "
			"ship: " + ", ".join(unshipped))

	release = destination / "releases" / version
	release.mkdir(parents=True)
	files: dict[str, dict[str, object]] = {}
	for name in REQUIRED_OUTPUTS:
		target = release / name
		shutil.copy2(source / name, target)
		files[name] = {"bytes": target.stat().st_size, "sha256": digest(target)}

	# The release manifest points at the shared, immutable /assets/ directory
	# two levels up. Names are already content hashes, so unchanged packages
	# keep their URL (and their browser/service-worker cache entry) across
	# releases.
	assets = destination / "assets"
	assets.mkdir()
	for name, package in source_manifest["packages"].items():
		shutil.copy2(source / package["url"], assets / Path(package["url"]).name)
		package["url"] = "../../" + package["url"]
	manifest_text = json.dumps(source_manifest, indent="\t", sort_keys=True) + "\n"
	(release / "asset-manifest.json").write_text(manifest_text, encoding="utf-8")
	files["asset-manifest.json"] = {
		"bytes": len(manifest_text.encode("utf-8")),
		"sha256": hashlib.sha256(manifest_text.encode("utf-8")).hexdigest(),
	}

	manifest: dict[str, object] = {
		"version": version,
		"entrypoint": f"releases/{version}/luanti.html",
		"buildId": source_manifest["buildId"],
		"files": files,
		"assets": {Path(p["url"]).name: {"bytes": p["bytes"], "sha256": p["sha256"]}
			for p in source_manifest["packages"].values()},
	}
	(destination / "version.json").write_text(
		json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

	entrypoint = manifest["entrypoint"]
	quoted_entrypoint = html.escape(str(entrypoint), quote=True)
	(destination / "index.html").write_text(f"""<!doctype html>
<html lang="en">
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<meta http-equiv="refresh" content="0; url={quoted_entrypoint}">
	<title>Luanti WebAssembly</title>
</head>
<body>
	<p>Opening <a href="{quoted_entrypoint}">Luanti WebAssembly {html.escape(version)}</a>…</p>
</body>
</html>
""", encoding="utf-8")

	(destination / "404.html").write_text("""<!doctype html>
<html lang="en"><meta charset="utf-8"><title>Not found</title>
<p>This Luanti WebAssembly build was not found. Return to <a href="/">the current build</a>.</p>
""", encoding="utf-8")

	(destination / "_headers").write_text("""/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
  Cross-Origin-Resource-Policy: cross-origin
  X-Content-Type-Options: nosniff
  Referrer-Policy: no-referrer

/index.html
  Cache-Control: no-store

/version.json
  Cache-Control: no-store

/releases/:version/luanti.html
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/luanti.js
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/luanti.wasm
  Cache-Control: public, max-age=31536000, immutable

/assets/*
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/asset-manifest.json
  Cache-Control: no-cache

/releases/:version/launcher.js
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/launcher.css
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/launcher-config.js
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/theater.js
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/videotexture.js
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/manifest.webmanifest
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/luanti-web.svg
  Cache-Control: public, max-age=31536000, immutable

/releases/:version/service-worker.js
  Cache-Control: no-cache
""", encoding="utf-8")
	return manifest


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--build", default="build-wasm/bin", help="directory containing luanti.html")
	parser.add_argument("--output", default="wasm-demo", help="new deployment directory")
	parser.add_argument(
		"--version", required=True,
		help="immutable build identifier, normally a Git SHA")
	parser.add_argument(
		"--size-report", metavar="PATH",
		help="also write the size report (see size_report.py) here as JSON; CI uploads it")
	parser.add_argument(
		"--no-size-report", action="store_true",
		help="skip the raw/gzip/Brotli size table (Brotli quality 11 takes a while on luanti.wasm)")
	parser.add_argument(
		"--wasm-opt-trial", action="store_true",
		help="re-run wasm-opt on a copy of luanti.wasm and report the difference; "
		"the release is not changed")
	parser.add_argument(
		"--simd", action="store_true",
		help="the build was linked with WASM_ENABLE_SIMD (tells --wasm-opt-trial)")
	args = parser.parse_args()

	destination = Path(args.output).resolve()
	manifest = package_demo(Path(args.build).resolve(), destination, args.version)
	print(json.dumps(manifest, indent=2, sort_keys=True))
	if not args.no_size_report:
		report = size_report.collect(
			destination / "releases" / args.version, destination / "assets",
			trial=args.wasm_opt_trial, simd=args.simd)
		print("\nDownload sizes (release " + args.version + "):")
		print(size_report.render(report))
		if args.size_report:
			target = Path(args.size_report)
			target.parent.mkdir(parents=True, exist_ok=True)
			target.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
