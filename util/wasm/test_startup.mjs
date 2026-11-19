// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Unit tests for client/web/startup.js (SIMD detection and the shared-memory
// ceiling probe) against fake WebAssembly.Memory constructors. Run:
//   node --test util/wasm/test_startup.mjs
// The browser round trip, against the real luanti.wasm, lives in
// util/wasm/test_memory_probe_browser.py.

import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import test from "node:test";

const source = await readFile(new URL("../../client/web/startup.js", import.meta.url), "utf8");

const MIB = 1048576;
const PAGE = 65536;
const BUILD = {
	type: "Release", simd: false, lto: false, malloc: "default",
	initialMemory: 256 * MIB, maximumMemory: 2147549184,
};

/**
 * Runs startup.js with fakes. `limit` is the largest `maximum` (bytes) the
 * fake browser will reserve; anything above throws like Chrome's
 * "RangeError: WebAssembly.Memory(): could not allocate memory".
 */
function run({build = BUILD, limit = Infinity, search = "", isolated = true,
		simd = true, worker = false, memory, shared = true, extra = {}} = {}) {
	const created = [];
	const events = [];
	const warnings = [];
	class FakeMemory {
		constructor(descriptor) {
			if (descriptor.maximum * PAGE > limit)
				throw new RangeError("WebAssembly.Memory(): could not allocate memory");
			this.descriptor = descriptor;
			this.buffer = shared ? new SharedArrayBuffer(8) : new ArrayBuffer(8);
			created.push(descriptor);
		}
	}
	const fakeWebAssembly = {Memory: FakeMemory, validate: () => simd};
	const module = {printErr: message => warnings.push(message), ...extra};
	if (build)
		module.luantiBuild = build;
	if (memory)
		module.wasmMemory = memory;
	const window = {
		location: {search},
		crossOriginIsolated: isolated,
		dispatchEvent: event => events.push(event),
	};
	class FakeEvent {
		constructor(type, init) {
			this.type = type;
			this.detail = init && init.detail;
		}
	}
	const execute = new Function("Module", "window", "WebAssembly", "CustomEvent",
		"performance", "URLSearchParams", "SharedArrayBuffer", "console", source);
	const args = [module, worker ? undefined : window, fakeWebAssembly, FakeEvent,
		{now: () => 0}, URLSearchParams, SharedArrayBuffer, {warn: m => warnings.push(m)}];
	let error = null;
	try {
		execute(...args);
	} catch (thrown) {
		error = thrown;
	}
	return {module, created, events, warnings, error};
}

test("the SIMD probe is a valid SIMD module in this runtime", () => {
	// Node ships with SIMD, so the real validator must accept the probe; a
	// browser that rejects it is exactly the one that cannot run a SIMD build.
	const result = run({build: {...BUILD, simd: true}, simd: true});
	assert.equal(result.module.luantiStartup.simd.supported, true);
	const probe = source.match(/new Uint8Array\(\[([^\]]+)\]\)/s)[1]
		.split(",").map(value => Number(value.trim()));
	assert.equal(WebAssembly.validate(new Uint8Array(probe)), true);
	// Corrupt the opcode: the same bytes must stop validating, so the check
	// is not vacuous.
	probe[probe.length - 3] = 0xff;
	assert.equal(WebAssembly.validate(new Uint8Array(probe)), false);
});

test("pthread workers skip every check", () => {
	const result = run({worker: true});
	assert.equal(result.error, null);
	assert.equal(result.created.length, 0);
	assert.equal(result.module.luantiStartup, undefined);
	assert.equal(result.module.wasmMemory, undefined);
});

test("the build's own ceiling is used when the browser grants it", () => {
	const {module, created, error} = run();
	assert.equal(error, null);
	assert.equal(created.length, 1);
	assert.deepEqual(created[0], {initial: 4096, maximum: 32769, shared: true});
	assert.equal(module.wasmMemory.descriptor, created[0]);
	const memory = module.luantiStartup.memory;
	assert.equal(memory.state, "reserved");
	assert.equal(memory.maximum, 2147549184);
	assert.equal(memory.reduced, false);
	assert.equal(memory.attempts.length, 1);
	assert.equal(module.luantiStartup.heapBytes(), 8);
});

test("a browser that refuses 2 GiB gets 1 GiB, and the probed memory is the one used", () => {
	const {module, created, warnings, error} = run({limit: 1024 * MIB});
	assert.equal(error, null);
	// Only the successful reservation is kept: nothing is probed and thrown away.
	assert.deepEqual(created, [{initial: 4096, maximum: 16384, shared: true}]);
	assert.equal(module.wasmMemory.descriptor, created[0]);
	const memory = module.luantiStartup.memory;
	assert.equal(memory.state, "reserved");
	assert.equal(memory.maximum, 1024 * MIB);
	assert.equal(memory.reduced, true);
	assert.deepEqual(memory.attempts.map(a => [a.maximum, a.ok]),
		[[2147549184, false], [1024 * MIB, true]]);
	assert.match(memory.attempts[0].error, /could not allocate/);
	assert.equal(warnings.length, 1);
	assert.match(warnings[0], /reduced to 1024 MiB/);
});

