#!/usr/bin/env python3
"""
deploy_1ink.py — luanti_wasm

Deploy the Emscripten client to https://test.1ink.us/luanti via
storage.noahcohn.com. No SFTP credentials are stored in this repo.

Usage:
  1. Build the WASM client (see below)
  2. python3 util/wasm/deploy_1ink.py
     # or: DEPLOY_TOKEN=... python3 util/wasm/deploy_1ink.py

Build (Emscripten latest):
  source /root/emsdk/emsdk_env.sh
  cmake --preset Emscripten -B build-wasm
  cmake --build build-wasm --parallel "$(($(nproc) + 1))"

Requirements:
  pip install requests
"""

from __future__ import annotations

import io
import json
import os
import shutil
import sys
import tempfile
import time
import zipfile
from pathlib import Path
from typing import Optional

import requests

# ============================================================
# PER-PROJECT CONFIGURATION
# ============================================================
PROJECT_NAME: str = "luanti"
# Flat Emscripten output (html/js/wasm/data + launcher assets)
BUILD_DIR: str = os.getenv("BUILD_DIR", "build-wasm/bin")
CONTABO_BASE_URL: str = "https://storage.noahcohn.com"

# Lands at /home/ford442/test.1ink.us/luanti → https://test.1ink.us/luanti/
TARGET_FOLDER: str = os.getenv("TARGET_FOLDER", "luanti")

# Set via environment: export DEPLOY_TOKEN="your_long_token_from_vps_env"
DEPLOY_TOKEN: Optional[str] = os.getenv("DEPLOY_TOKEN")

# Default test site; set DEPLOY_TARGET=go to deploy under go.1ink.us instead.
DEPLOY_TARGET: str = os.getenv("DEPLOY_TARGET", "test")

DEPLOY_MAX_RETRIES: int = int(os.getenv("DEPLOY_MAX_RETRIES", "3"))
DEPLOY_TIMEOUT: int = int(os.getenv("DEPLOY_TIMEOUT", "900"))

# Required engine + launcher files produced by the Emscripten preset.
REQUIRED_FILES = (
	"luanti.html",
	"luanti.js",
	"luanti.wasm",
	"asset-manifest.json",
	"launcher.js",
	"launcher.css",
	"launcher-config.js",
	"theater.js",
	"videotexture.js",
	"manifest.webmanifest",
	"service-worker.js",
	"luanti-web.svg",
)

# Optional companions (present on pthread builds).
OPTIONAL_FILES = (
	"luanti.worker.js",
)

# Host .htaccess lives in-repo. Deploy must not invent a second COOP header
# (test.1ink.us already sends one; duplicates break SharedArrayBuffer).
HTACCESS_SOURCE = Path(__file__).resolve().parents[2] / "client" / "web" / ".htaccess"
# ============================================================


def repo_root() -> Path:
	return Path(__file__).resolve().parents[2]


def resolve_build_dir() -> Path:
	path = Path(BUILD_DIR)
	if not path.is_absolute():
		path = repo_root() / path
	return path


def stage_deploy_tree(source: Path) -> Path:
	"""Copy build outputs into a clean staging dir with index.html + .htaccess."""
	missing = [name for name in REQUIRED_FILES if not (source / name).is_file()]
	if missing:
		raise FileNotFoundError(
			"missing generated WASM outputs: " + ", ".join(missing)
			+ f"\nBuild first, expected under {source}"
		)

	staging = Path(tempfile.mkdtemp(prefix="luanti-deploy-"))
	try:
		for name in REQUIRED_FILES:
			shutil.copy2(source / name, staging / name)
		for name in OPTIONAL_FILES:
			src = source / name
			if src.is_file():
				shutil.copy2(src, staging / name)
		# Content-addressed asset packages named by asset-manifest.json.
		manifest = json.loads((source / "asset-manifest.json").read_text(encoding="utf-8"))
		(staging / "assets").mkdir()
		for package in manifest["packages"].values():
			package_file = source / package["url"]
			if not package_file.is_file():
				raise FileNotFoundError(f"missing asset package {package_file}")
			shutil.copy2(package_file, staging / package["url"])
		# Runtime media (songs) is fetched by the page on demand, from the
		# URLs in the manifest's "media" section.
		for entry in manifest.get("media", {}).values():
			for item in entry["files"]:
				media_file = source / item["url"]
				if not media_file.is_file():
					raise FileNotFoundError(f"missing runtime media {media_file}")
				(staging / item["url"]).parent.mkdir(parents=True, exist_ok=True)
				shutil.copy2(media_file, staging / item["url"])

		# UTF-8 launcher as index.html. Do not publish it as 1ink.1ink — that
		# extension is served as charset=UTF-16 on this host.
		shutil.copy2(source / "luanti.html", staging / "index.html")
		htaccess = HTACCESS_SOURCE if HTACCESS_SOURCE.is_file() else source / ".htaccess"
		if not htaccess.is_file():
			raise FileNotFoundError(
				"missing client/web/.htaccess (required so deploy does not "
				"overwrite the test.1ink.us isolation headers)"
			)
		shutil.copy2(htaccess, staging / ".htaccess")
	except Exception:
		shutil.rmtree(staging, ignore_errors=True)
		raise
	return staging



