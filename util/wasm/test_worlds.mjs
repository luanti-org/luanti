// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Unit tests for client/web/worlds.js (the world library) against an
// in-memory stand-in for the Emscripten FS. Run: node util/wasm/test_worlds.mjs
// The browser round trip lives in util/wasm/test_worlds_browser.py.

import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import {deflateRawSync} from "node:zlib";
import test from "node:test";

const source = await readFile(new URL("../../client/web/worlds.js", import.meta.url), "utf8");

const DIR = 0o040000;
const FILE = 0o100000;

function memoryFS() {
	const nodes = new Map([["/", {mode: DIR}]]);
	const parent = path => path.slice(0, path.lastIndexOf("/")) || "/";
	const need = path => {
		const node = nodes.get(path);
		if (!node)
			throw Object.assign(new Error("ENOENT: " + path), {code: "ENOENT"});
		return node;
	};
	const FS = {
		nodes,
		isDir: mode => (mode & 0o170000) === DIR,
		isFile: mode => (mode & 0o170000) === FILE,
		stat(path) {
			const node = need(path);
			return {mode: node.mode, size: node.data ? node.data.length : 4096, mtime: new Date(node.mtime || 0)};
		},
		mkdir(path) {
			if (nodes.has(path))
				throw new Error("EEXIST: " + path);
			if (!FS.isDir(need(parent(path)).mode))
				throw new Error("ENOTDIR");
			nodes.set(path, {mode: DIR});
		},
		rmdir(path) {
			if (FS.readdir(path).length > 2)
				throw new Error("ENOTEMPTY: " + path);
			nodes.delete(path);
		},
		unlink(path) {
			need(path);
			nodes.delete(path);
		},
		readdir(path) {
			need(path);
			const prefix = path === "/" ? "/" : path + "/";
			const names = [".", ".."];
			for (const key of nodes.keys())
				if (key !== path && key.startsWith(prefix) && !key.slice(prefix.length).includes("/"))
					names.push(key.slice(prefix.length));
			return names;
		},
		rename(from, to) {
			need(from);
			if (nodes.has(to))
				throw new Error("EEXIST: " + to);
			for (const [key, value] of [...nodes]) {
				if (key === from || key.startsWith(from + "/")) {
					nodes.delete(key);
					nodes.set(to + key.slice(from.length), value);
				}
			}
		},
		writeFile(path, value) {
			need(parent(path));
			const data = typeof value === "string" ? new TextEncoder().encode(value) : new Uint8Array(value);
			nodes.set(path, {mode: FILE, data, mtime: Date.now()});
		},
		readFile(path, options) {
			const data = need(path).data;
			return options && options.encoding === "utf8" ? new TextDecoder().decode(data) : data.slice();
		},
		open(path, flags) {
			if (flags === "w")
				FS.writeFile(path, new Uint8Array(0));
			return {path, node: need(path), position: 0};
		},
		read(stream, buffer, offset, length, position) {
			const slice = stream.node.data.subarray(position, position + length);
			buffer.set(slice, offset);
			return slice.length;
		},
		write(stream, buffer, offset, length) {
			const old = stream.node.data;
			const next = new Uint8Array(old.length + length);
			next.set(old);
			next.set(buffer.subarray(offset, offset + length), old.length);
			stream.node.data = next;
		},
		close() {}
	};
	return FS;
}