test("the second fallback is 512 MiB", () => {
	const {module, created} = run({limit: 600 * MIB});
	assert.deepEqual(created, [{initial: 4096, maximum: 8192, shared: true}]);
	assert.equal(module.luantiStartup.memory.maximum, 512 * MIB);
	assert.equal(module.luantiStartup.memory.attempts.length, 3);
});

test("every ceiling refused is a readable fatal error and the engine never starts", () => {
	const {module, events, error, created} = run({limit: 128 * MIB});
	assert.ok(error instanceof Error);
	assert.match(error.message, /would not reserve shared WebAssembly memory/);
	assert.match(error.message, /2048 MiB down to 512 MiB/);
	assert.equal(created.length, 0);
	assert.equal(module.wasmMemory, undefined);
	assert.equal(module.luantiStartup.fatal.code, "memory");
	assert.equal(module.luantiStartup.memory.state, "failed");
	assert.equal(events.length, 1);
	assert.equal(events[0].type, "luanti:startup-error");
	assert.equal(events[0].detail.message, error.message);
});

test("a memory that is not shared counts as a refusal", () => {
	const {error, module} = run({shared: false});
	assert.ok(error instanceof Error);
	assert.match(error.message, /not shared/);
	assert.equal(module.luantiStartup.fatal.code, "memory");
});

test("?maxmem lowers the first ceiling tried and never raises it past the build's", () => {
	const first = search => run({search}).created[0].maximum * PAGE / MIB;
	assert.equal(first("?maxmem=1024"), 1024);
	assert.equal(first("?game=devtest&maxmem=700"), 700);
	assert.equal(first("?maxmem=512"), 512);
	assert.equal(first("?maxmem=99999"), 2147549184 / MIB);
	// Not a positive whole number: ignored.
	assert.equal(first("?maxmem=abc"), 2147549184 / MIB);
	assert.equal(first("?maxmem=-5"), 2147549184 / MIB);
	assert.equal(first("?maxmem=1.5"), 2147549184 / MIB);
	// Below the start size the memory could not be created at all: raised to it.
	assert.equal(first("?maxmem=100"), 256);
});

test("?maxmem still falls back below the requested ceiling", () => {
	const {module} = run({search: "?maxmem=1024", limit: 600 * MIB});
	assert.equal(module.luantiStartup.memory.maximum, 512 * MIB);
	assert.deepEqual(module.luantiStartup.memory.attempts.map(a => a.maximum),
		[1024 * MIB, 512 * MIB]);
});

test("a SIMD build is refused, before any memory is reserved, where SIMD is missing", () => {
	const {module, events, error, created} = run({build: {...BUILD, simd: true}, simd: false});
	assert.ok(error instanceof Error);
	assert.match(error.message, /needs WebAssembly SIMD/);
	assert.equal(created.length, 0);
	assert.equal(module.luantiStartup.simd.required, true);
	assert.equal(module.luantiStartup.simd.supported, false);
	assert.equal(module.luantiStartup.fatal.code, "simd");
	assert.equal(events[0].detail.code, "simd");
});

test("a build without SIMD never consults the SIMD validator", () => {
	const {module, error} = run({simd: false});
	assert.equal(error, null);
	assert.equal(module.luantiStartup.simd.required, false);
	assert.equal(module.luantiStartup.simd.supported, null);
});

test("a page that is not cross-origin isolated is told why", () => {
	const {module, error, created} = run({isolated: false});
	assert.ok(error instanceof Error);
	assert.match(error.message, /Cross-Origin-Opener-Policy/);
	assert.equal(created.length, 0);
	assert.equal(module.luantiStartup.fatal.code, "not-isolated");
});

test("a memory supplied by an embedding page is left alone", () => {
	const supplied = {buffer: new SharedArrayBuffer(16)};
	const {module, created, error} = run({memory: supplied});
	assert.equal(error, null);
	assert.equal(created.length, 0);
	assert.equal(module.wasmMemory, supplied);
	assert.equal(module.luantiStartup.memory.state, "provided");
	assert.equal(module.luantiStartup.heapBytes(), 16);
});

test("without build info the probe is skipped instead of guessing sizes", () => {
	const {module, created, error} = run({build: null});
	assert.equal(error, null);
	assert.equal(created.length, 0);
	assert.equal(module.luantiStartup.memory.state, "skipped");
	assert.equal(module.wasmMemory, undefined);
});

test("the allocator break is read from the engine once it exports it", () => {
	const before = run().module.luantiStartup;
	assert.equal(before.heapBreakBytes(), 0);
	// 0xFFFFFFFF + 1 wraps like a 32-bit pointer would be read: it must stay unsigned.
	const {module} = run({extra: {_luanti_heap_break: () => -1}});
	assert.equal(module.luantiStartup.heapBreakBytes(), 4294967295);
	const grown = run({extra: {_luanti_heap_break: () => 5 * MIB}}).module.luantiStartup;
	assert.equal(grown.heapBreakBytes(), 5 * MIB);
});
