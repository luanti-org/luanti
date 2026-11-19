// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Startup checks that must pass before the engine allocates anything.
// This file is included with --pre-js, before every other pre-js, so a failed
// check stops luanti.js before packages.js starts downloading and before the
// engine reserves memory. It reads Module.luantiBuild, which CMake generates
// from client/web/build-info.js.in so the numbers below cannot drift from the
// link flags.
//
// 1. SIMD. A build linked with -DWASM_ENABLE_SIMD=ON contains v128
//    instructions. A browser without them rejects the module with an opaque
//    CompileError, so a tiny SIMD module is validated first and the page
//    reports a readable error instead.
//
// 2. The shared-memory ceiling. luanti.wasm imports one shared memory declared
//    with a 256 MiB start and a 2 GiB + 64 KiB maximum. A shared memory
//    reserves address space for its maximum when it is created, so a phone or
//    a 32-bit browser can refuse it before any game code runs. A memory
//    created here, with a smaller maximum, is a valid import for the same
//    binary (WebAssembly only requires provided.maximum <= declared maximum
//    and provided.initial >= declared initial) and Emscripten uses
//    Module.wasmMemory instead of creating its own, then posts it to the
//    pthread workers. So the fallback is the same luanti.wasm with a smaller
//    ceiling: no second link, no second binary to cache or verify. The memory
//    that was probed is the memory that is used, so there is no window in
//    which the reservation can succeed once and fail again.
//
//    Ceilings are tried largest first: the build's own maximum, then
//    FALLBACK_MAXIMUMS. `?maxmem=<MiB>` makes that the largest one tried.
//
// Result and diagnostics: Module.luantiStartup (see emscripten-module.d.ts).
// A failed check dispatches "luanti:startup-error" (the launcher shows it) and
// throws, which ends the evaluation of luanti.js.