function harness(options = {}) {
	const FS = memoryFS();
	for (const dir of ["/home", "/home/web_user", "/home/web_user/.luanti", "/home/web_user/.luanti/worlds"])
		FS.mkdir(dir);
	const listeners = new Map();
	const events = [];
	const syncs = [];
	const window = {
		addEventListener(name, callback) {
			listeners.set(name, [...(listeners.get(name) || []), callback]);
		},
		removeEventListener(name, callback) {
			listeners.set(name, (listeners.get(name) || []).filter(entry => entry !== callback));
		},
		dispatchEvent(event) {
			events.push(event);
			for (const callback of listeners.get(event.type) || [])
				callback(event);
		}
	};
	const Module = {
		luantiWorldLimits: options.limits || {},
		luantiPersistence: {
			getState: () => ({state: "ready", fatal: false}),
			requestSync(request) {
				syncs.push(request);
				return Promise.resolve();
			}
		},
		luantiPackages: {
			manifest: () => Promise.resolve({
				games: {devtest: {packages: ["devtest"]}, luanti_web: {packages: ["luanti_web"]}},
				packages: {devtest: {label: "Development Test"}, luanti_web: {label: "Luanti Web Showcase"}}
			})
		},
		_luanti_engine_version: () => 0
	};
	const location = {href: "https://play.example/luanti.html", origin: "https://play.example"};
	const navigator = {storage: options.storage || null};
	const run = new Function("Module", "FS", "window", "location", "navigator", "fetch",
		"CustomEvent", source);
	run(Module, FS, window, location, navigator, options.fetch || (() => Promise.reject(new Error("no fetch"))),
		class {
			constructor(type, init) {
				this.type = type;
				this.detail = init.detail;
			}
		});
	return {FS, Module, window, library: Module.luantiPersistence.library, events, syncs};
}

const WORLDS = "/home/web_user/.luanti/worlds";

function fillWorld(FS, name, extra = {}) {
	const files = {
		"map.sqlite": new Uint8Array(3 * 1024 * 1024).map((_, i) => (i * 7919) % 251),
		"players.sqlite": new TextEncoder().encode("players".repeat(100)),
		"worldmods/demo/init.lua": new TextEncoder().encode("-- demo mod\n"),
		...extra
	};
	for (const [rel, data] of Object.entries(files)) {
		const parts = rel.split("/");
		let dir = WORLDS + "/" + name;
		for (const part of parts.slice(0, -1)) {
			dir += "/" + part;
			if (!FS.nodes.has(dir))
				FS.mkdir(dir);
		}
		FS.writeFile(WORLDS + "/" + name + "/" + rel, data);
	}
	return files;
}

// Minimal zip builder for hostile archives: each entry may lie about its
// sizes, CRC, flags or file type.
function craftZip(entries) {
	const chunks = [];
	const central = [];
	let offset = 0;
	const push = bytes => {
		chunks.push(bytes);
		offset += bytes.length;
	};
	for (const entry of entries) {
		const name = Buffer.from(entry.name);
		const raw = Buffer.from(entry.data || "");
		const method = entry.method ?? 0;
		const payload = entry.payload || (method === 8 ? deflateRawSync(raw) : raw);
		const crc = entry.crc ?? crc32(raw);
		const size = entry.size ?? raw.length;
		const local = Buffer.alloc(30);
		local.writeUInt32LE(0x04034b50, 0);
		local.writeUInt16LE(20, 4);
		local.writeUInt16LE(entry.flags || 0, 6);
		local.writeUInt16LE(method, 8);
		local.writeUInt32LE(crc, 14);
		local.writeUInt32LE(payload.length, 18);
		local.writeUInt32LE(size, 22);
		local.writeUInt16LE(name.length, 26);
		const at = offset;
		push(local);
		push(name);
		push(payload);
		const record = Buffer.alloc(46);
		record.writeUInt32LE(0x02014b50, 0);
		record.writeUInt16LE((3 << 8) | 20, 4);
		record.writeUInt16LE(20, 6);
		record.writeUInt16LE(entry.flags || 0, 8);
		record.writeUInt16LE(method, 10);
		record.writeUInt32LE(crc, 16);
		record.writeUInt32LE(payload.length, 20);
		record.writeUInt32LE(size, 24);
		record.writeUInt16LE(name.length, 28);
		record.writeUInt32LE(((entry.mode ?? 0o100644) << 16) >>> 0, 38);
		record.writeUInt32LE(at, 42);
		central.push(Buffer.concat([record, name]));
	}
	const cd = Buffer.concat(central);
	const end = Buffer.alloc(22);
	end.writeUInt32LE(0x06054b50, 0);
	end.writeUInt16LE(entries.length, 8);
	end.writeUInt16LE(entries.length, 10);
	end.writeUInt32LE(cd.length, 12);
	end.writeUInt32LE(offset, 16);
	return new Blob([...chunks, cd, end]);
}

