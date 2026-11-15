#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Frame-time benchmark for the generated browser client.

Creates a fresh luanti_web world (a new browser profile every run), drops a
world mod into it that attaches the player to an invisible rig, and flies the
rig once around the showcase island at a fixed height while looking at its
centre. The rig follows server wall-clock time, not client frames, so a slow
build covers the same path as a fast one in the same time. Frame statistics
come from Module.luantiPerf (client/web/perf.js) for exactly the frames drawn
between the mod's "path start" and "path end" log lines, and are written as a
JSON report.

On a real GPU, locally::

	python3 util/wasm/bench_frame_time.py --headed --gpu \\
		--browser "$(command -v google-chrome)" --output bench.json

In CI (SwiftShader, headless) the numbers are only good for spotting large
regressions between commits, never for comparing with a real GPU.

The report's "server" entry says whether the internal server kept up during
the path. Without WASMFS every SQLite read and write is a synchronous call
on the browser main thread, which in the proxied render mode is also busy
running GL, so the server can stall for seconds and the camera then stays
near its start: two runs whose server rates differ widely did not render the
same scene. --no-map-io stores the world in the "dummy" backends (nothing is
written) to compare rendering alone.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import threading
import time

from playwright.sync_api import Page, sync_playwright

from serve import create_server

GAME = "luanti_web"
WORLD = f"/home/web_user/.luanti/worlds/{GAME}"
CONFIG = "/home/web_user/.luanti/minetest.conf"
MOD_DIR = f"{WORLD}/worldmods/lw_bench"

# The path: an ellipse around the island (x = -36..36, z = -22..66, see
# games/luanti_web/mods/lw_world/init.lua), starting just south of the spawn
# plaza and looking at the courtyard the whole way round.
MOD_LUA = r"""
-- written by util/wasm/bench_frame_time.py; not part of the game
local WARMUP = %(warmup)f
local DURATION = %(duration)f
local CENTER = vector.new(0, 10, 22)
local RX, RZ, HEIGHT = 44, 56, 24

local rig, player_name, t0
local finished = false
-- Server progress during the path: globalsteps run and the longest gap
-- between two of them. A server starved by the browser main thread steps
-- rarely, and the rig (so the camera) then barely moves.
local steps, last_us, worst_gap_us = 0, nil, 0

core.register_entity("lw_bench:rig", {
	initial_properties = {
		physical = false,
		collide_with_objects = false,
		pointable = false,
		visual = "sprite",
		textures = {"blank.png"},
		is_visible = false,
		static_save = false,
	},
})

local function path_pos(t)
	local a = t * 2 * math.pi - math.pi / 2
	return vector.new(CENTER.x + RX * math.cos(a), HEIGHT,
		CENTER.z + RZ * math.sin(a))
end

local function aim(player, pos)
	local dir = vector.direction(vector.offset(pos, 0, 1.6, 0), CENTER)
	player:set_look_horizontal(core.dir_to_yaw(dir))
	player:set_look_vertical(-math.asin(dir.y))
end

local function start(name)
	local player = core.get_player_by_name(name)
	if not player then
		return
	end
	local pos = path_pos(0)
	rig = core.add_entity(pos, "lw_bench:rig")
	player:set_attach(rig, "", vector.zero(), vector.zero())
	aim(player, pos)
	t0 = core.get_us_time()
	core.log("action", "[lw_bench] path start")
end

core.register_on_joinplayer(function(player)
	if player_name then
		return
	end
	player_name = player:get_player_name()
	core.after(WARMUP, start, player_name)
end)

core.register_globalstep(function()
	if not t0 or finished then
		return
	end
	local player = core.get_player_by_name(player_name)
	local now = core.get_us_time()
	steps = steps + 1
	if last_us then
		worst_gap_us = math.max(worst_gap_us, now - last_us)
	end
	last_us = now
	local t = (now - t0) / 1e6 / DURATION
	if t >= 1 or not player or not rig:get_pos() then
		finished = true
		core.log("action", string.format(
			"[lw_bench] path end steps=%%d worst_gap_ms=%%.1f", steps,
			worst_gap_us / 1000))
		return
	end
	local pos = path_pos(t)
	rig:move_to(pos, true)
	aim(player, pos)
end)
"""

