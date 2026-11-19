// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Browser autoplay unlock for the engine's OpenAL contexts.
// This file is included with --pre-js.
//
// Emscripten's OpenAL is a JavaScript implementation over Web Audio. Under
// PROXY_TO_PTHREAD the engine calls alcOpenDevice/alcCreateContext from the
// application worker, but those entry points are proxied to the browser main
// thread, so every AudioContext lives here in AL.contexts[id].audioCtx. A
// context created without a user gesture starts "suspended" and stays silent
// until resume() is called from inside one.
//
// Emscripten's own autoResumeAudioContext registers its listeners with
// {once: true}: if the first keydown/mousedown after the context exists is
// not one the browser counts as activation, that was the only try. And the
// launcher's form-submit click happens before the game creates its context.
// So: retry on every gesture until each context reports "running", and log
// the state on the way so a silent session can be told apart from a locked
// one.
//
// A context can also stop later: some browsers suspend it when the tab is
// hidden (Safari reports "interrupted"), and the embedder pause
// (Module.luantiLauncher.setPaused) suspends it on purpose so the engine's
// stream stepping, which the engine skips while paused, and the audio clock
// stand still together. Hiding is left to the browser: nothing here keeps
// audio going or decodes ahead. When the page is visible again, or unpaused,
// every stopped context is resumed. (A resume outside a gesture is allowed once
// the page has had one, which the launcher click was.)

(function() {
	"use strict";
	// The application workers run this pre-js too; they have no AudioContext
	// and no AL contexts of their own. AL itself is declared further down the
	// generated file, so it is only looked up once the page is running.
	if (typeof document === "undefined")
		return;

	var seen = typeof WeakSet === "function" ? new WeakSet() : null;
	// True between Module.luantiSuspendAudio("pause") and the next unpause.
	var held = false;

	function contexts() {
		var out = [];
		if (typeof AL === "undefined" || !AL || !AL.contexts)
			return out;
		for (var id in AL.contexts) {
			var ctx = AL.contexts[id];
			if (ctx && ctx.audioCtx)
				out.push({id: id, audioCtx: ctx.audioCtx});
		}
		return out;
	}

	function watch(entry) {
		if (!seen || seen.has(entry.audioCtx))
			return;
		seen.add(entry.audioCtx);
		console.info("[audio] OpenAL context " + entry.id + ": AudioContext " +
			entry.audioCtx.state + ", " + entry.audioCtx.sampleRate + " Hz");
		entry.audioCtx.addEventListener("statechange", function() {
			console.info("[audio] OpenAL context " + entry.id + ": AudioContext is now " +
				entry.audioCtx.state);
			// Emscripten's own once-only gesture listeners resume a suspended
			// context too. While the embedder holds the pause, put it back.
			if (held && entry.audioCtx.state === "running") {
				console.info("[audio] OpenAL context " + entry.id + ": still paused, suspending again");
				entry.audioCtx.suspend().catch(function() {});
			}
		});
	}

	// Called from gestures, and by launcher.js. Returns how many contexts were
	// still suspended, so the caller can tell nothing was there to unlock.
	function resumeAll(reason) {
		var list = contexts();
		var suspended = 0;
		for (var i = 0; i < list.length; i++) {
			var entry = list[i];
			watch(entry);
			// "interrupted" is Safari's word for a context the system took away.
			if (entry.audioCtx.state !== "suspended" && entry.audioCtx.state !== "interrupted")
				continue;
			// Held on purpose by the embedder pause: only unpause resumes it.
			if (held && reason !== "unpause")
				continue;
			suspended++;
			console.info("[audio] " + reason + ": resuming OpenAL context " + entry.id +
				" (state before: " + entry.audioCtx.state + ")");
			(function(e) {
				e.audioCtx.resume().then(function() {
					console.info("[audio] OpenAL context " + e.id + " after " + reason +
						": " + e.audioCtx.state);
				}, function(err) {
					console.warn("[audio] OpenAL context " + e.id + " did not resume: " + err);
				});
			})(entry);
		}
		return suspended;
	}

	// Called by the launcher when the embedder pauses the engine: suspend every
	// context (a no-op for one that is already stopped) and stop gestures and
	// visibility changes from waking them until it is unpaused.
	function suspendAll(reason) {
		held = true;
		var list = contexts();
		for (var i = 0; i < list.length; i++) {
			watch(list[i]);
			if (list[i].audioCtx.state !== "running")
				continue;
			console.info("[audio] " + reason + ": suspending OpenAL context " + list[i].id);
			(function(e) {
				e.audioCtx.suspend().catch(function(err) {
					console.warn("[audio] OpenAL context " + e.id + " did not suspend: " + err);
				});
			})(list[i]);
		}
		return list.length;
	}

	function unhold(reason) {
		held = false;
		return resumeAll(reason);
	}

	Module["luantiResumeAudio"] = resumeAll;
	Module["luantiSuspendAudio"] = suspendAll;
	Module["luantiUnpauseAudio"] = unhold;
	Module["luantiAudioState"] = function() {
		var list = contexts();
		if (!list.length)
			return "none";
		return list.map(function(e) { return e.audioCtx.state; }).join(",");
	};

	// Capture phase on the document, so a canvas that stops propagation (or
	// takes pointer lock) still counts. Never removed: they cost nothing once
	// every context is running.
	var gestures = ["pointerdown", "mousedown", "keydown", "touchend", "click"];
	for (var g = 0; g < gestures.length; g++) {
		(function(name) {
			document.addEventListener(name, function() {
				resumeAll(name);
			}, {capture: true, passive: true});
		})(gestures[g]);
	}

	// Back in view: wake whatever the browser stopped while the tab was
	// hidden. Hidden is only logged: the browser decides whether to suspend,
	// and the engine drops to fps_max_unfocused on its own.
	document.addEventListener("visibilitychange", function() {
		if (document.visibilityState === "hidden") {
			console.info("[audio] page hidden; OpenAL contexts: " + Module["luantiAudioState"]());
			return;
		}
		console.info("[audio] page visible; OpenAL contexts: " + Module["luantiAudioState"]());
		resumeAll("visibilitychange");
	});
	// Restored from the back/forward cache: no visibilitychange precedes it
	// in every browser.
	window.addEventListener("pageshow", function(event) {
		if (event && event.persisted)
			resumeAll("pageshow");
	});

	// Report contexts as they appear, without waiting for a gesture.
	setInterval(function() {
		var list = contexts();
		for (var i = 0; i < list.length; i++)
			watch(list[i]);
	}, 1000);
})();