function crc32(bytes) {
	let crc = ~0;
	for (const byte of bytes) {
		crc ^= byte;
		for (let k = 0; k < 8; k++)
			crc = crc & 1 ? 0xEDB88320 ^ (crc >>> 1) : crc >>> 1;
	}
	return ~crc >>> 0;
}

const WORLD_MT = "gameid = devtest\nbackend = sqlite3\n";

async function rejects(promise, pattern, code) {
	await assert.rejects(promise, error => {
		assert.match(error.message, pattern);
		if (code)
			assert.equal(error.code, code);
		return true;
	});
}

test("create, list, rename, duplicate and delete worlds of one game", async () => {
	const {FS, library, syncs} = harness();
	await library.create({name: "First", game: "devtest", seed: "hello world", creative: true, damage: false});
	await library.create({name: "Second", game: "devtest"});
	await rejects(library.create({name: "First", game: "devtest"}), /already exists/, "exists");
	await rejects(library.create({name: "../escape", game: "devtest"}), /World names/, "name");
	await rejects(library.create({name: ".hidden", game: "devtest"}), /World names/, "name");

	const mt = FS.readFile(WORLDS + "/First/world.mt", {encoding: "utf8"});
	assert.match(mt, /^world_name = First$/m);
	assert.match(mt, /^gameid = devtest$/m);
	assert.match(mt, /^creative_mode = true$/m);
	assert.match(mt, /^enable_damage = false$/m);
	assert.doesNotMatch(FS.readFile(WORLDS + "/Second/world.mt", {encoding: "utf8"}), /creative_mode/);
	assert.equal(FS.readFile(WORLDS + "/First/map_meta.txt", {encoding: "utf8"}), "seed = hello world\n[end_of_params]\n");

	// A directory without world.mt (e.g. a test sentinel) is not a world.
	FS.mkdir(WORLDS + "/not-a-world");
	assert.deepEqual((await library.list()).map(world => world.name), ["First", "Second"]);

	await library.rename("Second", "Renamed");
	assert.match(FS.readFile(WORLDS + "/Renamed/world.mt", {encoding: "utf8"}), /^world_name = Renamed$/m);
	assert.ok(!FS.nodes.has(WORLDS + "/Second"));

	fillWorld(FS, "First");
	const copy = await library.duplicate("First");
	assert.equal(copy.name, "First copy");
	assert.deepEqual(FS.readFile(WORLDS + "/First copy/map.sqlite"), FS.readFile(WORLDS + "/First/map.sqlite"));
	assert.equal((await library.duplicate("First")).name, "First copy (2)");

	// A world named by the engine's own main menu is still manageable.
	FS.mkdir(WORLDS + "/Engine's World!");
	FS.writeFile(WORLDS + "/Engine's World!/world.mt", WORLD_MT);
	assert.equal((await library.get("Engine's World!")).game, "devtest");
	assert.equal((await library.duplicate("Engine's World!")).name, "Engine's World copy");
	await library.remove("Engine's World!");
	await rejects(library.remove("../worlds"), /no world called/, "missing");

	await library.remove("First copy (2)");
	assert.ok(![...FS.nodes.keys()].some(key => key.startsWith(WORLDS + "/First copy (2)")));
	assert.ok(![...FS.nodes.keys()].some(key => key.includes("/.copy-")));
	assert.ok(syncs.length >= 6 && syncs.every(sync => sync.urgent));
});

