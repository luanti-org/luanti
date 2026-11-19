#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Build the WASM client's streamed asset packages and asset-manifest.json.

The engine no longer preloads one monolithic luanti.data. Instead every
directory tree in packages.json becomes an immutable, content-addressed
`assets/<name>.<sha256[:12]>.data` file that client/web/packages.js downloads
(with real byte progress) and writes into the Emscripten filesystem, and
`asset-manifest.json` tells the launcher and the service worker which file is
which. See wasm_porting.md ("Content Budget") and doc/compiling/wasm.md.

Package file layout ("LWP1"):

	payload   the file bodies, concatenated in path order
	index     UTF-8 JSON: {"files": [{"p": "/abs/path", "s": start, "e": end}]}
	u32 LE    byte length of index
	"LWP1"    magic

The output is deterministic (sorted, no timestamps), so an unchanged tree
produces the same hash and therefore the same URL across releases.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys

MAGIC = b"LWP1"
HASH_LENGTH = 12
SKIP_NAMES = ("__pycache__",)
SKIP_SUFFIXES = (".pyc", ".orig", ".rej")


class PackageError(Exception):
	pass


def _skipped(name: str) -> bool:
	return name.startswith(".") or name in SKIP_NAMES or name.endswith(SKIP_SUFFIXES)


def _matches(relative: str, patterns: list[str]) -> bool:
	name = relative.rsplit("/", 1)[-1]
	return any(fnmatch.fnmatchcase(relative, pattern) or fnmatch.fnmatchcase(name, pattern)
		for pattern in patterns)


def collect_files(root: Path, mount: dict) -> list[tuple[str, Path]]:
	"""Return sorted (absolute destination path, source file) pairs."""
	source = root / mount["src"]
	if not source.is_dir():
		raise PackageError(f"package source is not a directory: {source}")
	dest = "/" + str(mount["dest"]).strip("/")
	include = list(mount.get("include", []))
	exclude = list(mount.get("exclude", []))
	found: list[tuple[str, Path]] = []
	for directory, directories, names in os.walk(source):
		directories[:] = sorted(d for d in directories if not _skipped(d))
		for name in sorted(names):
			if _skipped(name):
				continue
			path = Path(directory) / name
			if path.is_symlink() and not path.exists():
				raise PackageError(f"broken symlink in package source: {path}")
			relative = path.relative_to(source).as_posix()
			if include and not _matches(relative, include):
				continue
			if exclude and _matches(relative, exclude):
				continue
			found.append((f"{dest}/{relative}".replace("//", "/"), path))
	return found


def build_package(root: Path, spec: dict) -> tuple[bytes, int]:
	"""Return (package bytes, file count)."""
	entries: dict[str, Path] = {}
	for mount in spec["mounts"]:
		for destination, path in collect_files(root, mount):
			if destination in entries and entries[destination] != path:
				raise PackageError(f"two sources for {destination}")
			entries[destination] = path
	if not entries:
		raise PackageError("package matched no files")
	payload = bytearray()
	index = []
	for destination in sorted(entries):
		data = entries[destination].read_bytes()
		index.append({"p": destination, "s": len(payload), "e": len(payload) + len(data)})
		payload += data
	encoded = json.dumps({"files": index}, separators=(",", ":"), sort_keys=True).encode("utf-8")
	payload += encoded + struct.pack("<I", len(encoded)) + MAGIC
	return bytes(payload), len(index)


def read_index(package: bytes) -> list[dict]:
	"""Parse a package's trailer (used by tests; mirrors packages.js)."""
	if package[-4:] != MAGIC:
		raise PackageError("not an LWP1 package")
	(length,) = struct.unpack("<I", package[-8:-4])
	return json.loads(package[-8 - length:-8].decode("utf-8"))["files"]


def sha256_bytes(data: bytes) -> str:
	return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


MEDIA_NAME = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$")
MEDIA_SET = re.compile(r"^[a-z0-9_]+$")
MEDIA_ID = re.compile(r"^[a-z0-9_]{1,64}$")


def load_config(path: Path) -> dict:
	config = json.loads(path.read_text(encoding="utf-8"))
	packages = config["packages"]
	for game, entry in config["games"].items():
		for name in entry["packages"] + entry.get("background", []):
			if name not in packages:
				raise PackageError(f"game {game} needs unknown package {name}")
	for name, spec in config.get("media", {}).items():
		if not MEDIA_SET.fullmatch(name):
			raise PackageError(f"media set {name!r} needs a name of [a-z0-9_]")
		for field in ("src", "url", "dest", "group"):
			if not isinstance(spec.get(field), str) or not spec[field]:
				raise PackageError(f"media set {name} has no {field}")
		if re.compile(spec["group"]).groups != 1:
			raise PackageError(f"media set {name}: group needs exactly one capture group (the id)")
	return config


