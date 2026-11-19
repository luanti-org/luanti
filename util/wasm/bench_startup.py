#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Startup time, engine size and heap of one generated client build.

For each run a fresh browser context (no cache, no IndexedDB, so a new world)
loads luanti.html, waits for the launcher to enable Play, starts a luanti_web
world and waits for the game loop, then lets it run for a few seconds and
reads the committed heap. A report holds every run and their medians::

	python3 util/wasm/bench_startup.py --label default --output wasm-size/startup-default.json
	python3 util/wasm/bench_startup.py --label mimalloc --build build-wasm-mimalloc \\
		--output wasm-size/startup-mimalloc.json
	python3 util/wasm/bench_startup.py --compare wasm-size/startup-default.json \\
		wasm-size/startup-mimalloc.json

What is measured, per run:

* menuMs   navigation start to "Play is enabled" (engine instantiated, core
           package mounted): the cost of the binary plus the page.
* worldMs  Play clicked to the game loop running (menu to "world ready").
* heapMiB  committed WebAssembly heap after `--settle` seconds in the world
           (Module.luantiStartup.heapBytes()). It only grows, and in steps from
           the 256 MiB initial size, so it reads 256 for anything smaller.
* breakMiB the allocator's sbrk break at the same moment
           (heapBreakBytes()): how much of the heap has been carved up, which
           includes fragmentation. This is the number that tells allocators
           apart; dlmalloc and mimalloc (on emmalloc) both take memory from sbrk.
* wasmBytes / jsBytes   the binary and glue that were actually served.

--compare applies the rule from wasm_porting.md ("mimalloc"): the candidate is
adopted only if the binary is smaller AND startup (menu + world, median) is
faster; anything else is a rejection. Numbers from a shared or headless
SwiftShader machine are noisy, so the comparison prints the spread of the
runs next to each median and calls a difference under the spread "no change".
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import platform
import shutil
import statistics
import threading
import time

from serve import create_server

GAME = "luanti_web"
MIB = 1048576
METRICS = ("menuMs", "worldMs", "totalMs", "heapMiB", "breakMiB")


def median_and_spread(values: list[float]) -> dict:
	return {"median": round(statistics.median(values), 1),
		"min": round(min(values), 1), "max": round(max(values), 1)}


def run_once(browser, base: str, args) -> dict:
	console: list[str] = []
	errors: list[str] = []
	context = browser.new_context(viewport={"width": 1280, "height": 720})
	page = context.new_page()
	page.on("console", lambda message: console.append(message.text))
	page.on("pageerror", lambda error: errors.append(str(error)))
	try:
		started = time.monotonic()
		page.goto(f"{base}/luanti.html?game={GAME}&view={args.view}{args.query}",
			wait_until="domcontentloaded", timeout=120_000)
		page.wait_for_function(
			"Module.luantiLauncher?.getState().runtimeReady === true && "
			"Module.luantiLauncher.getState().coreReady === true && "
			"!document.getElementById('luanti-play-button').disabled", timeout=180_000)
		menu = time.monotonic()
		page.click("#luanti-play-button")
		page.wait_for_function(
			"Module.luantiLauncher?.getState().phase === 'playing'", timeout=300_000)
		world = time.monotonic()
		page.wait_for_timeout(int(args.settle * 1000))
		info = page.evaluate("""() => ({
			build: Module.luantiBuild || null,
			startup: Module.luantiStartup ? {
				memory: Module.luantiStartup.memory, heap: Module.luantiStartup.heapBytes(),
				heapBreak: Module.luantiStartup.heapBreakBytes()
			} : null,
		})""")
	except BaseException:
		print("console tail at failure:\n" + "\n".join(line[:200] for line in console[-30:]))
		raise
	finally:
		context.close()
	assert not errors, errors
	assert info["startup"], "this build has no Module.luantiStartup"
	return {
		"menuMs": round((menu - started) * 1000, 1),
		"worldMs": round((world - menu) * 1000, 1),
		"totalMs": round((world - started) * 1000, 1),
		"heapMiB": round(info["startup"]["heap"] / MIB, 1),
		"breakMiB": round(info["startup"]["heapBreak"] / MIB, 1),
		"build": info["build"],
		"ceilingMiB": info["startup"]["memory"].get("maximum", 0) // MIB,
	}


