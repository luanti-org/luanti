// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Runtime media: files a mod asks for by id, fetched on demand.
// This file is included with --pre-js.
//
// The streamed asset packages (packages.js) carry what a game needs to start.
// Some content is far too big for that and is only wanted by the few players
// who walk up to it: the showcase's concert songs are 26 MiB of mono Oggs. They
// sit in client/web/media/songs/ next to the page instead (like the theater
// clips), and asset-manifest.json's "media" section lists each set with its
// files, sizes and SHA-256 sums (built from util/wasm/packages.json by
// build_packages.py). A mod calls core.web_media_request("songs/<id>"); the
// engine forwards the id here, this file downloads the files and writes them
// into the Emscripten filesystem, and the mod then hands them on with
// core.dynamic_add_media. Nothing here decodes audio: the engine keeps Vorbis.
//
// The policy is deliberately narrow:
//   * a mod can only name an id from the manifest; the URLs and the
//     destination directory come from the manifest, never from the mod;
//   * one download at a time: a request for another id while one is in flight
//     is answered "busy" and not queued, so a listener cannot start a second
//     multi-megabyte transfer;
//   * every file is size- and hash-checked before any of it is written, and a
//     set is written whole or not at all;
//   * at most two sets stay resident; the mod gives each one back with
//     release() once the engine has taken the bytes.
// Property names on Module are quoted so they survive Closure.

