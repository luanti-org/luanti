// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// World library: list, create, rename, duplicate, delete, export and import
// singleplayer worlds stored under /home/web_user/.luanti/worlds.
//
// This is an Emscripten --pre-js that runs after persistence.js and installs
// Module.luantiPersistence.library. Every operation runs on the browser main
// thread against the Emscripten FS before the engine starts (the launcher
// hides itself once callMain runs), so no world is ever open in SQLite while
// it is copied, zipped or replaced.
//
// Archives (.luantiworld) are plain zip files:
//   luanti-world.json   manifest (format, engine, game, required content)
//   world/...           the world directory, world.mt at world/world.mt
// Import also accepts a zip of a bare world folder (world.mt at the root or
// in a single top-level directory), which is what an OS "compress folder"
// produces from a native Luanti world.
//
// Zip handling is done here with CompressionStream/DecompressionStream
// ("deflate-raw") rather than libzip: it streams entry by entry (a world is
// never held twice in memory), keeps the browser UI thread responsive, and
// needs no engine relink to change. Only stored and deflated entries without
// zip64 or encryption are read, which covers every mainstream zip tool.

(function() {
	"use strict";
	if (typeof window === "undefined" && typeof document === "undefined")
		return;

	var ROOT = "/home/web_user/.luanti";
	var WORLDS = ROOT + "/worlds";
	var LIBRARY_FILE = "web_library.json";
	var THUMBNAIL_FILE = "web_thumbnail.jpg";
	var MANIFEST_NAME = "luanti-world.json";
	var ARCHIVE_FORMAT = 1;
	var CHUNK = 1 << 20;
	var MAX_CONF_BYTES = 256 * 1024;

	// Caps for anything imported. The whole world lives in memory while IDBFS
	// is the backend, so these are well below the Wasm heap limit. An embedder
	// or test may lower them with Module.luantiWorldLimits.
	var limits = {
		"archiveBytes": 512 * 1024 * 1024,
		"uncompressedBytes": 1024 * 1024 * 1024,
		"entries": 50000,
		"pathDepth": 24,
		"nameBytes": 255
	};
	var overrides = Module["luantiWorldLimits"] || {};
	Object.keys(limits).forEach(function(key) {
		if (typeof overrides[key] === "number" && overrides[key] > 0)
			limits[key] = overrides[key];
	});

	function LibraryError(message, code) {
		var error = new Error(message);
		error.name = "LuantiWorldError";
		error["code"] = code || "invalid";
		return error;
	}

	var encoder = new TextEncoder();
	var decoder = new TextDecoder();

	function megabytes(bytes) {
		return (bytes / 1048576).toFixed(bytes < 10485760 ? 1 : 0) + " MB";
	}

	// ---------------------------------------------------------------- CRC-32

	var CRC_TABLE = (function() {
		var table = new Uint32Array(256);
		for (var n = 0; n < 256; n++) {
			var c = n;
			for (var k = 0; k < 8; k++)
				c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
			table[n] = c >>> 0;
		}
		return table;
	})();

	function crc32(crc, bytes) {
		crc = ~crc >>> 0;
		for (var i = 0; i < bytes.length; i++)
			crc = CRC_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >>> 8);
		return ~crc >>> 0;
	}

	// -------------------------------------------------------------- FS helpers

	function stat(path) {
		try {
			return FS.stat(path);
		} catch (_) {
			return null;
		}
	}

	function exists(path) {
		return stat(path) !== null;
	}

	function isDir(path) {
		var info = stat(path);
		return !!info && FS.isDir(info.mode);
	}

	function children(path) {
		return FS.readdir(path).filter(function(name) {
			return name !== "." && name !== "..";
		}).sort();
	}

	function mkdirs(path) {
		var parts = path.split("/").filter(Boolean);
		var current = "";
		parts.forEach(function(part) {
			current += "/" + part;
			if (!exists(current))
				FS.mkdir(current);
		});
	}

	function removeTree(path) {
		if (isDir(path)) {
			children(path).forEach(function(name) { removeTree(path + "/" + name); });
			FS.rmdir(path);
		} else if (exists(path)) {
			FS.unlink(path);
		}
	}

	// Files below `dir` as {rel, path, size, mtime}, directories first-seen
	// order, sorted for reproducible archives.
	function walk(dir, prefix, out) {
		out = out || [];
		children(dir).forEach(function(name) {
			var path = dir + "/" + name;
			var rel = prefix ? prefix + "/" + name : name;
			var info = FS.stat(path);
			if (FS.isDir(info.mode))
				walk(path, rel, out);
			else if (FS.isFile(info.mode))
				out.push({"rel": rel, "path": path, "size": info.size,
					"mtime": info.mtime ? +new Date(info.mtime) : Date.now()});
		});
		return out;
	}

	function readRange(path, position, length) {
		var stream = FS.open(path, "r");
		try {
			var buffer = new Uint8Array(length);
			var read = FS.read(stream, buffer, 0, length, position);
			return read === length ? buffer : buffer.subarray(0, read);
		} finally {
			FS.close(stream);
		}
	}

	function copyFile(from, to) {
		var size = FS.stat(from).size;
		var output = FS.open(to, "w");
		try {
			for (var position = 0; position < size; position += CHUNK) {
				var bytes = readRange(from, position, Math.min(CHUNK, size - position));
				FS.write(output, bytes, 0, bytes.length);
			}
		} finally {
			FS.close(output);
		}
	}

	function copyTree(from, to) {
		FS.mkdir(to);
		children(from).forEach(function(name) {
			var source = from + "/" + name;
			if (isDir(source))
				copyTree(source, to + "/" + name);
			else
				copyFile(source, to + "/" + name);
		});
	}

	function readText(path, maxBytes) {
		var info = stat(path);
		if (!info || FS.isDir(info.mode))
			return null;
		if (maxBytes && info.size > maxBytes)
			throw LibraryError(path.split("/").pop() + " is too large.", "invalid");
		return decoder.decode(FS.readFile(path));
	}

	// ------------------------------------------------- Luanti .conf (world.mt)

	function parseConf(text) {
		var values = {};
		String(text || "").split(/\r?\n/).forEach(function(line) {
			var match = /^\s*([^#=\s][^=]*?)\s*=\s*(.*?)\s*$/.exec(line);
			if (match)
				values[match[1]] = match[2];
		});
		return values;
	}

	function setConfValue(text, key, value) {
		var lines = String(text || "").split(/\r?\n/);
		if (lines.length && lines[lines.length - 1] === "")
			lines.pop();
		var found = false;
		lines = lines.map(function(line) {
			var match = /^\s*([^#=\s][^=]*?)\s*=/.exec(line);
			if (match && match[1] === key) {
				found = true;
				return key + " = " + value;
			}
			return line;
		});
		if (!found)
			lines.push(key + " = " + value);
		return lines.join("\n") + "\n";
	}

	function singleLine(value) {
		return String(value).replace(/[\r\n]+/g, " ").trim();
	}

	// ------------------------------------------------------------ world names

	// A world name is also its directory name, so it must be a single safe
	// path component. Leading dots are reserved for staging directories.
	var NAME_PATTERN = /^[\p{L}\p{N}][\p{L}\p{N} _.,()'+-]*$/u;
	var NAME_MAX = 48;

	function validateName(name) {
		name = String(name || "").trim();
		if (!name)
			throw LibraryError("Give the world a name.", "name");
		if (name.length > NAME_MAX)
			throw LibraryError("World names can be at most " + NAME_MAX + " characters.", "name");
		if (!NAME_PATTERN.test(name) || /[. ]$/.test(name))
			throw LibraryError("World names may use letters, numbers, spaces and _ . , ( ) ' + - " +
				"and must start with a letter or number.", "name");
		return name;
	}

	function sanitizeName(raw, fallback) {
		var name = String(raw || "").normalize("NFC")
			.replace(/[^\p{L}\p{N} _.,()'+-]+/gu, " ")
			.replace(/\s+/g, " ").trim()
			.replace(/^[^\p{L}\p{N}]+/u, "")
			.slice(0, NAME_MAX).replace(/[. ]+$/, "");
		return name || fallback || "Imported world";
	}

	function uniqueName(base) {
		base = sanitizeName(base);
		if (!exists(WORLDS + "/" + base))
			return base;
		var stem = base.replace(/ \(\d+\)$/, "").slice(0, NAME_MAX - 6).replace(/[. ]+$/, "");
		for (var n = 2; n < 1000; n++) {
			var candidate = stem + " (" + n + ")";
			if (!exists(WORLDS + "/" + candidate))
				return candidate;
		}
		throw LibraryError("Too many worlds named " + base + ".", "name");
	}

	function worldPath(name) {
		return WORLDS + "/" + validateName(name);
	}

	// Worlds made elsewhere (the engine's own main menu, an older launcher)
	// may have names the New world dialog would not allow; they only need to
	// be one safe path component to be managed.
	function requireWorld(name) {
		name = String(name || "");
		if (!name || name.charAt(0) === "." || name.indexOf("/") !== -1 || name.indexOf("\0") !== -1)
			throw LibraryError("There is no world called " + name + ".", "missing");
		var path = WORLDS + "/" + name;
		if (!exists(path + "/world.mt"))
			throw LibraryError("There is no world called " + name + ".", "missing");
		return path;
	}

	function requireFree(name) {
		var path = worldPath(name);
		if (exists(path))
			throw LibraryError("A world called " + name + " already exists.", "exists");
		return path;
	}

	// ------------------------------------------------------- library metadata

	function readLibraryMeta(path) {
		try {
			var text = readText(path + "/" + LIBRARY_FILE, 16384);
			var value = text ? JSON.parse(text) : {};
			return value && typeof value === "object" ? value : {};
		} catch (_) {
			return {};
		}
	}

	function writeLibraryMeta(path, changes) {
		var meta = readLibraryMeta(path);
		Object.keys(changes).forEach(function(key) { meta[key] = changes[key]; });
		FS.writeFile(path + "/" + LIBRARY_FILE, JSON.stringify(meta));
		return meta;
	}

	function describe(name) {
		var path = WORLDS + "/" + name;
		var conf = parseConf(readText(path + "/world.mt", MAX_CONF_BYTES));
		var files = walk(path, "");
		var size = 0;
		var newest = 0;
		files.forEach(function(file) {
			size += file.size;
			if (file.rel !== THUMBNAIL_FILE && file.rel !== LIBRARY_FILE)
				newest = Math.max(newest, file.mtime);
		});
		var meta = readLibraryMeta(path);
		return {
			"name": name,
			"path": path,
			"game": conf["gameid"] || "",
			"creative": conf["creative_mode"] === "true",
			"damage": conf["enable_damage"] === "true",
			"size": size,
			"files": files.length,
			"created": Number(meta["created"]) || null,
			"lastPlayed": Number(meta["lastPlayed"]) || null,
			"modified": newest || null,
			"hasThumbnail": exists(path + "/" + THUMBNAIL_FILE)
		};
	}

	// ------------------------------------------------------------ readiness

	var persistence = Module["luantiPersistence"];

	function storageState() {
		return persistence && persistence["getState"] ? persistence["getState"]() : {"state": "ready"};
	}

	function whenReady() {
		var state = storageState();
		if (state["fatal"])
			return Promise.reject(LibraryError("Storage is unavailable: " + (state["error"] || "unknown error"), "storage"));
		if (state["state"] !== "loading")
			return Promise.resolve();
		return new Promise(function(resolve, reject) {
			window.addEventListener("luanti:persistence", function listener(event) {
				var detail = /** @type {CustomEvent} */ (event).detail || {};
				if (detail["fatal"]) {
					window.removeEventListener("luanti:persistence", listener);
					reject(LibraryError("Storage is unavailable: " + (detail["error"] || "unknown error"), "storage"));
				} else if (detail["state"] !== "loading") {
					window.removeEventListener("luanti:persistence", listener);
					resolve(undefined);
				}
			});
		});
	}

	function save(reason) {
		if (!persistence || !persistence["requestSync"])
			return Promise.resolve();
		return persistence["requestSync"]({"reason": reason || "library", "urgent": true});
	}

	function emit(operation, detail) {
		var payload = storageState();
		payload["library"] = Object.assign({"operation": operation}, detail || {});
		var event;
		if (typeof CustomEvent === "function")
			event = new CustomEvent("luanti:persistence", {"detail": payload});
		else {
			event = document.createEvent("CustomEvent");
			event.initCustomEvent("luanti:persistence", false, false, payload);
		}
		window.dispatchEvent(event);
	}

	// Saves made through the library go through the ordinary persistence
	// queue; the change is visible to every listener once it is on disk.
	function commit(operation, detail) {
		return save("library-" + operation).then(function() {
			emit(operation, detail);
			requestDurableStorage();
			return detail;
		});
	}

	// ------------------------------------------------------ browser storage

	var persistRequested = false;

	// Ask the browser not to evict this origin once there is something worth
	// keeping. Browsers decide silently (Chrome grants it to engaged sites,
	// Firefox may prompt), so this is requested once per page and never
	// treated as an error.
	function requestDurableStorage() {
		if (persistRequested || typeof navigator === "undefined" || !navigator.storage ||
				typeof navigator.storage.persist !== "function")
			return Promise.resolve(false);
		persistRequested = true;
		return navigator.storage.persisted().then(function(already) {
			return already || navigator.storage.persist();
		}).catch(function() { return false; });
	}

	function storageInfo() {
		var storage = typeof navigator !== "undefined" ? navigator.storage : null;
		if (!storage || typeof storage.estimate !== "function")
			return Promise.resolve({"usage": null, "quota": null, "persisted": null});
		return Promise.all([
			storage.estimate(),
			typeof storage.persisted === "function" ? storage.persisted() : Promise.resolve(null)
		]).then(function(results) {
			return {"usage": results[0].usage || 0, "quota": results[0].quota || 0, "persisted": results[1]};
		}, function() {
			return {"usage": null, "quota": null, "persisted": null};
		});
	}

	if (window.addEventListener) {
		window.addEventListener("luanti:persistence", function(event) {
			var detail = /** @type {CustomEvent} */ (event).detail || {};
			if (detail["state"] === "saved" && !detail["library"])
				requestDurableStorage();
		});
	}

	// ----------------------------------------------------------- games

	function availableGames() {
		var bundled = Module["luantiPackages"] && Module["luantiPackages"]["manifest"] ?
			Module["luantiPackages"]["manifest"]().catch(function() { return null; }) :
			Promise.resolve(null);
		return bundled.then(function(manifest) {
			var games = {};
			if (manifest && manifest["games"]) {
				Object.keys(manifest["games"]).forEach(function(id) {
					var first = (manifest["games"][id]["packages"] || [])[0];
					var info = first && manifest["packages"] ? manifest["packages"][first] : null;
					games[id] = {"id": id, "title": info && info["label"] || id, "source": "bundled"};
				});
			}
			[ROOT + "/games", "/games"].forEach(function(dir) {
				if (!isDir(dir))
					return;
				children(dir).forEach(function(id) {
					if (games[id] || !exists(dir + "/" + id + "/game.conf"))
						return;
					var conf = {};
					try {
						conf = parseConf(readText(dir + "/" + id + "/game.conf", MAX_CONF_BYTES));
					} catch (_) {}
					games[id] = {"id": id, "title": conf["title"] || id,
						"source": dir === "/games" ? "bundled" : "installed"};
				});
			});
			return Object.keys(games).sort().map(function(id) { return games[id]; });
		});
	}

	function engineVersion() {
		try {
			var fn = Module["_luanti_engine_version"];
			if (typeof fn === "function" && typeof UTF8ToString === "function")
				return UTF8ToString(fn());
		} catch (_) {}
		return "unknown";
	}

	// ------------------------------------------------------------ zip writer

	function dosDateTime(ms) {
		var date = new Date(ms);
		var year = Math.max(1980, date.getFullYear());
		return {
			"time": (date.getHours() << 11) | (date.getMinutes() << 5) | (date.getSeconds() >> 1),
			"date": ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate()
		};
	}

	function canDeflate() {
		return typeof CompressionStream === "function";
	}

	// sink: {write(Uint8Array) -> Promise, close() -> Promise}
	function ZipWriter(sink, options) {
		this.sink = sink;
		this.offset = 0;
		this.central = [];
		this.deflate = canDeflate() && !(options && options["store"]);
	}

	ZipWriter.prototype.write = function(bytes) {
		this.offset += bytes.length;
		if (this.offset > 0xFFFFFFFF)
			throw LibraryError("The world is too large to export (over 4 GB).", "too-large");
		return this.sink.write(bytes);
	};

	// readChunk(position, length) -> Uint8Array
	ZipWriter.prototype.add = async function(name, size, readChunk, mtime) {
		if (this.central.length >= 0xFFFF)
			throw LibraryError("The world has too many files to export.", "too-large");
		var nameBytes = encoder.encode(name);
		var method = this.deflate && size > 0 ? 8 : 0;
		var stamp = dosDateTime(mtime || Date.now());
		var flags = 0x0808; // UTF-8 names, sizes in the data descriptor
		var headerOffset = this.offset;

		var header = new DataView(new ArrayBuffer(30));
		header.setUint32(0, 0x04034b50, true);
		header.setUint16(4, 20, true);
		header.setUint16(6, flags, true);
		header.setUint16(8, method, true);
		header.setUint16(10, stamp["time"], true);
		header.setUint16(12, stamp["date"], true);
		header.setUint16(26, nameBytes.length, true);
		await this.write(new Uint8Array(header.buffer));
		await this.write(nameBytes);

		var crc = 0;
		var compressed = 0;
		var self = this;
		if (method === 8) {
			var stream = new CompressionStream("deflate-raw");
			var input = stream.writable.getWriter();
			var output = stream.readable.getReader();
			var pump = (async function() {
				for (;;) {
					var next = await output.read();
					if (next.done)
						return;
					compressed += next.value.length;
					await self.write(next.value);
				}
			})();
			try {
				for (var position = 0; position < size; position += CHUNK) {
					var chunk = readChunk(position, Math.min(CHUNK, size - position));
					crc = crc32(crc, chunk);
					await input.write(chunk);
				}
				await input.close();
			} catch (error) {
				input.abort(error).catch(function() {});
				throw error;
			}
			await pump;
		} else {
			for (var at = 0; at < size; at += CHUNK) {
				var piece = readChunk(at, Math.min(CHUNK, size - at));
				crc = crc32(crc, piece);
				compressed += piece.length;
				await this.write(piece);
			}
		}

		var descriptor = new DataView(new ArrayBuffer(16));
		descriptor.setUint32(0, 0x08074b50, true);
		descriptor.setUint32(4, crc, true);
		descriptor.setUint32(8, compressed, true);
		descriptor.setUint32(12, size, true);
		await this.write(new Uint8Array(descriptor.buffer));

		this.central.push({"name": nameBytes, "method": method, "flags": flags, "stamp": stamp,
			"crc": crc, "compressed": compressed, "size": size, "offset": headerOffset});
	};

	ZipWriter.prototype.addBytes = function(name, bytes, mtime) {
		return this.add(name, bytes.length, function(position, length) {
			return bytes.subarray(position, position + length);
		}, mtime);
	};

	ZipWriter.prototype.finish = async function() {
		var start = this.offset;
		for (var i = 0; i < this.central.length; i++) {
			var entry = this.central[i];
			var record = new DataView(new ArrayBuffer(46));
			record.setUint32(0, 0x02014b50, true);
			record.setUint16(4, (3 << 8) | 20, true); // made by Unix, zip 2.0
			record.setUint16(6, 20, true);
			record.setUint16(8, entry["flags"], true);
			record.setUint16(10, entry["method"], true);
			record.setUint16(12, entry["stamp"]["time"], true);
			record.setUint16(14, entry["stamp"]["date"], true);
			record.setUint32(16, entry["crc"], true);
			record.setUint32(20, entry["compressed"], true);
			record.setUint32(24, entry["size"], true);
			record.setUint16(28, entry["name"].length, true);
			record.setUint32(38, (0o100644 << 16) >>> 0, true); // regular file, rw-r--r--
			record.setUint32(42, entry["offset"], true);
			await this.write(new Uint8Array(record.buffer));
			await this.write(entry["name"]);
		}
		var end = new DataView(new ArrayBuffer(22));
		end.setUint32(0, 0x06054b50, true);
		end.setUint16(8, this.central.length, true);
		end.setUint16(10, this.central.length, true);
		end.setUint32(12, this.offset - start, true);
		end.setUint32(16, start, true);
		await this.write(new Uint8Array(end.buffer));
		await this.sink.close();
	};

	function memorySink() {
		var parts = [];
		var sink = {
			"write": function(bytes) {
				// The writer may reuse nothing, but copy anyway: a subarray of a
				// caller's buffer must not be retained.
				parts.push(bytes.slice());
				return Promise.resolve();
			},
			"close": function() { return Promise.resolve(); },
			"blob": function() { return new Blob(parts, {"type": "application/zip"}); }
		};
		return sink;
	}

	// A FileSystemWritableFileStream (File System Access API) or a WHATWG
	// WritableStream. Writes are batched so a world of small files does not
	// turn into thousands of tiny disk writes.
	function streamSink(writable) {
		var writer = typeof writable.getWriter === "function" ? writable.getWriter() : null;
		var pending = [];
		var pendingBytes = 0;
		function flush() {
			if (!pendingBytes)
				return Promise.resolve();
			var blob = new Blob(pending);
			pending = [];
			pendingBytes = 0;
			return writer ? writer.write(blob) : writable.write(blob);
		}
		return {
			"write": function(bytes) {
				pending.push(bytes.slice());
				pendingBytes += bytes.length;
				return pendingBytes >= 4 * CHUNK ? flush() : Promise.resolve();
			},
			"close": function() {
				return flush().then(function() { return writer ? writer.close() : writable.close(); });
			}
		};
	}

	// ------------------------------------------------------------ zip reader

	async function blobBytes(blob, start, end) {
		return new Uint8Array(await blob.slice(start, end).arrayBuffer());
	}

	function readError(message) {
		return LibraryError("This file is not a usable world archive: " + message, "invalid");
	}

	// Returns the central directory as [{name, method, flags, crc, compressed,
	// size, offset, directory, symlink}], enforcing the size and entry caps
	// before any entry is decompressed.
	async function readDirectory(blob) {
		var size = blob.size;
		if (size > limits["archiveBytes"])
			throw LibraryError("The archive is " + megabytes(size) + "; the limit is " +
				megabytes(limits["archiveBytes"]) + ".", "too-large");
		if (size < 22)
			throw readError("it is too small to be a zip file.");
		var tailStart = Math.max(0, size - 22 - 0xFFFF);
		var tail = await blobBytes(blob, tailStart, size);
		var view = new DataView(tail.buffer);
		var eocd = -1;
		for (var i = tail.length - 22; i >= 0; i--) {
			if (view.getUint32(i, true) === 0x06054b50) {
				eocd = i;
				break;
			}
		}
		if (eocd < 0)
			throw readError("it is not a zip file.");
		var disk = view.getUint16(eocd + 4, true);
		var cdDisk = view.getUint16(eocd + 6, true);
		var count = view.getUint16(eocd + 10, true);
		var cdSize = view.getUint32(eocd + 12, true);
		var cdOffset = view.getUint32(eocd + 16, true);
		if (disk !== 0 || cdDisk !== 0)
			throw readError("split archives are not supported.");
		if (count === 0xFFFF || cdSize === 0xFFFFFFFF || cdOffset === 0xFFFFFFFF)
			throw readError("zip64 archives are not supported.");
		if (count > limits["entries"])
			throw LibraryError("The archive has " + count + " entries; the limit is " +
				limits["entries"] + ".", "too-large");
		if (cdOffset + cdSize > tailStart + eocd)
			throw readError("its directory is damaged.");

		var cd = await blobBytes(blob, cdOffset, cdOffset + cdSize);
		var cdView = new DataView(cd.buffer);
		var entries = [];
		var total = 0;
		var position = 0;
		for (var n = 0; n < count; n++) {
			if (position + 46 > cd.length || cdView.getUint32(position, true) !== 0x02014b50)
				throw readError("its directory is damaged.");
			var madeBy = cdView.getUint16(position + 4, true) >> 8;
			var flags = cdView.getUint16(position + 8, true);
			var method = cdView.getUint16(position + 10, true);
			var crc = cdView.getUint32(position + 16, true);
			var compressed = cdView.getUint32(position + 20, true);
			var uncompressed = cdView.getUint32(position + 24, true);
			var nameLength = cdView.getUint16(position + 28, true);
			var extraLength = cdView.getUint16(position + 30, true);
			var commentLength = cdView.getUint16(position + 32, true);
			var external = cdView.getUint32(position + 38, true);
			var offset = cdView.getUint32(position + 42, true);
			if (position + 46 + nameLength > cd.length)
				throw readError("its directory is damaged.");
			if (nameLength > limits["nameBytes"] * limits["pathDepth"])
				throw readError("an entry name is too long.");
			var name = decoder.decode(cd.subarray(position + 46, position + 46 + nameLength));
			position += 46 + nameLength + extraLength + commentLength;

			if (compressed === 0xFFFFFFFF || uncompressed === 0xFFFFFFFF || offset === 0xFFFFFFFF)
				throw readError("zip64 entries are not supported.");
			if (flags & 0x41)
				throw readError("encrypted entries are not supported.");
			var directory = name.endsWith("/");
			var unixMode = madeBy === 3 ? external >>> 16 : 0;
			var symlink = (unixMode & 0o170000) === 0o120000;
			if (!directory && method !== 0 && method !== 8)
				throw readError("\"" + name + "\" uses an unsupported compression method.");
			total += uncompressed;
			if (total > limits["uncompressedBytes"])
				throw LibraryError("The world unpacks to more than " +
					megabytes(limits["uncompressedBytes"]) + ", the import limit.", "too-large");
			entries.push({"name": name, "method": method, "flags": flags, "crc": crc,
				"compressed": compressed, "size": uncompressed, "offset": offset,
				"directory": directory, "symlink": symlink});
		}
		return entries;
	}

	// Streams one entry's bytes to onChunk, checking the declared size and the
	// CRC. A deflate stream that inflates past its declared size (a zip bomb
	// with a lying directory) is cut off as soon as it does.
	async function extractEntry(blob, entry, onChunk) {
		var header = await blobBytes(blob, entry["offset"], entry["offset"] + 30);
		var view = new DataView(header.buffer);
		if (header.length < 30 || view.getUint32(0, true) !== 0x04034b50)
			throw readError("\"" + entry["name"] + "\" is damaged.");
		var start = entry["offset"] + 30 + view.getUint16(26, true) + view.getUint16(28, true);
		if (start + entry["compressed"] > blob.size)
			throw readError("\"" + entry["name"] + "\" is truncated.");
		var stream = blob.slice(start, start + entry["compressed"]).stream();
		if (entry["method"] === 8) {
			if (typeof DecompressionStream !== "function")
				throw LibraryError("This browser cannot unpack compressed archives.", "unsupported");
			stream = stream.pipeThrough(new DecompressionStream("deflate-raw"));
		}
		var reader = stream.getReader();
		var produced = 0;
		var crc = 0;
		try {
			for (;;) {
				var next = await reader.read();
				if (next.done)
					break;
				produced += next.value.length;
				if (produced > entry["size"])
					throw readError("\"" + entry["name"] + "\" is larger than its directory says.");
				crc = crc32(crc, next.value);
				await onChunk(next.value);
			}
		} catch (error) {
			reader.cancel().catch(function() {});
			if (error && error.name === "LuantiWorldError")
				throw error;
			throw readError("\"" + entry["name"] + "\" could not be decompressed.");
		}
		if (produced !== entry["size"] || crc !== entry["crc"])
			throw readError("\"" + entry["name"] + "\" is corrupt (checksum mismatch).");
	}

	async function entryText(blob, entry, maxBytes) {
		if (entry["size"] > maxBytes)
			throw readError(entry["name"] + " is too large.");
		var parts = [];
		await extractEntry(blob, entry, function(chunk) { parts.push(chunk.slice()); });
		return decoder.decode(await new Blob(parts).arrayBuffer());
	}

	// Archive paths are untrusted. Reject anything that is absolute, climbs
	// out with "..", uses backslashes or drive letters, or contains control
	// characters, instead of trying to repair it.
	function checkEntryPath(name) {
		var display = JSON.stringify(name.length > 80 ? name.slice(0, 80) + "…" : name);
		var unsafe = function(reason) {
			return LibraryError("Archive entry " + display + " " + reason + "; the archive was not imported.", "unsafe-path");
		};
		// eslint-disable-next-line no-control-regex
		if (/[\u0000-\u001f\u007f\\]/.test(name))
			throw unsafe("contains a backslash or control character");
		if (name.startsWith("/") || /^[A-Za-z]:/.test(name))
			throw unsafe("is an absolute path");
		var parts = name.replace(/\/$/, "").split("/");
		if (parts.length > limits["pathDepth"])
			throw unsafe("is nested too deeply");
		parts.forEach(function(part) {
			if (part === "" || part === "." || part === "..")
				throw unsafe("points outside the world folder");
			if (encoder.encode(part).length > limits["nameBytes"])
				throw unsafe("has a file name that is too long");
		});
		return parts.join("/");
	}

	// ------------------------------------------------------- archive layout

	// Reads an archive's directory and decides what it would import, without
	// writing anything to storage.
	async function inspect(blob, options) {
		options = options || {};
		var entries = await readDirectory(blob);
		var seen = {};
		entries.forEach(function(entry) {
			entry["path"] = checkEntryPath(entry["name"]);
			if (entry["symlink"])
				throw LibraryError("Archive entry " + JSON.stringify(entry["name"]) +
					" is a symbolic link; the archive was not imported.", "unsafe-path");
			if (!entry["directory"]) {
				var key = entry["path"].toLowerCase();
				if (seen[key])
					throw readError("it contains " + JSON.stringify(entry["path"]) + " twice.");
				seen[key] = true;
			}
		});
		var files = entries.filter(function(entry) { return !entry["directory"]; });
		var byPath = {};
		files.forEach(function(entry) { byPath[entry["path"]] = entry; });

		var manifest = null;
		var prefix = null;
		var warnings = [];
		if (byPath[MANIFEST_NAME]) {
			try {
				manifest = JSON.parse(await entryText(blob, byPath[MANIFEST_NAME], 65536));
			} catch (error) {
				if (error && error.name === "LuantiWorldError")
					throw error;
				throw readError(MANIFEST_NAME + " is not valid JSON.");
			}
			if (!manifest || typeof manifest !== "object" || manifest["format"] !== ARCHIVE_FORMAT)
				throw readError("it was written in an unknown format (" +
					JSON.stringify(manifest && manifest["format"]) + ").");
			prefix = "world/";
		} else if (byPath["world.mt"]) {
			prefix = "";
		} else {
			var tops = {};
			files.forEach(function(entry) { tops[entry["path"].split("/")[0]] = true; });
			var names = Object.keys(tops);
			if (names.length === 1 && byPath[names[0] + "/world.mt"])
				prefix = names[0] + "/";
		}
		if (prefix === null || !byPath[prefix + "world.mt"])
			throw readError("it does not contain a Luanti world (no world.mt).");

		var worldFiles = files.filter(function(entry) {
			return entry["path"].startsWith(prefix) && entry["path"] !== MANIFEST_NAME;
		});
		var ignored = files.length - worldFiles.length - (manifest ? 1 : 0);
		if (ignored > 0)
			warnings.push(ignored + " file(s) outside the world folder will be skipped.");

		var conf = parseConf(await entryText(blob, byPath[prefix + "world.mt"], MAX_CONF_BYTES));
		var game = conf["gameid"] || "";
		if (!/^[a-z0-9_-]+$/.test(game))
			throw readError("world.mt does not name a valid game.");
		if (manifest && manifest["game"] && manifest["game"] !== game)
			warnings.push("The manifest says game " + manifest["game"] + " but world.mt says " + game + "; using " + game + ".");

		var games = options["games"] || await availableGames();
		var gameInfo = games.filter(function(entry) { return entry["id"] === game; })[0] || null;
		var engine = engineVersion();
		if (manifest && manifest["engine"] && engine !== "unknown" && manifest["engine"] !== engine)
			warnings.push("Exported from Luanti " + manifest["engine"] + "; this is " + engine + ".");

		var bytes = 0;
		worldFiles.forEach(function(entry) { bytes += entry["size"]; });
		var suggested = sanitizeName((manifest && manifest["name"]) || conf["world_name"] ||
			(prefix && prefix !== "world/" ? prefix.slice(0, -1) : "") ||
			String(options["fileName"] || "").replace(/\.(luantiworld|zip)$/i, ""));
		return {
			"name": suggested,
			"game": game,
			"gameTitle": gameInfo ? gameInfo["title"] : game,
			"gameAvailable": !!gameInfo,
			"engine": manifest && manifest["engine"] || null,
			"exported": manifest && manifest["exported"] || null,
			"content": manifest && Array.isArray(manifest["content"]) ? manifest["content"] : [],
			"archiveBytes": blob.size,
			"uncompressedBytes": bytes,
			"files": worldFiles.length,
			"warnings": warnings,
			"_entries": worldFiles,
			"_prefix": prefix
		};
	}

	/** @returns {any} */
	function publicPlan(plan) {
		var copy = {};
		Object.keys(plan).forEach(function(key) {
			if (key.charAt(0) !== "_")
				copy[key] = plan[key];
		});
		return copy;
	}

	// --------------------------------------------------------------- import

	async function importArchive(blob, options) {
		options = options || {};
		await whenReady();
		var plan = await inspect(blob, options);
		if (!plan["gameAvailable"])
			throw LibraryError("This world needs the game \"" + plan["game"] +
				"\", which is not available in this browser.", "missing-game");

		var info = await storageInfo();
		if (info["quota"] && info["usage"] !== null &&
				info["quota"] - info["usage"] < plan["uncompressedBytes"] * 1.1)
			throw LibraryError("Not enough browser storage: the world needs " +
				megabytes(plan["uncompressedBytes"]) + " and " +
				megabytes(Math.max(0, info["quota"] - info["usage"])) + " is free.", "quota");

		var name = options["name"] ? validateName(options["name"]) : uniqueName(plan["name"]);
		var target = requireFree(name);
		mkdirs(WORLDS);
		var staging = WORLDS + "/.import-" + Date.now().toString(36) +
			Math.random().toString(36).slice(2, 8);
		var progress = typeof options["onProgress"] === "function" ? options["onProgress"] : null;
		var written = 0;
		try {
			FS.mkdir(staging);
			for (var i = 0; i < plan["_entries"].length; i++) {
				var entry = plan["_entries"][i];
				var rel = entry["path"].slice(plan["_prefix"].length);
				var dest = staging + "/" + rel;
				mkdirs(dest.slice(0, dest.lastIndexOf("/")));
				var output = FS.open(dest, "w");
				try {
					await extractEntry(blob, entry, function(chunk) {
						FS.write(output, chunk, 0, chunk.length);
						written += chunk.length;
						if (progress)
							progress(written, plan["uncompressedBytes"]);
					});
				} finally {
					FS.close(output);
				}
			}
			var mtPath = staging + "/world.mt";
			FS.writeFile(mtPath, setConfValue(readText(mtPath, MAX_CONF_BYTES), "world_name", name));
			var now = Date.now();
			FS.writeFile(staging + "/" + LIBRARY_FILE, JSON.stringify({
				"created": now, "imported": now, "lastPlayed": null,
				"source": options["source"] ? String(options["source"]).slice(0, 512) : null
			}));
			if (exists(target))
				throw LibraryError("A world called " + name + " already exists.", "exists");
			FS.rename(staging, target);
		} catch (error) {
			try {
				removeTree(staging);
			} catch (_) {}
			throw error;
		}
		var result = describe(name);
		await commit("import", {"world": name});
		return result;
	}

	// --------------------------------------------------------------- export

	async function exportWorld(name, options) {
		options = options || {};
		await whenReady();
		var path = requireWorld(name);
		var world = describe(name);
		var conf = parseConf(readText(path + "/world.mt", MAX_CONF_BYTES));
		var files = walk(path, "").filter(function(file) { return file.rel !== LIBRARY_FILE; });
		var content = [{"type": "game", "name": world["game"]}];
		Object.keys(conf).sort().forEach(function(key) {
			var match = /^load_mod_(.+)$/.exec(key);
			if (match && conf[key] !== "false" && conf[key] !== "")
				content.push({"type": "mod", "name": match[1],
					"bundled": isDir(path + "/worldmods/" + match[1])});
		});
		var manifest = {
			"format": ARCHIVE_FORMAT,
			"kind": "luanti-world",
			"name": name,
			"game": world["game"],
			"engine": engineVersion(),
			"exported": new Date().toISOString(),
			"files": files.length,
			"bytes": world["size"],
			"content": content
		};
		var memory = options["writable"] ? null : memorySink();
		var sink = memory || streamSink(options["writable"]);
		var writer = new ZipWriter(sink, options);
		var progress = typeof options["onProgress"] === "function" ? options["onProgress"] : null;
		var done = 0;
		try {
			await writer.addBytes(MANIFEST_NAME, encoder.encode(JSON.stringify(manifest, null, "\t") + "\n"));
			for (var i = 0; i < files.length; i++) {
				var file = files[i];
				await writer.add("world/" + file.rel, file.size, function(position, length) {
					var bytes = readRange(file.path, position, length);
					done += bytes.length;
					if (progress)
						progress(done, world["size"]);
					return bytes;
				}, file.mtime);
			}
			await writer.finish();
		} catch (error) {
			if (!memory && options["writable"] && typeof options["writable"].abort === "function")
				options["writable"].abort().catch(function() {});
			throw error;
		}
		emit("export", {"world": name, "bytes": writer.offset});
		return memory ? memory["blob"]() : null;
	}

	// ----------------------------------------------------------- share links

	// Downloads a shared .luantiworld for inspection. Only HTTPS (or this
	// page's own origin) is accepted, credentials are never sent, and the
	// download stops at the archive cap whatever Content-Length claims.
	async function fetchShared(rawUrl, options) {
		options = options || {};
		var url;
		try {
			url = new URL(String(rawUrl || ""), location.href);
		} catch (_) {
			throw LibraryError("The shared world link is not a valid URL.", "invalid-url");
		}
		var allowed = function(candidate) {
			return candidate.protocol === "https:" || candidate.origin === location.origin;
		};
		if (!allowed(url))
			throw LibraryError("Shared worlds must be served over HTTPS.", "invalid-url");
		if (url.username || url.password)
			throw LibraryError("Shared world links cannot contain a user name or password.", "invalid-url");
		var response = await fetch(url.href, {"mode": "cors", "credentials": "omit",
			"redirect": "follow", "signal": options["signal"]});
		if (response.url && !allowed(new URL(response.url)))
			throw LibraryError("The shared world link redirected away from HTTPS.", "invalid-url");
		if (!response.ok)
			throw LibraryError("The shared world could not be downloaded (HTTP " + response.status + ").", "download");
		var declared = Number(response.headers.get("Content-Length")) || 0;
		if (declared > limits["archiveBytes"])
			throw LibraryError("The shared world is " + megabytes(declared) + "; the limit is " +
				megabytes(limits["archiveBytes"]) + ".", "too-large");
		var parts = [];
		var received = 0;
		var reader = response.body.getReader();
		for (;;) {
			var next = await reader.read();
			if (next.done)
				break;
			received += next.value.length;
			if (received > limits["archiveBytes"]) {
				reader.cancel().catch(function() {});
				throw LibraryError("The shared world is larger than the " +
					megabytes(limits["archiveBytes"]) + " limit.", "too-large");
			}
			parts.push(next.value);
			if (typeof options["onProgress"] === "function")
				options["onProgress"](received, declared);
		}
		var blob = new Blob(parts, {"type": "application/zip"});
		var fileName = decodeURIComponent(url.pathname.split("/").pop() || "");
		var plan = await inspect(blob, {"fileName": fileName});
		return {"blob": blob, "url": url.href, "host": url.host, "fileName": fileName, "plan": publicPlan(plan)};
	}

	// ------------------------------------------------------------ library API

	var library = {
		"limits": function() { return Object.assign({}, limits); },
		"ready": whenReady,
		"list": function() {
			return whenReady().then(function() {
				if (!isDir(WORLDS))
					return [];
				return children(WORLDS).filter(function(name) {
					return name.charAt(0) !== "." && exists(WORLDS + "/" + name + "/world.mt");
				}).map(describe);
			});
		},
		"get": function(name) {
			return whenReady().then(function() {
				requireWorld(name);
				return describe(name);
			});
		},
		"games": availableGames,
		"validateName": validateName,
		"suggestName": function(base) {
			return whenReady().then(function() { return uniqueName(base); });
		},
		"pathFor": worldPath,
		"create": function(options) {
			options = options || {};
			return whenReady().then(function() {
				var name = validateName(options["name"]);
				var game = String(options["game"] || "");
				if (!/^[a-z0-9_-]+$/.test(game))
					throw LibraryError("Choose a game for the new world.", "game");
				var path = requireFree(name);
				// Same choice as loadGameConfAndInitWorld (src/content/subgames.cpp),
				// when the game's files are already in the filesystem.
				var backend = "sqlite3";
				[ROOT + "/games/" + game, "/games/" + game].forEach(function(dir) {
					try {
						if (parseConf(readText(dir + "/game.conf", MAX_CONF_BYTES))["map_persistent"] === "false")
							backend = "dummy";
					} catch (_) {}
				});
				mkdirs(path);
				var lines = [
					"world_name = " + name,
					"gameid = " + game,
					"backend = " + backend,
					"player_backend = sqlite3",
					"auth_backend = sqlite3",
					"mod_storage_backend = sqlite3"
				];
				// Left out unless chosen, so the game's and the user's
				// defaults still apply.
				if (typeof options["creative"] === "boolean")
					lines.push("creative_mode = " + options["creative"]);
				if (typeof options["damage"] === "boolean")
					lines.push("enable_damage = " + options["damage"]);
				FS.writeFile(path + "/world.mt", lines.join("\n") + "\n");
				var seed = options["seed"] === undefined || options["seed"] === null ?
					"" : singleLine(options["seed"]);
				if (seed)
					FS.writeFile(path + "/map_meta.txt", "seed = " + seed + "\n[end_of_params]\n");
				writeLibraryMeta(path, {"created": Date.now(), "lastPlayed": null});
				var result = describe(name);
				return commit("create", {"world": name}).then(function() { return result; });
			});
		},
		"rename": function(from, to) {
			return whenReady().then(function() {
				var source = requireWorld(from);
				var name = validateName(to);
				if (name === from)
					return describe(from);
				var target = requireFree(name);
				FS.rename(source, target);
				var mt = target + "/world.mt";
				FS.writeFile(mt, setConfValue(readText(mt, MAX_CONF_BYTES), "world_name", name));
				var result = describe(name);
				return commit("rename", {"world": name, "from": from}).then(function() { return result; });
			});
		},
		"duplicate": function(from, to) {
			return whenReady().then(function() {
				var source = requireWorld(from);
				var name = to ? validateName(to) : uniqueName(from + " copy");
				var target = requireFree(name);
				var staging = WORLDS + "/.copy-" + Date.now().toString(36);
				try {
					copyTree(source, staging);
					var mt = staging + "/world.mt";
					FS.writeFile(mt, setConfValue(readText(mt, MAX_CONF_BYTES), "world_name", name));
					writeLibraryMeta(staging, {"created": Date.now(), "lastPlayed": null});
					FS.rename(staging, target);
				} catch (error) {
					try {
						removeTree(staging);
					} catch (_) {}
					throw error;
				}
				var result = describe(name);
				return commit("duplicate", {"world": name, "from": from}).then(function() { return result; });
			});
		},
		"remove": function(name) {
			return whenReady().then(function() {
				removeTree(requireWorld(name));
				return commit("delete", {"world": name});
			});
		},
		"markPlayed": function(name) {
			return whenReady().then(function() {
				writeLibraryMeta(requireWorld(name), {"lastPlayed": Date.now()});
				// The engine is about to start and will sync on its own.
			});
		},
		"thumbnail": function(name) {
			return whenReady().then(function() {
				var path = requireWorld(name) + "/" + THUMBNAIL_FILE;
				return exists(path) ? new Blob([FS.readFile(path)], {"type": "image/jpeg"}) : null;
			});
		},
		"saveThumbnail": function(name, blob) {
			return whenReady().then(function() {
				return blob.arrayBuffer();
			}).then(function(buffer) {
				if (buffer.byteLength > 512 * 1024)
					throw LibraryError("Thumbnail is too large.", "too-large");
				FS.writeFile(requireWorld(name) + "/" + THUMBNAIL_FILE, new Uint8Array(buffer));
				return commit("thumbnail", {"world": name});
			});
		},
		"exportWorld": exportWorld,
		"inspectArchive": function(blob, options) {
			return whenReady().then(function() {
				return inspect(blob, options);
			}).then(publicPlan);
		},
		"importArchive": importArchive,
		"fetchShared": fetchShared,
		"storage": storageInfo,
		"requestPersistence": function() {
			persistRequested = false;
			return requestDurableStorage();
		},
		// Internals for util/wasm/test_worlds.mjs.
		"_zip": {"ZipWriter": ZipWriter, "memorySink": memorySink, "crc32": crc32,
			"readDirectory": readDirectory, "checkEntryPath": checkEntryPath}
	};

	if (persistence)
		persistence["library"] = library;
	Module["luantiWorlds"] = library;
})();