def measure(args) -> dict:
	from playwright.sync_api import sync_playwright

	repo = Path(__file__).resolve().parents[2]
	output = (repo / args.build / "bin").resolve()
	if not (output / "luanti.html").exists():
		raise SystemExit(f"generated client not found under {output}")
	runs: list[dict] = []
	with create_server(output, "127.0.0.1", 0, quiet=True) as server:
		threading.Thread(target=server.serve_forever, daemon=True).start()
		try:
			with sync_playwright() as playwright:
				browser = playwright.chromium.launch(
					headless=True, executable_path=args.browser,
					args=["--no-sandbox", "--use-gl=angle", "--use-angle=swiftshader",
						"--enable-unsafe-swiftshader"])
				try:
					base = f"http://127.0.0.1:{server.server_address[1]}"
					for index in range(args.runs):
						result = run_once(browser, base, args)
						runs.append(result)
						print(f"run {index + 1}/{args.runs}: menu {result['menuMs']:.0f} ms, "
							f"world {result['worldMs']:.0f} ms, heap {result['heapMiB']} MiB "
							f"(break {result['breakMiB']} MiB) "
							f"(malloc: {(result['build'] or {}).get('malloc')})")
				finally:
					browser.close()
		finally:
			server.shutdown()
	builds = {json.dumps(run["build"], sort_keys=True) for run in runs}
	assert len(builds) == 1, f"runs disagree about the build: {builds}"
	return {
		"label": args.label,
		"date": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
		"host": platform.platform(),
		"browser": args.browser,
		"build": runs[0]["build"],
		"ceilingMiB": runs[0]["ceilingMiB"],
		"config": {"game": GAME, "view": args.view, "settleSeconds": args.settle,
			"runs": args.runs, "query": args.query},
		"wasmBytes": (output / "luanti.wasm").stat().st_size,
		"jsBytes": (output / "luanti.js").stat().st_size,
		"summary": {name: median_and_spread([run[name] for run in runs]) for name in METRICS},
		"runs": runs,
	}


def compare(baseline: dict, candidate: dict) -> tuple[str, bool]:
	"""Return (table, adopt). Adopt only if smaller binary AND faster startup."""
	lines = [f"{'':14s}{baseline['label']:>24s}{candidate['label']:>24s}{'change':>10s}"]

	def row(name: str, before: float, after: float, spread: str = "") -> None:
		change = f"{(after - before) / before * 100:+.1f}%" if before else ""
		lines.append(f"{name:14s}{before:>24.1f}{after:>24.1f}{change:>10s}{spread}")

	row("luanti.wasm", baseline["wasmBytes"] / MIB, candidate["wasmBytes"] / MIB)
	row("luanti.js", baseline["jsBytes"] / MIB, candidate["jsBytes"] / MIB)
	verdicts = {}
	for name, label in (("menuMs", "menu ms"), ("worldMs", "world ms"),
			("totalMs", "total ms"), ("heapMiB", "heap MiB"), ("breakMiB", "heap break MiB")):
		a, b = baseline["summary"][name], candidate["summary"][name]
		row(label, a["median"], b["median"],
			f"   spread {a['min']:.0f}-{a['max']:.0f} vs {b['min']:.0f}-{b['max']:.0f}")
		# (improvement of the median, run-to-run spread): a difference that
		# does not exceed the widest spread seen is noise, not a result.
		verdicts[name] = (a["median"] - b["median"],
			max(a["max"] - a["min"], b["max"] - b["min"]))
	smaller = candidate["wasmBytes"] < baseline["wasmBytes"]
	faster = verdicts["totalMs"][0] > verdicts["totalMs"][1]
	adopt = smaller and faster
	lines.append("")
	lines.append(f"binary smaller: {'yes' if smaller else 'no'}; "
		f"startup faster beyond the run-to-run spread: {'yes' if faster else 'no'} "
		f"({verdicts['totalMs'][0]:+.0f} ms against a spread of {verdicts['totalMs'][1]:.0f} ms)")
	lines.append("verdict: " + (f"adopt {candidate['label']}" if adopt else
		f"reject {candidate['label']}, keep {baseline['label']}"))
	return "\n".join(lines), adopt


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--browser", default=shutil.which("google-chrome"))
	parser.add_argument("--build", default="build-wasm", help="build directory (holds bin/)")
	parser.add_argument("--label", default="build")
	parser.add_argument("--runs", type=int, default=5)
	parser.add_argument("--view", default="100")
	parser.add_argument("--settle", type=float, default=10.0,
		help="seconds in the world before the heap is read")
	parser.add_argument("--query", default="", help="extra URL query, starting with &")
	parser.add_argument("--output", help="write the report here")
	parser.add_argument("--compare", nargs=2, metavar=("BASELINE", "CANDIDATE"),
		help="compare two reports instead of measuring")
	args = parser.parse_args()

	if args.compare:
		table, adopt = compare(*(json.loads(Path(p).read_text()) for p in args.compare))
		print(table)
		return 0
	if not args.browser:
		parser.error("Google Chrome was not found")
	report = measure(args)
	print(f"{report['label']}: " + ", ".join(
		f"{name} {report['summary'][name]['median']}" for name in METRICS)
		+ f"; luanti.wasm {report['wasmBytes'] / MIB:.2f} MiB")
	if args.output:
		target = Path(args.output)
		target.parent.mkdir(parents=True, exist_ok=True)
		target.write_text(json.dumps(report, indent=2) + "\n")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