# --no-map-io: a world.mt written before the world is first opened.
NO_MAP_IO_WORLD_MT = f"""gameid = {GAME}
backend = dummy
player_backend = dummy
mod_storage_backend = dummy
"""

PRESENT_STATS = ("Module.luantiPresentStats ? "
	"Module.luantiPresentStats() : null")
# Main-thread animation frames per second over 2 s, measured after the path:
# what the compositor can still deliver while the engine runs.
RAF_RATE = """() => new Promise(done => {
	let frames = 0;
	const start = performance.now();
	function tick() {
		frames++;
		if (performance.now() - start < 2000) requestAnimationFrame(tick);
		else done(Math.round(frames / 2 * 10) / 10);
	}
	requestAnimationFrame(tick);
})"""

DEFAULT_SETTINGS = ["fps_max = 1000", "fps_max_unfocused = 1000"]

COMPAT_STATS = ("Module.luantiWebGLCompat ? "
	"Module.luantiWebGLCompat.stats() : null")

MOD_CONF = "name = lw_bench\ndescription = Frame-time benchmark path (generated)\n"


def format_page_error(error: object) -> str:
	return str(getattr(error, "stack", None) or error)


def wait_for_marker(page: Page, console: list[str], marker: str,
		timeout: float) -> None:
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		if any(marker in line for line in console):
			return
		page.wait_for_timeout(50)
	tail = "\n".join(line[:200] for line in console[-30:])
	raise AssertionError(f"never saw {marker!r}\nconsole tail:\n{tail}")


def server_progress(console: list[str], duration: float) -> dict | None:
	match = None
	for line in console:
		match = re.search(r"\[lw_bench\] path end steps=(\d+) worst_gap_ms=([\d.]+)",
			line) or match
	if not match:
		return None
	steps = int(match.group(1))
	return {"steps": steps, "stepHz": round(steps / duration, 2),
		"worstGapMs": float(match.group(2))}


def gpu_description(page: Page) -> dict:
	# The engine's own context is not reachable from here in every render
	# mode, so ask a throwaway canvas which GPU the browser exposes.
	return page.evaluate("""() => {
		const gl = document.createElement("canvas").getContext("webgl2") ||
			document.createElement("canvas").getContext("webgl");
		if (!gl) return {};
		const ext = gl.getExtension("WEBGL_debug_renderer_info");
		return {
			vendor: ext ? gl.getParameter(ext.UNMASKED_VENDOR_WEBGL) : gl.getParameter(gl.VENDOR),
			renderer: ext ? gl.getParameter(ext.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER)
		};
	}""")


def timeline(frames: list[dict]) -> list[dict]:
	"""Frames per wall-clock second, to show warm-up and hitches."""
	out: list[dict] = []
	elapsed = 0.0
	bucket: list[float] = []
	for frame in frames:
		bucket.append(frame["frameMs"])
		elapsed += frame["frameMs"]
		if elapsed >= 1000.0 * (len(out) + 1):
			out.append({"second": len(out) + 1, "frames": len(bucket),
				"worstMs": round(max(bucket), 2)})
			bucket = []
	return out


def git_revision(repo: Path) -> str:
	try:
		return subprocess.run(["git", "describe", "--always", "--dirty"],
			cwd=repo, capture_output=True, text=True, check=True).stdout.strip()
	except (OSError, subprocess.CalledProcessError):
		return ""