(function() {
	"use strict";
	// In PROXY_TO_PTHREAD builds this program is also evaluated in workers;
	// the filesystem is owned by the browser main thread.
	if (typeof window === "undefined" && typeof document === "undefined")
		return;

	var ID_PATTERN = /^[a-z0-9_]+\/[a-z0-9_]{1,64}$/;
	var FILE_PATTERN = /^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$/;
	var DEST_PATTERN = /^\/[A-Za-z0-9_/.-]+$/;
	var MAX_RESIDENT = 2;
	var RETRY_MS = 5000;

	// Codes returned to the engine (src/porting_emscripten.cpp reads these).
	var CODES = {"unavailable": 0, "ready": 1, "loading": 2, "busy": 3,
		"missing": 4, "failed": 5, "invalid": 6};

	var sets = {};
	var inFlight = null;
	var resident = [];

	function emit(id, state, detail) {
		try {
			window.dispatchEvent(new CustomEvent("luanti:media", {"detail":
				{"id": id, "state": state, "detail": detail || null}}));
		} catch (_) {}
	}

	function manifestUrl() {
		return Module["luantiAssetManifestUrl"] || "asset-manifest.json";
	}

	function resolve(relative) {
		// Media URLs are relative to the manifest, not to the page.
		return new URL(relative, new URL(manifestUrl(), document.baseURI)).href;
	}

	function toHex(buffer) {
		return Array.prototype.map.call(new Uint8Array(buffer), function(byte) {
			return (byte < 16 ? "0" : "") + byte.toString(16);
		}).join("");
	}

	function MissingError(message) {
		this.message = message;
	}
	MissingError.prototype = Object.create(Error.prototype);

	async function fetchOnce(item, cache) {
		var response = await fetch(resolve(item["url"]), {"cache": cache});
		if (response.status === 404)
			throw new MissingError(item["url"] + ": HTTP 404");
		if (!response.ok)
			throw new Error(item["url"] + ": HTTP " + response.status);
		var expected = item["bytes"];
		var bytes = new Uint8Array(expected);
		var received = 0;
		if (response.body && response.body.getReader) {
			var reader = response.body.getReader();
			for (;;) {
				var step = await reader.read();
				if (step.done)
					break;
				if (received + step.value.length > expected)
					throw new Error(item["url"] + ": larger than the manifest says");
				bytes.set(step.value, received);
				received += step.value.length;
			}
		} else {
			var whole = new Uint8Array(await response.arrayBuffer());
			if (whole.length > expected)
				throw new Error(item["url"] + ": larger than the manifest says");
			bytes.set(whole);
			received = whole.length;
		}
		if (received !== expected)
			throw new Error(item["url"] + ": truncated (" + received + " of " + expected + " bytes)");
		if (window.crypto && window.crypto.subtle) {
			var digest = toHex(await window.crypto.subtle.digest("SHA-256", bytes));
			if (digest !== item["sha256"])
				throw new Error(item["url"] + ": hash mismatch");
		}
		return bytes;
	}

	async function download(item) {
		try {
			return await fetchOnce(item, "default");
		} catch (error) {
			// A stale HTTP cache entry (the file changed in place) is the one
			// failure a second, uncached request can cure.
			if (error instanceof MissingError || String(error).indexOf("mismatch") < 0)
				throw error;
			return await fetchOnce(item, "reload");
		}
	}

	function fsReady() {
		return typeof FS !== "undefined" && FS.root && FS.createDataFile;
	}

	function unlinkQuietly(path) {
		try {
			FS.unlink(path);
		} catch (_) {}
	}

	function write(entry, files) {
		FS.createPath("/", entry["dest"], true, true);
		for (var i = 0; i < files.length; i++) {
			var path = entry["dest"] + "/" + files[i].name;
			unlinkQuietly(path);
			FS.createDataFile(entry["dest"], files[i].name, files[i].bytes, true, true, true);
		}
	}

	function erase(id) {
		var set = sets[id];
		if (!set || set.state !== "ready")
			return false;
		for (var i = 0; i < set.names.length; i++)
			unlinkQuietly(set.dest + "/" + set.names[i]);
		resident = resident.filter(function(other) { return other !== id; });
		delete sets[id];
		console.info("[media] " + id + ": released (" + set.bytes + " bytes)");
		emit(id, "released");
		return true;
	}

	async function run(id) {
		var set = sets[id];
		try {
			var packages = Module["luantiPackages"];
			if (!packages || typeof packages["manifest"] !== "function")
				throw new Error("the asset loader (packages.js) is missing");
			var manifest = await packages["manifest"]();
			var entry = manifest["media"] && manifest["media"][id];
			if (!entry) {
				// Not an error in the page: this build does not ship the set.
				console.warn("[media] " + id + ": file missing (not in this build's asset manifest)");
				set.state = "missing";
				return;
			}
			if (!DEST_PATTERN.test(entry["dest"]) || entry["dest"].indexOf("..") >= 0)
				throw new Error("unusable destination " + entry["dest"]);
			var files = [];
			var total = 0;
			for (var i = 0; i < entry["files"].length; i++) {
				var item = entry["files"][i];
				if (!FILE_PATTERN.test(item["name"]))
					throw new Error("unusable file name " + item["name"]);
				var bytes = await download(item);
				files.push({"name": item["name"], "bytes": bytes});
				total += bytes.length;
			}
			// All of the set is here and verified; only now touch the FS.
			while (!fsReady())
				await new Promise(function(done) { setTimeout(done, 20); });
			while (resident.length >= MAX_RESIDENT)
				erase(resident[0]);
			write(entry, files);
			set.dest = entry["dest"];
			set.names = files.map(function(f) { return f.name; });
			set.bytes = total;
			set.state = "ready";
			resident.push(id);
			console.info("[media] " + id + ": ready, " + files.length + " files, " +
				(total / 1048576).toFixed(2) + " MiB written to " + entry["dest"]);
		} catch (error) {
			if (error instanceof MissingError) {
				console.warn("[media] " + id + ": file missing (" + /** @type {Error} */ (error).message + ")");
				set.state = "missing";
			} else {
				console.warn("[media] " + id + ": fetch failed: " + (error && error.message || error));
				set.state = "failed";
				set["error"] = String(error && error.message || error);
				set.at = Date.now();
			}
		} finally {
			if (inFlight === id)
				inFlight = null;
			emit(id, set.state, set["error"]);
		}
	}

	function request(id) {
		if (typeof id !== "string" || !ID_PATTERN.test(id))
			return "invalid";
		var known = sets[id];
		// A failure is retried, but not every sweep of a mod.
		if (known && !(known.state === "failed" && Date.now() - known.at >= RETRY_MS))
			return known.state;
		if (inFlight !== null && inFlight !== id)
			return "busy";
		sets[id] = {"state": "loading", "names": [], "dest": "", "bytes": 0, "at": 0};
		inFlight = id;
		console.info("[media] " + id + ": fetching");
		emit(id, "loading");
		run(id);
		return "loading";
	}

	Module["luantiRuntimeMedia"] = {
		"request": request,
		"state": function(id) {
			return sets[id] ? sets[id].state : "idle";
		},
		"release": erase,
		// For the engine: the same answers as numbers, so no string has to
		// cross from the application worker.
		"requestCode": function(id) {
			var state = request(id);
			return CODES[state] === undefined ? CODES["failed"] : CODES[state];
		},
		"releaseCode": function(id) {
			return erase(id) ? 1 : 0;
		},
		"status": function() {
			return {"inFlight": inFlight, "resident": resident.slice(),
				"sets": Object.keys(sets).reduce(function(all, id) {
					all[id] = sets[id].state;
					return all;
				}, {})};
		}
	};
})();
