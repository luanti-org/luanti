// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// WebGL2 compatibility shim for resizable WASM heaps.
// This file is included with --pre-js.
//
// With -sMAX_WEBGL_VERSION=2 Emscripten uses the "garbage-free" entry points,
// e.g. GLctx.uniform1iv(location, HEAP32, offset, count), which pass a view of
// the whole heap. With -sALLOW_MEMORY_GROWTH=1 and shared memory, browsers may
// back that view with a resizable/growable buffer, which WebGL2 rejects:
//   TypeError: The provided ArrayBuffer value must not be resizable
// WEBGL_USE_GARBAGE_FREE_APIS is an internal emcc setting and cannot be
// disabled from the command line, so copy the data out of such heaps before
// it reaches WebGL.
//
// Only views of a resizable or growable buffer are copied; everything else
// goes straight through. Copies go into a scratch buffer per entry point that
// grows to the largest upload seen, instead of a fresh .slice() per call:
// every WebGL entry point patched here consumes its data before returning,
// so the scratch is free again as soon as the call is. Counters are exposed
// as Module.luantiWebGLCompat.stats() for the frame-time benchmark.

(function() {
	"use strict";
	// Proxied render mode: WebGL runs on the browser main thread. Offscreen
	// mode: it runs on the engine worker, where the same heap restriction
	// applies, so this also patches workers that expose WebGL2 (Chrome and
	// Firefox do; the counters are then per thread).
	if (typeof WebGL2RenderingContext === "undefined")
		return;

	var stats = {"calls": 0, "copies": 0, "bytes": 0, "scratchBytes": 0};
	if (typeof Module !== "undefined") {
		Module["luantiWebGLCompat"] = {
			"stats": function() {
				return {
					"calls": stats["calls"],
					"copies": stats["copies"],
					"bytes": stats["bytes"],
					"scratchBytes": stats["scratchBytes"]
				};
			}
		};
	}

	function isResizable(buffer) {
		return buffer && (buffer.resizable === true || buffer.growable === true);
	}

	// One scratch ArrayBuffer per (entry point, argument) so two arguments of
	// one call never share storage. Grown in powers of two.
	var scratch = {};
	function copyOf(key, view, start, end) {
		var ctor = view.constructor;
		var count = end - start;
		var bytes = count * ctor.BYTES_PER_ELEMENT;
		var buffer = scratch[key];
		if (!buffer || buffer.byteLength < bytes) {
			var size = 64;
			while (size < bytes)
				size *= 2;
			stats["scratchBytes"] += size - (buffer ? buffer.byteLength : 0);
			buffer = scratch[key] = new ArrayBuffer(size);
		}
		var copy = new ctor(buffer, 0, count);
		copy.set(start === 0 && end === view.length ? view : view.subarray(start, end));
		stats["copies"]++;
		stats["bytes"] += bytes;
		return copy;
	}

	// Index of the data argument and whether it is followed by srcOffset and
	// srcLength (always the case for the uniform*v family).
	function patch(name, dataIndex) {
		var proto = WebGL2RenderingContext.prototype;
		var original = proto[name];
		if (typeof original !== "function")
			return;
		proto[name] = function() {
			stats["calls"]++;
			/** @type {any} */
			var data = arguments[dataIndex];
			if (!data || !ArrayBuffer.isView(data) || !isResizable(data.buffer))
				return original.apply(this, arguments);
			var offset = arguments[dataIndex + 1] || 0;
			var length = arguments[dataIndex + 2];
			var end = length ? offset + length : /** @type {any} */ (data).length;
			var args = Array.prototype.slice.call(arguments, 0, dataIndex);
			args.push(copyOf(name, data, offset, end));
			return original.apply(this, args);
		};
	}

	var vectorTypes = ["1f", "2f", "3f", "4f", "1i", "2i", "3i", "4i",
		"1ui", "2ui", "3ui", "4ui"];
	for (var i = 0; i < vectorTypes.length; i++)
		patch("uniform" + vectorTypes[i] + "v", 1);

	var matrixTypes = ["2fv", "3fv", "4fv", "2x3fv", "2x4fv", "3x2fv",
		"3x4fv", "4x2fv", "4x3fv"];
	for (var j = 0; j < matrixTypes.length; j++)
		patch("uniformMatrix" + matrixTypes[j], 2);

	// Everything else that takes array data. With the garbage-free APIs off
	// (MAXIMUM_MEMORY in src/CMakeLists.txt) Emscripten passes a subarray of
	// the heap with no offset arguments, but that view is still on the
	// growable shared buffer and WebGL rejects it just the same. Inputs are
	// copied; outputs (readPixels, getBufferSubData) are read into a copy and
	// written back.
	var outputs = {readPixels: true, getBufferSubData: true};
	function patchViews(name) {
		var proto = WebGL2RenderingContext.prototype;
		var original = proto[name];
		if (typeof original !== "function")
			return;
		proto[name] = function() {
			stats["calls"]++;
			var copied = null;
			var back = null;
			for (var k = 0; k < arguments.length; k++) {
				/** @type {any} */
				var arg = arguments[k];
				if (arg && ArrayBuffer.isView(arg) && isResizable(arg.buffer)) {
					copied = copied || Array.prototype.slice.call(arguments);
					copied[k] = copyOf(name + k, arg, 0, /** @type {any} */ (arg).length);
					if (outputs[name])
						back = [k, arg];
				}
			}
			if (!copied)
				return original.apply(this, arguments);
			var result = original.apply(this, copied);
			if (back)
				back[1].set(copied[back[0]]);
			return result;
		};
	}
	var viewTakers = ["bufferData", "bufferSubData", "texImage2D", "texSubImage2D",
		"texImage3D", "texSubImage3D", "compressedTexImage2D",
		"compressedTexSubImage2D", "compressedTexImage3D", "compressedTexSubImage3D",
		"readPixels", "getBufferSubData", "vertexAttrib1fv", "vertexAttrib2fv",
		"vertexAttrib3fv", "vertexAttrib4fv", "vertexAttribI4iv", "vertexAttribI4uiv",
		"clearBufferfv", "clearBufferiv", "clearBufferuiv", "drawBuffers"];
	for (var v = 0; v < viewTakers.length; v++)
		patchViews(viewTakers[v]);
})();