test("export then import round-trips every byte", async () => {
	const {FS, library, events} = harness();
	await library.create({name: "Keep", game: "devtest"});
	const files = fillWorld(FS, "Keep", {"web_thumbnail.jpg": new Uint8Array([0xff, 0xd8, 1, 2])});
	FS.writeFile(WORLDS + "/Keep/world.mt",
		FS.readFile(WORLDS + "/Keep/world.mt", {encoding: "utf8"}) + "load_mod_demo = true\nload_mod_off = false\n");

	let progress = 0;
	const blob = await library.exportWorld("Keep", {onProgress: done => { progress = done; }});
	assert.ok(blob.size < 3 * 1024 * 1024, "deflated: " + blob.size);
	assert.ok(progress > 3 * 1024 * 1024);
	assert.equal(events.at(-1).detail.library.operation, "export");

	const plan = await library.inspectArchive(blob, {fileName: "Keep.luantiworld"});
	assert.equal(plan.name, "Keep");
	assert.equal(plan.game, "devtest");
	assert.equal(plan.gameTitle, "Development Test");
	assert.ok(plan.gameAvailable);
	assert.deepEqual(plan.content.map(item => item.type + ":" + item.name), ["game:devtest", "mod:demo"]);
	assert.equal(plan.content[1].bundled, true);
	assert.ok(!("_entries" in plan));

	await library.remove("Keep");
	const imported = await library.importArchive(blob);
	assert.equal(imported.name, "Keep");
	for (const [rel, data] of Object.entries(files))
		assert.deepEqual(FS.readFile(WORLDS + "/Keep/" + rel), new Uint8Array(data), rel);
	assert.equal(events.at(-1).detail.library.operation, "import");

	// Importing again never overwrites: it picks a fresh name.
	assert.equal((await library.importArchive(blob)).name, "Keep (2)");
	await rejects(library.importArchive(blob, {name: "Keep"}), /already exists/, "exists");
	assert.ok(![...FS.nodes.keys()].some(key => key.includes("/.import-")));
});

test("stored archives and empty files also round-trip", async () => {
	const {FS, library} = harness();
	await library.create({name: "Plain", game: "devtest"});
	FS.writeFile(WORLDS + "/Plain/empty.txt", new Uint8Array(0));
	const {ZipWriter, memorySink} = library._zip;
	const sink = memorySink();
	const writer = new ZipWriter(sink, {store: true});
	await writer.addBytes("world/world.mt", new TextEncoder().encode(WORLD_MT));
	await writer.addBytes("world/empty.txt", new Uint8Array(0));
	await writer.finish();
	const world = await library.importArchive(sink.blob(), {name: "Stored"});
	assert.equal(world.game, "devtest");
	assert.equal(FS.readFile(WORLDS + "/Stored/empty.txt").length, 0);
	assert.equal(FS.readFile(WORLDS + "/Stored/world.mt", {encoding: "utf8"}).match(/world_name/g).length, 1);
});

test("a zip of a bare world folder is accepted", async () => {
	const {FS, library} = harness();
	const zip = craftZip([
		{name: "My Old World/", data: ""},
		{name: "My Old World/world.mt", data: WORLD_MT + "world_name = My Old World\n", method: 8},
		{name: "My Old World/map.sqlite", data: "x".repeat(5000), method: 8}
	]);
	const plan = await library.inspectArchive(zip);
	assert.equal(plan.name, "My Old World");
	assert.equal(plan.files, 2);
	await library.importArchive(zip);
	assert.equal(FS.readFile(WORLDS + "/My Old World/map.sqlite").length, 5000);

	const flat = craftZip([{name: "world.mt", data: WORLD_MT}]);
	assert.equal((await library.inspectArchive(flat, {fileName: "flat.zip"})).name, "flat");
});