def collect_media(root: Path, config: dict) -> dict[str, dict]:
	"""Runtime media: files fetched on demand, not packaged.

	Each `media` entry of packages.json names a source directory (copied next
	to luanti.html by the build as `url`), the virtual-filesystem directory the
	page writes the files to, and a regular expression whose one capture group
	is the id of a set. Everything in a set is fetched together (a song is a
	pair of mono Oggs), and one id is what a mod asks for:
	`core.web_media_request("<set>/<id>")` (see client/web/runtime_media.js).
	"""
	media: dict[str, dict] = {}
	for set_name, spec in config.get("media", {}).items():
		source = root / spec["src"]
		if not source.is_dir():
			raise PackageError(f"media source is not a directory: {source}")
		group = re.compile(spec["group"])
		dest = "/" + str(spec["dest"]).strip("/")
		url = str(spec["url"]).strip("/")
		limit = int(spec.get("maxSetBytes", 0))
		found: dict[str, list[dict]] = {}
		for path in sorted(source.iterdir()):
			if not path.is_file() or _skipped(path.name) or path.suffix == ".md":
				continue
			match = group.match(path.name)
			if not match:
				raise PackageError(f"{path} does not belong to a {set_name} media set "
					f"(file names have to match {spec['group']})")
			identifier = match.group(1)
			if not MEDIA_ID.fullmatch(identifier) or not MEDIA_NAME.fullmatch(path.name):
				raise PackageError(f"{path}: unusable media name")
			found.setdefault(identifier, []).append({
				"name": path.name,
				"url": f"{url}/{path.name}",
				"bytes": path.stat().st_size,
				"sha256": sha256_file(path),
			})
		for identifier, files in sorted(found.items()):
			total = sum(f["bytes"] for f in files)
			if limit and total > limit:
				raise PackageError(f"media {set_name}/{identifier} is {total} bytes; "
					f"the cap for a {set_name} set is {limit}")
			media[f"{set_name}/{identifier}"] = {"dest": dest, "bytes": total, "files": files}
	return media


BUILD_ID_PLACEHOLDER = "__LUANTI_BUILD_ID__"


def stamp_service_worker(source: str, build_id: str) -> str:
	"""Return the worker with its build id filled in (exactly one placeholder)."""
	quoted = f'"{BUILD_ID_PLACEHOLDER}"'
	if source.count(quoted) != 1:
		raise PackageError("service worker must contain one quoted build id placeholder")
	return source.replace(quoted, f'"{build_id}"')


def write_if_changed(path: Path, text: str) -> None:
	if not path.is_file() or path.read_text(encoding="utf-8") != text:
		path.write_text(text, encoding="utf-8")


def build_all(root: Path, config: dict, output: Path, *, shell_dir: Path | None = None,
		prune: bool = True) -> dict:
	"""Write assets/ and asset-manifest.json under output; return the manifest."""
	assets = output / "assets"
	assets.mkdir(parents=True, exist_ok=True)
	packages: dict[str, dict] = {}
	keep: set[str] = set()
	for name, spec in config["packages"].items():
		data, count = build_package(root, spec)
		digest = sha256_bytes(data)
		filename = f"{name}.{digest[:HASH_LENGTH]}.data"
		target = assets / filename
		if not target.is_file() or target.stat().st_size != len(data):
			temporary = assets / (filename + ".tmp")
			temporary.write_bytes(data)
			temporary.replace(target)
		keep.add(filename)
		packages[name] = {
			"url": f"assets/{filename}",
			"bytes": len(data),
			"sha256": digest,
			"files": count,
			"label": spec.get("label", name),
			"eager": bool(spec.get("eager", False)),
		}
	if prune:
		for stale in assets.glob("*.data"):
			if stale.name not in keep:
				stale.unlink()
		for stale in assets.glob("*.tmp"):
			stale.unlink()

	shell: dict[str, dict] = {}
	if shell_dir is not None:
		verified = set(config.get("verified", []))
		for name in config.get("shell", []):
			path = shell_dir / name
			if not path.is_file():
				raise PackageError(f"missing shell file {path}")
			shell[name] = {"bytes": path.stat().st_size, "sha256": sha256_file(path),
				"verify": name in verified}
		for name in config.get("shellOptional", []):
			path = shell_dir / name
			if path.is_file():
				shell[name] = {"bytes": path.stat().st_size, "sha256": sha256_file(path),
					"verify": name in verified}

	# The build id names the shell (page, engine, launcher): it is what the
	# service worker is stamped with. Packages are content-addressed on their
	# own and deliberately do not change it.
	identity = json.dumps({
		"shell": {k: v["sha256"] for k, v in sorted(shell.items())},
		"packages": {} if shell else {k: v["sha256"] for k, v in sorted(packages.items())},
	}, sort_keys=True).encode("utf-8")
	manifest = {
		"schema": config["schema"],
		"buildId": sha256_bytes(identity)[:16],
		"packages": packages,
		"games": config["games"],
		"media": collect_media(root, config),
		"shell": shell,
	}
	text = json.dumps(manifest, indent="\t", sort_keys=True) + "\n"
	write_if_changed(output / "asset-manifest.json", text)
	worker = config.get("serviceWorker")
	if worker and shell:
		write_if_changed(output / "service-worker.js",
			stamp_service_worker((root / worker).read_text(encoding="utf-8"), manifest["buildId"]))
	return manifest


def main() -> int:
	here = Path(__file__).resolve().parent
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--root", default=str(here.parents[1]), help="source tree root")
	parser.add_argument("--config", default=str(here / "packages.json"))
	parser.add_argument("--output", default="build-wasm/bin",
		help="directory receiving assets/ and asset-manifest.json")
	parser.add_argument("--shell-dir", default=None,
		help="directory holding the generated luanti.* files to list in the manifest "
		"(default: --output; pass '' to skip)")
	parser.add_argument("--no-prune", action="store_true", help="keep stale hashed packages")
	args = parser.parse_args()

	output = Path(args.output).resolve()
	shell_dir = output if args.shell_dir is None else (Path(args.shell_dir) if args.shell_dir else None)
	try:
		manifest = build_all(Path(args.root).resolve(), load_config(Path(args.config)), output,
			shell_dir=shell_dir, prune=not args.no_prune)
	except PackageError as error:
		print(f"build_packages: {error}", file=sys.stderr)
		return 1
	total = sum(p["bytes"] for p in manifest["packages"].values())
	for name, package in manifest["packages"].items():
		print(f"  {name:24s} {package['bytes'] / 1048576:8.2f} MiB  {package['files']:5d} files  {package['url']}")
	print(f"  {'total':24s} {total / 1048576:8.2f} MiB  build {manifest['buildId']}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