def fetch_remote_sizes(target_folder, target_site="test"):
    """Ask the VPS for {rel_path: bytes} already on the deploy target."""
    base = CONTABO_BASE_URL.rstrip("/")
    url = f"{base}/api/deploy/{PROJECT_NAME}/sizes"
    headers = {}
    token = globals().get("DEPLOY_TOKEN")
    if token:
        headers["X-Deploy-Token"] = token
    params = {"target_site": target_site or "test"}
    if target_folder:
        params["target_folder"] = target_folder
    try:
        response = requests.get(url, params=params, headers=headers, timeout=60)
        if response.status_code == 200:
            files = response.json().get("files") or {}
            print(f"Remote size map: {len(files)} file(s)")
            return {str(k).replace("\\", "/"): int(v) for k, v in files.items()}
        print(f"  ! sizes HTTP {response.status_code}; uploading all files")
    except Exception as exc:
        print(f"  ! Could not fetch remote sizes ({exc}); uploading all files")
    return {}


def build_zip(build_path: Path, skip_sizes=None) -> bytes:
	"""Zip the contents of build_path into an in-memory archive."""
	buf = io.BytesIO()
	with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
		for file in sorted(build_path.rglob("*")):
			if file.is_dir():
				continue
			if file.is_symlink():
				continue
			rel = file.relative_to(build_path)
			parts = rel.parts
			if any(p in (".git", "node_modules", "__pycache__") for p in parts):
				continue
			rel_s = str(rel).replace("\\", "/")
			local_size = file.stat().st_size
			if (skip_sizes or {}).get(rel_s) == local_size:
				print(f"  = {rel} ({local_size} bytes, unchanged)")
				continue
			zf.write(file, rel_s)
			print(f"  + {rel}")
	return buf.getvalue()


def _print_partial_failures(data: dict) -> None:
	print(f"  ✓ {data.get('uploaded', 0)} files uploaded")
	if data.get("failed"):
		print("  Failures:")
		for f in data["failed"]:
			print(f"    ✗ {f['path']}: {f['error']}")