test("hostile and malformed archives are rejected before anything is written", async () => {
	const {FS, library} = harness({limits: {entries: 10, uncompressedBytes: 1 << 20, archiveBytes: 2 << 20}});
	const before = [...FS.nodes.keys()].sort();
	const cases = [
		[[{name: "world/world.mt", data: WORLD_MT}, {name: "world/../../evil.lua", data: "x"}], /points outside/, "unsafe-path"],
		[[{name: "world.mt", data: WORLD_MT}, {name: "/etc/passwd", data: "x"}], /absolute path/, "unsafe-path"],
		[[{name: "world.mt", data: WORLD_MT}, {name: "C:/evil", data: "x"}], /absolute path/, "unsafe-path"],
		[[{name: "world.mt", data: WORLD_MT}, {name: "mods\\..\\..\\x", data: "x"}], /backslash/, "unsafe-path"],
		[[{name: "world.mt", data: WORLD_MT}, {name: "a//b", data: "x"}], /points outside/, "unsafe-path"],
		[[{name: "world.mt", data: WORLD_MT}, {name: "link", data: "/etc", mode: 0o120777}], /symbolic link/, "unsafe-path"],
		[Array.from({length: 11}, (_, i) => ({name: "f" + i, data: "x"})), /11 entries; the limit is 10/, "too-large"],
		[[{name: "world.mt", data: WORLD_MT}, {name: "big", data: "", size: 2 << 20}], /unpacks to more than/, "too-large"],
		[[{name: "map.sqlite", data: "x"}], /no world\.mt/, "invalid"],
		[[{name: "world.mt", data: "backend = sqlite3\n"}], /valid game/, "invalid"],
		[[{name: "world.mt", data: "gameid = not_installed\n"}], /needs the game "not_installed"/, "missing-game"],
		[[{name: "world.mt", data: WORLD_MT, flags: 1}], /encrypted/, "invalid"],
		[[{name: "world.mt", data: WORLD_MT, method: 12}], /unsupported compression/, "invalid"],
		[[{name: "world.mt", data: WORLD_MT}, {name: "world.mt", data: WORLD_MT}], /twice/, "invalid"],
		[[{name: "luanti-world.json", data: "{\"format\": 99}"}, {name: "world/world.mt", data: WORLD_MT}], /unknown format/, "invalid"],
		[[{name: "luanti-world.json", data: "{nope"}, {name: "world/world.mt", data: WORLD_MT}], /not valid JSON/, "invalid"],
	];
	for (const [entries, pattern, code] of cases)
		await rejects(library.importArchive(craftZip(entries)), pattern, code);

	// Lies the directory tells are caught while extracting.
	const bomb = deflateRawSync(Buffer.alloc(900 * 1024));
	await rejects(library.importArchive(craftZip([
		{name: "world.mt", data: WORLD_MT},
		{name: "map.sqlite", method: 8, payload: bomb, size: 100, crc: 0}
	])), /larger than its directory says/, "invalid");
	await rejects(library.importArchive(craftZip([
		{name: "world.mt", data: WORLD_MT},
		{name: "map.sqlite", data: "hello", crc: 1234}
	])), /checksum mismatch/, "invalid");
	await rejects(library.importArchive(new Blob(["not a zip at all, just text padding it out"])), /not a zip/, "invalid");
	await rejects(library.importArchive(new Blob([new Uint8Array(3 << 20)])), /limit is 2\.0 MB/, "too-large");

	assert.deepEqual([...FS.nodes.keys()].sort(), before, "a rejected import leaves storage untouched");
});

test("entry names are checked component by component", () => {
	const {library} = harness();
	const check = library._zip.checkEntryPath;
	assert.equal(check("world/mods/x/init.lua"), "world/mods/x/init.lua");
	assert.equal(check("dir/"), "dir");
	assert.equal(check("..data/ok"), "..data/ok");
	for (const bad of ["..", "./x", "a/./b", "a/../b", "\u0000", "a\nb"])
		assert.throws(() => check(bad), /not imported/);
});

test("imports respect the storage quota", async () => {
	const storage = {
		estimate: () => Promise.resolve({usage: 1000, quota: 1100}),
		persisted: () => Promise.resolve(false),
		persist: () => Promise.resolve(true)
	};
	const {library} = harness({storage});
	await rejects(library.importArchive(craftZip([{name: "world.mt", data: WORLD_MT + "#".repeat(500)}])),
		/Not enough browser storage/, "quota");
	assert.deepEqual(await library.storage(), {usage: 1000, quota: 1100, persisted: false});
});