def run(page: Page, url: str, console: list[str], args) -> dict:
	page.goto(url, wait_until="domcontentloaded", timeout=120_000)
	page.wait_for_function(
		"Module.luantiPersistence?.getState().state === 'ready'", timeout=120_000)
	page.wait_for_function(
		"Module.luantiLauncher?.getState().runtimeReady === true", timeout=120_000)
	assert page.evaluate("Module.luantiPerf?.available()"), \
		"this build has no Module.luantiPerf"

	# Uncapped by default so the report measures what the build can do, not
	# fps_max; the headless page also counts as unfocused. Later lines win.
	settings = "".join(f"{s}\n" for s in DEFAULT_SETTINGS + args.setting)
	world_mt = NO_MAP_IO_WORLD_MT if args.no_map_io else ""
	page.evaluate("""([dir, lua, conf, configPath, settings, world, worldMt]) => {
		FS.mkdirTree(dir);
		FS.writeFile(dir + "/init.lua", lua);
		FS.writeFile(dir + "/mod.conf", conf);
		if (settings)
			FS.writeFile(configPath, "# written by bench_frame_time.py\\n" + settings);
		if (worldMt)
			FS.writeFile(world + "/world.mt", worldMt);
	}""", [MOD_DIR, MOD_LUA % {"warmup": args.warmup, "duration": args.duration},
		MOD_CONF, CONFIG, settings, WORLD, world_mt])

	page.click("#luanti-play-button")
	page.wait_for_function(
		"Module.luantiLauncher?.getState().phase === 'playing'", timeout=300_000)
	wait_for_marker(page, console, "[lw_bench] path start", 300 + args.warmup)
	start = page.evaluate("Module.luantiPerf.cursor()")
	compat_start = page.evaluate(COMPAT_STATS)
	shown_start = page.evaluate(PRESENT_STATS)
	t_start = time.monotonic()
	wait_for_marker(page, console, "[lw_bench] path end", args.duration + 60)
	compat_end = page.evaluate(COMPAT_STATS)
	shown_end = page.evaluate(PRESENT_STATS)
	elapsed = time.monotonic() - t_start
	samples = page.evaluate("since => Module.luantiPerf.samples(since)", start)
	summary = page.evaluate("since => Module.luantiPerf.summary({since})", start)
	if args.screenshot:
		Path(args.screenshot).parent.mkdir(parents=True, exist_ok=True)
		page.screenshot(path=args.screenshot)
	frames = samples["frames"]
	assert frames, "no frames were recorded during the path"
	return {
		"summary": summary,
		"timeline": timeline(frames),
		"frameMs": [round(f["frameMs"], 2) for f in frames],
		"webgl": page.evaluate("Module.luantiWebGLInfo || null"),
		# Offscreen mode only: frames the page actually put on screen (the
		# rest were replaced by a newer frame before an animation frame).
		"presented": {
			"shown": shown_end["shown"] - shown_start["shown"],
			"dropped": shown_end["dropped"] - shown_start["dropped"],
			"shownFps": round((shown_end["shown"] - shown_start["shown"]) / elapsed, 2),
		} if shown_start and shown_end and shown_end["shown"] else None,
		"rafHz": page.evaluate(RAF_RATE),
		# Server globalsteps during the path. Frame times only compare when
		# the server kept up: a starved server leaves the camera parked.
		"server": server_progress(console, args.duration),
		# Shim work during the path only (null when the shim is not loaded
		# in the page, i.e. when GL does not run on the main thread).
		"webglCompat": {key: compat_end[key] - compat_start[key]
			for key in compat_end} if compat_start and compat_end else None,
		"heapGrowable": page.evaluate(
			"!!(Module.HEAPU8.buffer.growable || Module.HEAPU8.buffer.resizable)"),
		"gpu": gpu_description(page),
		"userAgent": page.evaluate("navigator.userAgent"),
		"hardwareConcurrency": page.evaluate("navigator.hardwareConcurrency"),
	}


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--browser", default=shutil.which("google-chrome"),
		help="Chrome executable Playwright should drive")
	parser.add_argument("--build", default="build-wasm")
	parser.add_argument("--output", default="bench-frame-time.json",
		help="JSON report path (default: %(default)s)")
	parser.add_argument("--label", default="",
		help="free-form name for this configuration, copied into the report")
	parser.add_argument("--headed", action="store_true")
	parser.add_argument("--gpu", action="store_true",
		help="use the browser's real GPU instead of SwiftShader")
	parser.add_argument("--view", default="100", choices=("60", "100", "160"),
		help="launcher view distance (default: %(default)s)")
	parser.add_argument("--duration", type=float, default=30.0,
		help="seconds to fly the path (default: %(default)s)")
	parser.add_argument("--warmup", type=float, default=10.0,
		help="seconds between joining and starting the path (default: %(default)s)")
	parser.add_argument("--width", type=int, default=1280)
	parser.add_argument("--height", type=int, default=720)
	parser.add_argument("--query", default="",
		help="extra launcher query parameters, e.g. '&webgl=1'")
	parser.add_argument("--setting", action="append", default=[],
		metavar="KEY=VALUE", help="minetest.conf line for this run (repeatable)")
	parser.add_argument("--screenshot", help="save a screenshot at path end")
	parser.add_argument("--no-map-io", action="store_true",
		help="keep the world in the dummy backends: no map, player or mod "
		"storage file I/O, so the server cannot stall on it")
	args = parser.parse_args()
	if not args.browser:
		parser.error("no browser executable was found; pass --browser")
	for line in args.setting:
		if "=" not in line or "\n" in line:
			parser.error(f"--setting needs KEY=VALUE, got {line!r}")

	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "luanti.html").exists():
		parser.error(f"generated client not found under {output}")

	console: list[str] = []
	page_errors: list[str] = []
	with create_server(output, "127.0.0.1", 0, quiet=True) as server:
		threading.Thread(target=server.serve_forever, daemon=True).start()
		try:
			with sync_playwright() as playwright:
				flags = ["--no-sandbox"]
				if args.gpu:
					flags += ["--enable-gpu", "--ignore-gpu-blocklist"]
				else:
					flags += ["--use-gl=angle", "--use-angle=swiftshader",
						"--enable-unsafe-swiftshader"]
				browser = playwright.chromium.launch(headless=not args.headed,
					executable_path=args.browser, args=flags)
				page = browser.new_context(viewport={
					"width": args.width, "height": args.height}).new_page()
				page.on("console", lambda message: console.append(message.text))
				page.on("pageerror", lambda error: page_errors.append(
					format_page_error(error)))
				url = (f"http://127.0.0.1:{server.server_address[1]}/luanti.html"
					f"?game={GAME}&view={args.view}{args.query}")
				try:
					result = run(page, url, console, args)
				except BaseException:
					print("console tail at failure:\n" + "\n".join(
						line[:200] for line in console[-40:]))
					raise
				finally:
					browser.close()
		finally:
			server.shutdown()

	report = {
		"label": args.label,
		"revision": git_revision(repo),
		"date": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
		"host": platform.platform(),
		"config": {"view": int(args.view), "duration": args.duration,
			"warmup": args.warmup, "viewport": [args.width, args.height],
			"gpu": args.gpu, "headed": args.headed, "query": args.query,
			"settings": DEFAULT_SETTINGS + args.setting,
			"noMapIo": args.no_map_io},
		"pageErrors": page_errors,
		**result,
	}
	Path(args.output).parent.mkdir(parents=True, exist_ok=True)
	Path(args.output).write_text(json.dumps(report, indent=1) + "\n")
	s = result["summary"]
	print(f"{args.label or 'bench'}: {s['frames']} frames in {s['seconds']} s, "
		f"{s['fps']} fps; frame ms p50 {s['frameMs']['p50']} "
		f"p95 {s['frameMs']['p95']} p99 {s['frameMs']['p99']}; "
		f"present ms p95 {s['swapMs']['p95']}; drawcalls p50 {s['drawcalls']['p50']}; "
		f"rAF {result['rafHz']} Hz; shown {(result['presented'] or {}).get('shownFps', '-')} fps; "
		f"server {(result['server'] or {}).get('stepHz', '-')} steps/s, "
		f"worst gap {(result['server'] or {}).get('worstGapMs', '-')} ms; "
		f"{(result['webgl'] or {}).get('render', '?')} render; "
		f"GPU {result['gpu'].get('renderer', '?')}")
	print(f"report: {args.output}")
	assert not page_errors, page_errors
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