(function() {
	"use strict";
	// pthread workers evaluate this file too. They receive the memory object
	// from the page that created it, so there is nothing to check there.
	if (typeof window === "undefined" || typeof Module === "undefined")
		return;

	var PAGE = 65536;
	var MIB = 1048576;
	// Tried after the build's maximum, largest first. Documented in
	// wasm_porting.md ("Appendix: Flag Matrix"); change both together.
	var FALLBACK_MAXIMUMS = [1024 * MIB, 512 * MIB];

	// (module (func (result v128) i32.const 0 i8x16.splat i8x16.popcnt))
	// A module with a SIMD instruction validates only where SIMD exists.
	var SIMD_PROBE = new Uint8Array([
		0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0,
		10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11
	]);

	var build = Module["luantiBuild"] || null;
	var startup = /** @type {NonNullable<LuantiModule["luantiStartup"]>} */ ({
		"build": build,
		"simd": {"required": !!(build && build["simd"]), "supported": null},
		"memory": {"state": "skipped"},
		"fatal": null,
		// Committed heap size right now, in bytes (grows, never shrinks).
		"heapBytes": function() {
			var memory = Module["wasmMemory"];
			if (memory && memory.buffer)
				return memory.buffer.byteLength;
			var heap = Module["HEAPU8"];
			return heap ? heap.length : 0;
		},
		// How far the allocator has carved up the heap (the sbrk break), in
		// bytes; 0 until the engine is running. Unlike heapBytes() it moves
		// while the heap is still below its initial size, so it is what an
		// allocator comparison reads (src/porting_emscripten.cpp).
		"heapBreakBytes": function() {
			var read = Module["_luanti_heap_break"];
			return typeof read === "function" ? read() >>> 0 : 0;
		}
	});
	Module["luantiStartup"] = startup;

	function mib(bytes) {
		return Math.round(bytes / MIB);
	}

	function fatal(code, message) {
		startup["fatal"] = {"code": code, "message": message};
		try {
			window.dispatchEvent(new CustomEvent("luanti:startup-error", {"detail": startup["fatal"]}));
		} catch (_) {
		}
		throw new Error(message);
	}

	function warn(message) {
		var report = Module["printErr"] || (typeof console !== "undefined" && console.warn);
		if (report)
			report.call(typeof console !== "undefined" ? console : null, message);
	}

	function supportsSimd() {
		try {
			return WebAssembly.validate(SIMD_PROBE);
		} catch (_) {
			return false;
		}
	}

	function requestedCeiling() {
		try {
			var raw = new URLSearchParams(window.location.search).get("maxmem");
			var value = raw === null ? NaN : Number(raw);
			if (Number.isInteger(value) && value > 0)
				return value * MIB;
		} catch (_) {
		}
		return 0;
	}

	// Distinct whole-page ceilings, largest first, none below the start size.
	function ceilings(initial, buildMaximum, requested) {
		var top = requested ? Math.min(requested, buildMaximum) : buildMaximum;
		top = Math.max(initial, Math.floor(top / PAGE) * PAGE);
		var list = [top];
		FALLBACK_MAXIMUMS.forEach(function(value) {
			if (value < top && value >= initial && list.indexOf(value) < 0)
				list.push(value);
		});
		return list;
	}

	function reserve(initial, list) {
		var attempts = [];
		for (var i = 0; i < list.length; i++) {
			var started = performance.now();
			try {
				var memory = new WebAssembly.Memory({
					"initial": initial / PAGE,
					"maximum": list[i] / PAGE,
					"shared": true
				});
				if (typeof SharedArrayBuffer !== "undefined" && !(memory.buffer instanceof SharedArrayBuffer))
					throw new Error("the browser returned a memory that is not shared");
				attempts.push({"maximum": list[i], "ok": true, "ms": performance.now() - started});
				return {"memory": memory, "attempts": attempts};
			} catch (error) {
				attempts.push({
					"maximum": list[i], "ok": false, "ms": performance.now() - started,
					"error": String(error && error.message || error)
				});
			}
		}
		return {"memory": null, "attempts": attempts};
	}

	if (typeof WebAssembly === "undefined")
		fatal("no-webassembly", "This browser does not support WebAssembly, which Luanti needs.");

	if (startup["simd"]["required"]) {
		startup["simd"]["supported"] = supportsSimd();
		if (!startup["simd"]["supported"]) {
			fatal("simd", "This build of Luanti needs WebAssembly SIMD, which this browser does not " +
				"support. Update the browser (SIMD arrived in Chrome and Edge 91, Firefox 89 and " +
				"Safari 16.4) or open a build made without SIMD.");
		}
	}

	if (Module["wasmMemory"]) {
		// An embedding page supplied its own memory; it owns the ceiling.
		startup["memory"] = {"state": "provided"};
	} else if (build && build["initialMemory"] && build["maximumMemory"]) {
		if (window.crossOriginIsolated === false) {
			fatal("not-isolated", "This page is not cross-origin isolated, so the browser will not " +
				"give it the shared memory Luanti needs. The server must send " +
				"Cross-Origin-Opener-Policy: same-origin and Cross-Origin-Embedder-Policy: " +
				"require-corp (util/wasm/serve.py does).");
		}
		var initial = build["initialMemory"];
		var list = ceilings(initial, build["maximumMemory"], requestedCeiling());
		var result = reserve(initial, list);
		startup["memory"] = {
			"state": result["memory"] ? "reserved" : "failed",
			"initial": initial,
			"buildMaximum": build["maximumMemory"],
			"maximum": result["memory"] ? result["attempts"][result["attempts"].length - 1]["maximum"] : 0,
			"reduced": !!result["memory"] && result["attempts"].length > 1,
			"attempts": result["attempts"]
		};
		if (!result["memory"]) {
			var last = result["attempts"][result["attempts"].length - 1];
			fatal("memory", "This browser would not reserve shared WebAssembly memory for the engine. " +
				"It refused every ceiling from " + mib(list[0]) + " MiB down to " +
				mib(list[list.length - 1]) + " MiB (start size " + mib(initial) + " MiB): " +
				last["error"] + ". Close other tabs and apps and reload, or try a desktop browser.");
		}
		Module["wasmMemory"] = result["memory"];
		if (startup["memory"]["reduced"]) {
			var refused = result["attempts"][0];
			warn("Memory ceiling reduced to " + mib(startup["memory"]["maximum"]) + " MiB: the browser " +
				"refused " + mib(refused["maximum"]) + " MiB (" + refused["error"] + ").");
		}
	}
})();