test("durable storage is requested once something is saved", async () => {
	let asked = 0;
	const storage = {
		estimate: () => Promise.resolve({usage: 0, quota: 1e9}),
		persisted: () => Promise.resolve(false),
		persist: () => { asked++; return Promise.resolve(true); }
	};
	const {library} = harness({storage});
	await library.create({name: "A", game: "devtest"});
	await library.create({name: "B", game: "devtest"});
	await new Promise(resolve => setTimeout(resolve, 0));
	assert.equal(asked, 1);
});

test("share links: HTTPS only, no credentials, size capped while streaming", async () => {
	const zip = craftZip([{name: "world.mt", data: WORLD_MT}]);
	const respond = (body, headers = {}, url = "https://cdn.example/w.luantiworld") =>
		Object.defineProperty(new Response(body, {headers}), "url", {value: url});
	let fetched = null;
	const {library} = harness({
		limits: {archiveBytes: 4096},
		fetch(url, init) {
			fetched = {url, init};
			if (url.includes("huge-declared"))
				return Promise.resolve(respond("x", {"Content-Length": "999999"}));
			if (url.includes("huge-stream"))
				return Promise.resolve(respond(new Uint8Array(8192)));
			if (url.includes("redirect"))
				return Promise.resolve(respond(zip, {}, "http://evil.example/w.luantiworld"));
			if (url.includes("missing"))
				return Promise.resolve(new Response("", {status: 404}));
			return Promise.resolve(respond(zip));
		}
	});
	await rejects(library.fetchShared("http://cdn.example/w.luantiworld"), /HTTPS/, "invalid-url");
	await rejects(library.fetchShared("javascript:alert(1)"), /HTTPS/, "invalid-url");
	await rejects(library.fetchShared("https://user:pw@cdn.example/w"), /user name/, "invalid-url");
	await rejects(library.fetchShared("https://cdn.example/huge-declared"), /limit is/, "too-large");
	await rejects(library.fetchShared("https://cdn.example/huge-stream"), /larger than/, "too-large");
	await rejects(library.fetchShared("https://cdn.example/redirect"), /redirected/, "invalid-url");
	await rejects(library.fetchShared("https://cdn.example/missing"), /HTTP 404/, "download");

	const shared = await library.fetchShared("https://cdn.example/Cool%20World.luantiworld");
	assert.equal(fetched.init.credentials, "omit");
	assert.equal(shared.host, "cdn.example");
	assert.equal(shared.plan.name, "Cool World");
	// Fetching only inspects; nothing is imported until asked.
	assert.deepEqual(await library.list(), []);
	await library.importArchive(shared.blob, {fileName: shared.fileName});
	assert.deepEqual((await library.list()).map(world => world.name), ["Cool World"]);

	// Same-origin links are allowed even without HTTPS (local test servers).
	await library.fetchShared("/worlds/w.luantiworld");
	assert.equal(fetched.url, "https://play.example/worlds/w.luantiworld");
});

test("operations wait for storage hydration", async () => {
	const h = harness();
	let state = {state: "loading", fatal: false};
	h.Module.luantiPersistence.getState = () => state;
	let listed = null;
	h.library.list().then(worlds => { listed = worlds; });
	await new Promise(resolve => setTimeout(resolve, 5));
	assert.equal(listed, null);
	state = {state: "ready", fatal: false};
	// persistence.js announces readiness with a luanti:persistence event.
	h.window.dispatchEvent({type: "luanti:persistence", detail: state});
	await new Promise(resolve => setTimeout(resolve, 0));
	assert.deepEqual(listed, []);

	state = {state: "error", fatal: true, error: "IDB blocked"};
	await rejects(h.library.list(), /Storage is unavailable: IDB blocked/, "storage");
});