def deploy_bundle(build_path: Path) -> bool:
	"""Zip the build and upload it as a single archive."""
	url = f"{CONTABO_BASE_URL}/api/deploy/{PROJECT_NAME}/zip"
	headers = {}
	if DEPLOY_TOKEN:
		headers["X-Deploy-Token"] = DEPLOY_TOKEN

	form_data = {"target_site": DEPLOY_TARGET}
	target_folder = TARGET_FOLDER.strip()
	if target_folder:
		form_data["target_folder"] = target_folder

	print("Building zip archive...")
	target_folder_for_sizes = globals().get("DEPLOY_FOLDER") or globals().get("TARGET_FOLDER") or PROJECT_NAME
	if "target_folder" in locals() and target_folder:
		target_folder_for_sizes = target_folder
	target_site_for_sizes = globals().get("DEPLOY_TARGET", "test")
	force = os.getenv("FORCE_DEPLOY", "").strip().lower() in ("1", "true", "yes")
	if force:
		print("FORCE_DEPLOY set — uploading all files (ignoring remote sizes).")
		form_data["force"] = "1"
		url = f"{url}?force_upload=1"
		skip_sizes = {}
	else:
		print("Checking remote file sizes...")
		skip_sizes = fetch_remote_sizes(target_folder_for_sizes, target_site_for_sizes)
	zip_bytes = build_zip(build_path, skip_sizes)
	print(f"Archive size: {len(zip_bytes) / 1024:.1f} KB\n")

	with zipfile.ZipFile(io.BytesIO(zip_bytes)) as _zf:
		if not _zf.namelist():
			print("All files identical in size on the target; nothing to upload.")
			return True

	for attempt in range(1, DEPLOY_MAX_RETRIES + 1):
		if attempt > 1:
			wait = min(2 ** (attempt - 2), 8)
			print(f"Retry {attempt}/{DEPLOY_MAX_RETRIES} (waiting {wait}s)...")
			time.sleep(wait)

		print(f"Uploading to target '{DEPLOY_TARGET}' ...")
		try:
			response = requests.post(
				url,
				files={"archive": ("build.zip", zip_bytes, "application/zip")},
				data=form_data,
				headers=headers,
				timeout=DEPLOY_TIMEOUT,
			)
		except Exception as exc:
			print(f"  ✗ Upload exception: {exc}")
			if attempt == DEPLOY_MAX_RETRIES:
				return False
			continue

		if response.status_code == 403:
			print("  ✗ 403 Forbidden: invalid or missing DEPLOY_TOKEN.")
			print('    Set: export DEPLOY_TOKEN="<value from VPS DEPLOY_AUTH_TOKEN>"')
			return False

		if response.status_code == 200:
			data = response.json()
			if not data.get("failed"):
				_print_partial_failures(data)
				return True
			_print_partial_failures(data)
			if attempt < DEPLOY_MAX_RETRIES:
				print(
					f"  Partial upload — will retry ({len(data['failed'])} file(s) failed)."
				)
				continue
			return False

		if response.status_code >= 500:
			print(f"  ✗ Server error {response.status_code}: {response.text[:400]}")
			if attempt < DEPLOY_MAX_RETRIES:
				continue
			return False

		print(f"  ✗ {response.status_code}: {response.text[:400]}")
		return False

	return False


def main() -> None:
	target_host = "go.1ink.us" if DEPLOY_TARGET == "go" else "test.1ink.us"
	remote_folder = TARGET_FOLDER or PROJECT_NAME
	print(
		f"\n=== Deploying '{PROJECT_NAME}' via Contabo -> "
		f"https://{target_host}/{remote_folder}/ "
		f"(target={DEPLOY_TARGET}) ===\n"
	)

	source = resolve_build_dir()
	if not source.is_dir():
		print(f"ERROR: Build directory '{source}' does not exist.")
		print("Build the WASM client first, then re-run deploy_1ink.py.")
		sys.exit(1)

	try:
		health = requests.get(f"{CONTABO_BASE_URL}/api/deploy/health", timeout=10)
		if health.status_code == 200:
			data = health.json()
			status = data.get("status", "unknown")
			print(f"Contabo deploy service: {status}")
			if status != "ok":
				print("ERROR: Deploy service is not configured on the VPS.")
				sys.exit(1)
			if data.get("has_token") and not DEPLOY_TOKEN:
				print("ERROR: VPS requires DEPLOY_TOKEN but it is not set.")
				print('  export DEPLOY_TOKEN="<value from VPS DEPLOY_AUTH_TOKEN>"')
				sys.exit(1)
	except Exception as exc:
		print(
			f"Warning: Could not contact deploy health endpoint ({exc}); continuing anyway."
		)

	staging: Optional[Path] = None
	try:
		print(f"Staging from {source} ...")
		staging = stage_deploy_tree(source)
		print()
		success = deploy_bundle(staging)
	except FileNotFoundError as exc:
		print(f"ERROR: {exc}")
		sys.exit(1)
	finally:
		if staging is not None:
			shutil.rmtree(staging, ignore_errors=True)

	if success:
		print(f"\nLive URL: https://{target_host}/{remote_folder}/")
		print(f"Direct:   https://{target_host}/{remote_folder}/luanti.html")
	print(f"\n=== {'Deployment complete' if success else 'Deployment finished with errors'} ===")
	sys.exit(0 if success else 1)


if __name__ == "__main__":
	main()
