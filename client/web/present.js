// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Presents frames rendered on the engine worker (offscreen render mode).
// This file is included with --pre-js.
//
// In offscreen mode the engine's WebGL context lives on a worker-owned
// OffscreenCanvas (irr/src/CIrrDeviceSDL.cpp, "Offscreen render mode"). The
// worker never returns to its event loop, so the canvas cannot present itself;
// instead each finished frame is taken with transferToImageBitmap() and posted
// here, where it is shown through a "bitmaprenderer" context on #canvas. That
// is a GPU texture handoff, not a pixel copy. #canvas itself is never
// transferred, so input, pointer lock, fullscreen and resizing work on it
// exactly as in the proxied mode.
//
// Pacing: frames are shown on requestAnimationFrame. Every frame received is
// acknowledged at the next animation frame, whether it was shown or replaced
// by a newer one, by adding to a counter in shared memory. The worker keeps at
// most one frame unacknowledged (it renders the next one meanwhile), so it
// runs no faster than the display.

(function() {
	"use strict";
	if (typeof window === "undefined" || typeof Module === "undefined")
		return;

	var ctx = null;
	var pending = null;
	var ackPtr = 0;
	var unacked = 0;
	var scheduled = false;
	var shown = 0;
	var dropped = 0;

	function acknowledge() {
		var heap = Module["HEAPU8"];
		if (ackPtr && heap && unacked)
			// The counter is a static, so a view of an older (pre-growth)
			// buffer still covers it.
			Atomics.add(new Int32Array(heap.buffer), ackPtr >>> 2, unacked);
		unacked = 0;
	}

	function show() {
		scheduled = false;
		var bitmap = pending;
		pending = null;
		if (!bitmap) {
			acknowledge();
			return;
		}
		var canvas = Module["canvas"];
		if (!ctx && canvas)
			ctx = canvas.getContext("bitmaprenderer");
		if (!ctx) {
			bitmap.close();
			dropped++;
			acknowledge();
			return;
		}
		ctx.transferFromImageBitmap(bitmap);
		shown++;
		acknowledge();
		if (shown === 1)
			console.info("Luanti: presenting worker-rendered frames on #canvas");
		var launcher = Module["luantiLauncher"];
		if (launcher && typeof launcher["notifyFramePresented"] === "function")
			launcher["notifyFramePresented"]();
	}

	Module["luantiPresentFrame"] = function(bitmap, counter) {
		if (pending) {
			pending.close();
			dropped++;
		}
		pending = bitmap;
		ackPtr = counter;
		unacked++;
		// No animation frames while hidden; show (and acknowledge) right away
		// so the worker is not held at its in-flight limit.
		if (document.hidden) {
			show();
			return;
		}
		if (!scheduled) {
			scheduled = true;
			requestAnimationFrame(show);
		}
	};

	// Frames arrive as {luantiFrame, luantiAck} messages from the engine
	// worker. Emscripten's own handler ignores messages without a "cmd" field,
	// and its command format differs between releases, so listen on every
	// worker the page creates instead. This runs before Emscripten creates its
	// pthread pool (pre-js); other workers simply never send such a message.
	var NativeWorker = window.Worker;
	if (typeof NativeWorker === "function") {
		var onMessage = function(event) {
			var data = event.data;
			if (data && data["luantiFrame"])
				Module["luantiPresentFrame"](data["luantiFrame"], data["luantiAck"]);
		};
		/** @param {string | URL} url @param {WorkerOptions=} options */
		var LuantiWorker = function(url, options) {
			var worker = new NativeWorker(url, options);
			worker.addEventListener("message", onMessage);
			return worker;
		};
		LuantiWorker.prototype = NativeWorker.prototype;
		window.Worker = /** @type {any} */ (LuantiWorker);
	}

	Module["luantiPresentStats"] = function() {
		return {"shown": shown, "dropped": dropped};
	};
})();
