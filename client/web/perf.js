// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Module.luantiPerf: frame statistics recorded by the engine.
// This file is included with --pre-js.
//
// The application worker appends one record per game frame to a ring in
// shared memory (src/porting_emscripten.cpp, PerfRing). This reads that ring
// directly, so neither recording nor reading costs a thread hop. Records are
// only written in-game (Game::drawScene), not in the main menu.
//
//   Module.luantiPerf.available()        -> true once the engine is linked in
//   Module.luantiPerf.cursor()           -> frames recorded so far
//   Module.luantiPerf.samples(since)     -> per-frame records from `since`
//   Module.luantiPerf.summary(opts)      -> percentiles over a window
//
// summary() takes {seconds: n} (the last n seconds of frames, default 5) or
// {since: cursor} (every frame after a cursor() taken earlier).

(function() {
	"use strict";
	if (typeof window === "undefined" || typeof Module === "undefined")
		return;

	var RING_VERSION = 1;
	// Header words: version, capacity, words per frame, written.
	var HEADER_WORDS = 4;

	function ring() {
		var heap = Module["HEAPU8"];
		var locate = Module["_luanti_perf_ring"];
		if (!heap || typeof locate !== "function")
			return null;
		var base = locate() >>> 0;
		if (!base || base % 4)
			return null;
		var u32 = new Uint32Array(heap.buffer);
		var word = base >>> 2;
		if (u32[word] !== RING_VERSION)
			return null;
		return {
			u32: u32,
			f32: new Float32Array(heap.buffer),
			frames: word + HEADER_WORDS,
			capacity: u32[word + 1],
			stride: u32[word + 2],
			written: Atomics.load(u32, word + 3)
		};
	}

	function read(r, since) {
		var first = Math.max(since | 0, r.written - r.capacity + 1, 0);
		var out = [];
		for (var n = first; n < r.written; n++) {
			var at = r.frames + (n % r.capacity) * r.stride;
			out.push({
				"frameMs": r.f32[at],
				"drawMs": r.f32[at + 1],
				"swapMs": r.f32[at + 2],
				"drawcalls": r.u32[at + 3],
				"primitives": r.u32[at + 4],
				"meshQueue": r.u32[at + 5]
			});
		}
		return {"first": first, "next": r.written, "frames": out};
	}

	function percentile(sorted, p) {
		if (!sorted.length)
			return 0;
		var rank = Math.ceil(p / 100 * sorted.length) - 1;
		return sorted[Math.min(sorted.length - 1, Math.max(0, rank))];
	}

	function distribution(values) {
		var sorted = values.slice().sort(function(a, b) { return a - b; });
		var sum = 0;
		for (var i = 0; i < sorted.length; i++)
			sum += sorted[i];
		function round(v) { return Math.round(v * 1000) / 1000; }
		return {
			"mean": round(sorted.length ? sum / sorted.length : 0),
			"p50": round(percentile(sorted, 50)),
			"p95": round(percentile(sorted, 95)),
			"p99": round(percentile(sorted, 99)),
			"max": round(sorted.length ? sorted[sorted.length - 1] : 0)
		};
	}

	/** @param {Array<Object<string, number>>} frames */
	function summarize(frames) {
		function column(name) {
			return frames.map(function(f) { return f[name]; });
		}
		var total = 0;
		for (var i = 0; i < frames.length; i++)
			total += frames[i]["frameMs"];
		var mesh = column("meshQueue");
		return {
			"frames": frames.length,
			"seconds": Math.round(total) / 1000,
			"fps": total > 0 ? Math.round(frames.length / total * 1e5) / 100 : 0,
			"frameMs": distribution(column("frameMs")),
			"drawMs": distribution(column("drawMs")),
			"swapMs": distribution(column("swapMs")),
			"drawcalls": distribution(column("drawcalls")),
			"primitives": distribution(column("primitives")),
			"meshQueue": distribution(mesh),
			"meshQueueLast": mesh.length ? mesh[mesh.length - 1] : 0
		};
	}

	Module["luantiPerf"] = {
		"available": function() {
			return ring() !== null;
		},
		"cursor": function() {
			var r = ring();
			return r ? r.written : 0;
		},
		"samples": function(since) {
			var r = ring();
			return r ? read(r, since) : {"first": 0, "next": 0, "frames": []};
		},
		"summary": function(opts) {
			var r = ring();
			if (!r)
				return summarize([]);
			opts = opts || {};
			if (typeof opts["since"] === "number")
				return summarize(read(r, opts["since"]).frames);
			var seconds = typeof opts["seconds"] === "number" ? opts["seconds"] : 5;
			var all = read(r, 0).frames;
			var budget = seconds * 1000;
			var start = all.length;
			while (start > 0 && budget > 0)
				budget -= all[--start]["frameMs"];
			return summarize(all.slice(start));
		}
	};
})();
