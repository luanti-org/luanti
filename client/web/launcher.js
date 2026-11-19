// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

(function() {
	"use strict";

	/** @type {Partial<LuantiWebConfig>} */
	var config = window.LUANTI_WEB_CONFIG || {};
	var query = new URLSearchParams(location.search);
	var runtimeReady = false;
	// Embedder pause request (luantiLauncher.setPaused). Kept here so a request
	// made before the runtime exists is applied the moment it does.
	var enginePaused = false;
	var coreReady = false;
	var backgroundStarted = false;
	var started = false;
	var launchInFlight = false;
	var activeMode = query.get("address") ? "remote" : "local";
	var activePhase = "assets";
	var currentSession = null;
	var hadPointerLock = false;
	var loadingShellDismissed = false;
	var startupFailure = null;
	var logs = [];

	/**
	 * The launcher markup is fixed (client/web/shell.html), so each element is
	 * typed `any` rather than guessing HTMLInputElement/HTMLButtonElement/etc.
	 * @param {string} id
	 * @returns {any}
	 */
	function element(id) {
		return document.getElementById(id);
	}

	var ui = {
		launcher: element("luanti-launcher"),
		gameShell: element("luanti-game-shell"),
		form: element("luanti-launch-form"),
		localTab: element("luanti-local-tab"),
		remoteTab: element("luanti-remote-tab"),
		localPanel: element("luanti-local-panel"),
		remotePanel: element("luanti-remote-panel"),
		address: element("luanti-address"),
		port: element("luanti-port"),
		name: element("luanti-player-name"),
		game: element("luanti-game"),
		quality: element("luanti-quality"),
		language: element("luanti-language"),
		proxyRegion: element("luanti-proxy-region"),
		customProxyRow: element("luanti-custom-proxy-row"),
		proxyUrl: element("luanti-proxy-url"),
		play: element("luanti-play-button"),
		playLabel: element("luanti-play-label"),
		error: element("luanti-launch-error"),
		serverListButton: element("luanti-server-list-button"),
		serverList: element("luanti-server-list"),
		loading: element("luanti-loading"),
		loadingPhase: element("luanti-loading-phase"),
		loadingStatus: element("luanti-loading-status"),
		loadingProgress: element("luanti-loading-progress"),
		share: element("luanti-share-button"),
		fullscreen: element("luanti-fullscreen-button"),
		consoleButton: element("luanti-console-button"),
		consolePanel: element("luanti-console"),
		consoleOutput: element("luanti-console-output"),
		resume: element("luanti-resume"),
		resumeButton: element("luanti-resume-button"),
		glLost: element("luanti-gl-lost"),
		glLostText: element("luanti-gl-lost-text"),
		glLostReload: element("luanti-gl-lost-reload"),
		update: element("luanti-update"),
		updateReload: element("luanti-update-reload"),
		worldList: element("luanti-world-list"),
		newWorld: element("luanti-new-world"),
		importWorld: element("luanti-import-world"),
		importFile: element("luanti-import-file"),
		libraryStatus: element("luanti-library-status"),
		storageNote: element("luanti-storage-note"),
		dialog: element("luanti-dialog"),
		dialogForm: element("luanti-dialog-form"),
		dialogTitle: element("luanti-dialog-title"),
		dialogBody: element("luanti-dialog-body"),
		dialogError: element("luanti-dialog-error"),
		dialogCancel: element("luanti-dialog-cancel"),
		dialogConfirm: element("luanti-dialog-confirm")
	};

	// Render overrides read by CIrrDeviceSDL.cpp, for tests and benchmarks:
	// ?render=proxy|offscreen|auto picks the thread that owns WebGL (default:
	// the build's WASM_RENDER_THREAD), ?webgl=1|2 forces a single WebGL
	// version, ?swapsleep=N sleeps N ms after each proxied frame.
	var webglOptions = Module["luantiWebGL"] || {};
	var renderMode = query.get("render");
	if (renderMode === "proxy" || renderMode === "offscreen" || renderMode === "auto")
		webglOptions["render"] = renderMode;
	var forcedWebGL = Number(query.get("webgl"));
	if (forcedWebGL === 1 || forcedWebGL === 2)
		webglOptions["major"] = forcedWebGL;
	var swapSleep = Number(query.get("swapsleep"));
	if (swapSleep > 0 && swapSleep <= 100)
		webglOptions["swapSleepMs"] = swapSleep;
	Module["luantiWebGL"] = webglOptions;

	function safeStorageGet(key) {
		try {
			return localStorage.getItem(key) || "";
		} catch (_) {
			return "";
		}
	}

	function safeStorageSet(key, value) {
		try {
			localStorage.setItem(key, value);
		} catch (_) {
		}
	}

	function appendLog(kind, values) {
		var line = Array.prototype.map.call(values, String).join(" ");
		logs.push("[" + kind + "] " + line);
		if (logs.length > 300)
			logs.splice(0, logs.length - 300);
		ui.consoleOutput.textContent = logs.join("\n");
		ui.consoleOutput.scrollTop = ui.consoleOutput.scrollHeight;
	}

	Module["print"] = function() {
		appendLog("info", arguments);
		console.log.apply(console, arguments);
	};
	Module["printErr"] = function() {
		appendLog("error", arguments);
		console.error.apply(console, arguments);
	};

	function setProgress(percent) {
		var value = Number(percent);
		if (!Number.isFinite(value))
			return;
		ui.loadingProgress.style.width = Math.max(2, Math.min(100, value)) + "%";
	}

	function dismissLoadingShell() {
		if (!ui.loading || loadingShellDismissed)
			return;
		loadingShellDismissed = true;
		// Keep the node in the layout tree (OFFSCREEN_FRAMEBUFFER compositing)
		// but never cover the canvas again for this session.
		ui.loading.style.opacity = "0";
		ui.loading.style.pointerEvents = "none";
	}

	function showLoadingShell() {
		if (!ui.loading || loadingShellDismissed)
			return;
		ui.loading.hidden = false;
		ui.loading.style.opacity = "1";
		ui.loading.style.pointerEvents = "";
	}

	// Optional content (the showcase songs) streams in once the world is up,
	// so it never delays the first frame. lw_amps registers songs lazily and
	// stays quiet until the files arrive.
	function startBackgroundContent() {
		if (backgroundStarted || !currentSession || currentSession.mode !== "local")
			return;
		backgroundStarted = true;
		Module["luantiPackages"]["loadBackground"](currentSession.game).catch(function(error) {
			appendLog("packages", ["background content failed: " + String(error && error.message || error)]);
		});
	}

	function reportStatus(message, percent, phase) {
		if (phase)
			activePhase = phase;
		var phaseLabels = {
			"assets": "Downloading assets",
			"storage": "Opening local storage",
			"engine": "Starting engine",
			"menu": "Main menu",
			"loading": "Loading world",
			"playing": "Entering world"
		};
		if (!loadingShellDismissed) {
			ui.loadingPhase.textContent = phaseLabels[activePhase] || activePhase;
			if (message)
				ui.loadingStatus.textContent = message;
			setProgress(percent);
		}
		if (started && (activePhase === "engine" || activePhase === "loading"))
			showLoadingShell();
		if (activePhase === "menu") {
			ui.loading.hidden = true;
			ui.share.hidden = true;
		}
		if (activePhase === "playing") {
			startBackgroundContent();
			armThumbnail();
			// Dismiss as soon as the engine enters the game loop. Waiting for
			// the first GL commit left a full-screen "World ready" panel that
			// flickered against the canvas whenever status/frame hooks raced.
			dismissLoadingShell();
			ui.share.hidden = false;
			ui.share.textContent = currentSession && currentSession.mode === "remote" ?
				"Copy invite link" : "Copy launcher link";
		}
	}

	Module["luantiLauncher"] = {
		"reportEngineStatus": reportStatus,
		"prepareCompositor": function(percent) {
			// Never re-show the shell. Progress only.
			if (!loadingShellDismissed)
				setProgress(percent);
		},
		"notifyFramePresented": function() {
			if (started && activePhase === "playing") {
				dismissLoadingShell();
				captureThumbnail();
			}
		},
		// Stand the engine's frame loop still (or resume it). The flag is an
		// atomic in the engine's shared memory, so this is safe at any point of
		// the page lifetime: before callMain it is remembered and applied once
		// the runtime exists, afterwards it takes effect on the next frame. The
		// world, input and presents wait; an integrated server thread does not.
		"setPaused": function(paused) {
			enginePaused = !!paused;
			applyPaused();
			return enginePaused;
		},
		"isPaused": function() {
			return enginePaused;
		},
		// Measured frame clock of the engine loop: frames counted so far and
		// the latest dtime in milliseconds (null until the runtime exists).
		"getFrameClock": function() {
			var count = Module["_luanti_frame_clock_count"];
			var last = Module["_luanti_frame_clock_last_dtime_ms"];
			if (!runtimeReady || typeof count !== "function" || typeof last !== "function")
				return null;
			return {"frames": count(), "lastDtimeMs": last(), "paused": enginePaused};
		},
		"getGLState": function() {
			return {"lost": glLostCount, "restored": glRestoredCount};
		},
		// Offscreen render mode: the engine's context lives on its worker, so
		// the DOM event below never fires and the engine reports the loss.
		"notifyContextLost": function() {
			handleContextLost();
		},
		// Tests: lose the engine's context in whichever thread owns it.
		"debugLoseContext": function() {
			var info = Module["luantiWebGLInfo"];
			if (info && info["render"] === "offscreen") {
				Module["_luanti_debug_lose_webgl_context"]();
				return true;
			}
			var gl = Module.canvas.getContext("webgl2") || Module.canvas.getContext("webgl");
			var ext = gl && gl.getExtension("WEBGL_lose_context");
			if (!ext)
				return false;
			ext.loseContext();
			window["luantiLoseContextExt"] = ext;
			return true;
		},
		"getState": function() {
			return {
				"runtimeReady": runtimeReady,
				"started": started,
				"phase": activePhase,
				"mode": currentSession ? currentSession.mode : activeMode,
				"loadingDismissed": loadingShellDismissed,
				// Proxy render mode stops calling notifyFramePresented once the
				// shell is dismissed (irr/src/CIrrDeviceSDL.cpp); keep it coming
				// until the world thumbnail is taken.
				"frameHookWanted": thumbnail.pending,
				"world": currentSession && currentSession.world || null,
				"coreReady": coreReady,
				"packages": Module["luantiPackages"] ? Module["luantiPackages"]["getStates"]() : {}
			};
		}
	};

	Module["setStatus"] = function(text) {
		text = String(text || "").trim();
		if (!text)
			return;
		var counts = /\((\d+(?:\.\d+)?)\s*\/\s*(\d+(?:\.\d+)?)\)/.exec(text);
		var percent = counts && Number(counts[2]) > 0 ? Number(counts[1]) / Number(counts[2]) * 100 : null;
		reportStatus(text, percent, activePhase === "storage" ? "storage" : "assets");
	};

	function megabytes(bytes) {
		return (bytes / 1048576).toFixed(1);
	}

	function reportDownload(label, loaded, total) {
		var percent = total > 0 ? loaded / total * 100 : null;
		reportStatus("Downloading " + label + " (" + megabytes(loaded) + " / " + megabytes(total) + " MB)",
			percent, "assets");
	}

	// The core package (builtin, fonts, textures, shaders) starts downloading
	// from packages.js as soon as the engine script is evaluated. Everything
	// else is fetched when a game is chosen.
	window.addEventListener("luanti:package", function(event) {
		var detail = /** @type {CustomEvent} */ (event).detail || {};
		if (detail.name !== "core" || coreReady)
			return;
		if (detail.state === "loading" && detail.total > 0) {
			ui.playLabel.textContent = "Downloading engine data… " +
				Math.floor(detail.loaded / detail.total * 100) + "%";
		}
	});

	function enablePlayIfReady() {
		if (!runtimeReady || !coreReady)
			return;
		ui.play.disabled = false;
		ui.playLabel.textContent = activeMode === "remote" ? "Join server" : "Start world";
		reportStatus("Ready to launch", 100, "engine");
		// A shared world link always waits for the player to confirm it.
		if (query.get("autostart") === "1" && !query.get("world"))
			ui.form.requestSubmit();
	}

	function applyPaused() {
		var setPaused = Module["_luanti_set_paused"];
		if (runtimeReady && typeof setPaused === "function")
			setPaused(enginePaused ? 1 : 0);
	}

	Module["onRuntimeInitialized"] = function() {
		runtimeReady = true;
		applyPaused();
		var packages = Module["luantiPackages"];
		if (!packages) {
			showError("This build is missing its asset loader (packages.js).");
			return;
		}
		packages["core"].then(function() {
			coreReady = true;
			enablePlayIfReady();
		}, function(error) {
			ui.playLabel.textContent = "Engine data failed to load";
			showError("Could not download the engine data: " + String(error && error.message || error) +
				". Check your connection and reload the page.");
		});
	};

	// startup.js ends luanti.js before the engine exists when the browser
	// cannot run this build (no SIMD, no shared memory at any ceiling), so
	// onRuntimeInitialized never fires. Say why, in the panel the player is
	// already looking at.
	function showStartupError(failure) {
		startupFailure = failure || {};
		var message = String(startupFailure.message || "This browser cannot start the engine.");
		ui.playLabel.textContent = "Can't start on this browser";
		ui.play.disabled = true;
		if (ui.loading) {
			ui.loading.hidden = false;
			ui.loadingPhase.textContent = "Cannot start";
			ui.loadingStatus.textContent = message;
		}
		setProgress(0);
		showError(message);
		appendLog("error", [message]);
		// On a phone the launcher form is taller than the screen, and a phone
		// is the likeliest browser to land here: bring the reason into view.
		if (ui.error.scrollIntoView)
			ui.error.scrollIntoView({block: "center"});
	}

	window.addEventListener("luanti:startup-error", function(event) {
		showStartupError(/** @type {CustomEvent} */ (event).detail);
	});
	if (Module["luantiStartup"] && Module["luantiStartup"]["fatal"])
		showStartupError(Module["luantiStartup"]["fatal"]);

	window.addEventListener("luanti:persistence", function(event) {
		var state = event.detail || {};
		if (!started && state.state === "loading")
			reportStatus("Loading saved worlds…", 80, "storage");
		if (!started && !state.fatal && state.state !== "loading") {
			if (!libraryLoaded) {
				libraryLoaded = true;
				refreshWorlds().then(openSharedWorldFromQuery);
			} else if (state.library) {
				refreshWorlds();
			}
		}
		if (started && state.state === "saving" && thumbnail.pending &&
				Date.now() - thumbnail.armedAt > 3000)
			thumbnail.requested = true;
		if (state.fatal) {
			showError("Persistent storage could not start: " + (state.error || "unknown error"));
			ui.play.disabled = true;
		}
	});

	function showError(message) {
		ui.error.textContent = message;
		ui.error.hidden = !message;
	}

	function setMode(mode) {
		activeMode = mode;
		var remote = mode === "remote";
		ui.localTab.classList.toggle("is-active", !remote);
		ui.remoteTab.classList.toggle("is-active", remote);
		ui.localTab.setAttribute("aria-selected", String(!remote));
		ui.remoteTab.setAttribute("aria-selected", String(remote));
		ui.localPanel.hidden = remote;
		ui.remotePanel.hidden = !remote;
		ui.playLabel.textContent = runtimeReady ? (remote ? "Join server" : "Start world") : "Preparing engine…";
		showError("");
		if (startupFailure)
			showStartupError(startupFailure);
	}

	ui.localTab.addEventListener("click", function() { setMode("local"); });
	ui.remoteTab.addEventListener("click", function() { setMode("remote"); });

	function addProxyOption(value, label) {
		var option = document.createElement("option");
		option.value = value;
		option.textContent = label;
		ui.proxyRegion.appendChild(option);
	}

	function configuredProxyUrl() {
		if (ui.proxyRegion.value === "custom")
			return ui.proxyUrl.value.trim();
		var selected = (config.proxies || []).find(function(proxy) {
			return String(proxy.id) === ui.proxyRegion.value;
		});
		return selected ? String(selected.url || "").trim() : "";
	}

	function initializeProxies() {
		addProxyOption("none", "Singleplayer only — no proxy");
		(config.proxies || []).forEach(function(proxy) {
			if (proxy && proxy.id && proxy.url)
				addProxyOption(String(proxy.id), String(proxy.label || proxy.id));
		});
		addProxyOption("custom", "Custom / self-hosted");

		var initial = query.get("proxy") || Module["luantiProxyUrl"] || safeStorageGet("luanti.proxyUrl");
		var match = (config.proxies || []).find(function(proxy) { return proxy.url === initial; });
		if (match) {
			ui.proxyRegion.value = String(match.id);
		} else if (initial) {
			ui.proxyRegion.value = "custom";
			ui.proxyUrl.value = initial;
		} else {
			ui.proxyRegion.value = "none";
		}
		ui.customProxyRow.hidden = ui.proxyRegion.value !== "custom";
	}

	ui.proxyRegion.addEventListener("change", function() {
		ui.customProxyRow.hidden = ui.proxyRegion.value !== "custom";
	});

	function validateProxy(remote) {
		var url = configuredProxyUrl();
		if (remote && !url)
			throw new Error("Remote servers need a multiplayer WebSocket proxy. Choose a hosted region or enter a self-hosted WSS URL.");
		if (!Module["luantiNetwork"])
			throw new Error("The multiplayer transport is not ready yet.");
		Module["luantiNetwork"]["setProxyUrl"](url);
		return url;
	}

	function valueFromQuery(name, fallback) {
		var value = query.get(name);
		return value === null ? fallback : value;
	}

	ui.address.value = valueFromQuery("address", "");
	ui.port.value = valueFromQuery("port", "30000");
	ui.name.value = valueFromQuery("name", safeStorageGet("luanti.playerName"));
	ui.game.value = valueFromQuery("game", "luanti_web");
	ui.language.value = valueFromQuery("language", safeStorageGet("luanti.language"));
	if (["60", "100", "160"].indexOf(query.get("view")) !== -1)
		ui.quality.value = query.get("view");
	else if (["60", "100", "160"].indexOf(safeStorageGet("luanti.viewDistance")) !== -1)
		ui.quality.value = safeStorageGet("luanti.viewDistance");

	// ------------------------------------------------------------------
	// World library. client/web/worlds.js (Module.luantiWorlds) owns every
	// file operation; this section is the launcher UI on top of it. Library
	// operations only run before callMain, while no world is open.

	var worlds = [];
	var selectedWorld = null; // a world name, or null: the game's default world
	var libraryLoaded = false;
	var libraryBusy = false;
	var thumbnailUrls = [];
	var preferredWorld = query.get("worldname");
	/** @type {{pending: boolean, requested: boolean, armedAt: number, attempts: number, timer: any}} */
	var thumbnail = {pending: false, requested: false, armedAt: 0, attempts: 0, timer: null};

	function library() {
		return Module["luantiWorlds"] || null;
	}

	function messageOf(error) {
		return String(error && error.message || error);
	}

	/**
	 * @param {string} tag
	 * @param {Record<string, any>} [attributes]
	 * @param {Array<Node|string>|Node|string} [content]
	 * @returns {any}
	 */
	function make(tag, attributes, content) {
		var node = document.createElement(tag);
		Object.keys(attributes || {}).forEach(function(key) {
			var value = attributes[key];
			if (value === null || value === undefined || value === false)
				return;
			if (key === "text")
				node.textContent = value;
			else if (key === "className")
				node.className = value;
			else
				node.setAttribute(key, value === true ? "" : String(value));
		});
		[].concat(content === undefined ? [] : content).forEach(function(child) {
			node.append(child);
		});
		return node;
	}

	function setLibraryStatus(text, state) {
		ui.libraryStatus.textContent = text || "";
		ui.libraryStatus.hidden = !text;
		if (state)
			ui.libraryStatus.dataset.state = state;
		else
			delete ui.libraryStatus.dataset.state;
	}

	function sizeText(bytes) {
		if (bytes === null || bytes === undefined)
			return "";
		if (bytes < 1024 * 1024)
			return Math.max(1, Math.round(bytes / 1024)) + " KB";
		if (bytes < 1024 * 1024 * 1024)
			return (bytes / 1048576).toFixed(bytes < 10485760 ? 1 : 0) + " MB";
		return (bytes / 1073741824).toFixed(1) + " GB";
	}

	function relativeTime(ms) {
		var seconds = (ms - Date.now()) / 1000;
		/** @type {Array<[Intl.RelativeTimeFormatUnit, number]>} */
		var units = [["year", 31536000], ["month", 2592000], ["week", 604800], ["day", 86400],
			["hour", 3600], ["minute", 60]];
		var format = typeof Intl !== "undefined" && Intl.RelativeTimeFormat ?
			new Intl.RelativeTimeFormat(undefined, {numeric: "auto"}) : null;
		for (var i = 0; i < units.length; i++) {
			if (Math.abs(seconds) >= units[i][1] || i === units.length - 1) {
				var value = Math.round(seconds / units[i][1]);
				if (!format)
					return Math.abs(value) + " " + units[i][0] + "(s) ago";
				return format.format(value, units[i][0]);
			}
		}
		return "";
	}

	function gameTitle(id) {
		var option = Array.prototype.find.call(ui.game.options, function(entry) {
			return entry.value === id;
		});
		return option ? option.textContent : id;
	}

	function worldsForGame(game) {
		return worlds.filter(function(world) {
			return world.game === game;
		}).sort(function(a, b) {
			var at = a.lastPlayed || a.modified || a.created || 0;
			var bt = b.lastPlayed || b.modified || b.created || 0;
			return bt - at || a.name.localeCompare(b.name);
		});
	}

	function refreshWorlds() {
		var lib = library();
		if (!lib)
			return Promise.resolve();
		return lib.list().then(function(list) {
			worlds = list;
			renderWorlds();
			updateStorageNote();
		}, function(error) {
			setLibraryStatus("Could not read saved worlds: " + messageOf(error), "error");
		});
	}

	function updateStorageNote() {
		var lib = library();
		if (!lib)
			return;
		lib.storage().then(function(info) {
			var text = "Worlds are saved in this browser";
			if (info.usage !== null)
				text += " (" + sizeText(info.usage) + " used)";
			text += ". Export one to keep a copy.";
			if (info.persisted === false)
				text += " The browser may clear them if it runs out of space.";
			ui.storageNote.textContent = text;
		});
	}

	function selectWorld(name) {
		selectedWorld = name;
		Array.prototype.forEach.call(ui.worldList.querySelectorAll(".world-entry"), function(row) {
			var match = row.dataset.world === name;
			row.classList.toggle("is-selected", match);
			var radio = row.querySelector("input[type=radio]");
			if (radio)
				radio.checked = match;
		});
	}

	function renderWorlds() {
		var game = ui.game.value;
		var list = worldsForGame(game);
		thumbnailUrls.forEach(function(url) { URL.revokeObjectURL(url); });
		thumbnailUrls = [];
		ui.worldList.replaceChildren();
		ui.worldList.setAttribute("aria-busy", "false");

		var names = list.map(function(world) { return world.name; });
		if (preferredWorld && names.indexOf(preferredWorld) !== -1) {
			selectedWorld = preferredWorld;
			preferredWorld = null;
		} else if (names.indexOf(selectedWorld) === -1) {
			// The game's legacy world (named after the game) first, then the
			// most recently played one.
			selectedWorld = names.indexOf(game) !== -1 ? game : names[0] || null;
		}

		if (!list.length) {
			ui.worldList.append(make("p", {"className": "world-empty"},
				"No worlds for " + gameTitle(game) + " yet. Start world creates one, or use New world to choose a name and seed."));
			return;
		}

		list.forEach(function(world) {
			var details = [];
			if (world.lastPlayed)
				details.push("Played " + relativeTime(world.lastPlayed));
			else if (world.created || world.modified)
				details.push("Created " + relativeTime(world.created || world.modified));
			else
				details.push("Never played");
			details.push(sizeText(world.size));
			var thumb = make("span", {"className": "world-thumb", "aria-hidden": "true"}, "▦");
			var radio = make("input", {"type": "radio", "name": "world", "value": world.name});
			var row = make("div", {"className": "world-entry", "data-world": world.name}, [
				make("label", {"className": "world-pick"}, [
					radio,
					thumb,
					make("span", {"className": "world-text"}, [
						make("strong", {"text": world.name}),
						make("small", {"text": details.join(" · ")})
					])
				]),
				make("details", {"className": "world-menu"}, [
					make("summary", {"aria-label": "Actions for " + world.name, "title": "World actions"}, "•••"),
					make("div", {"className": "world-menu-items"}, [
						make("button", {"type": "button", "data-action": "rename", "data-world": world.name}, "Rename"),
						make("button", {"type": "button", "data-action": "duplicate", "data-world": world.name}, "Duplicate"),
						make("button", {"type": "button", "data-action": "export", "data-world": world.name}, "Export"),
						make("button", {"type": "button", "data-action": "delete", "data-world": world.name,
							"className": "is-danger"}, "Delete")
					])
				])
			]);
			radio.addEventListener("change", function() { selectWorld(world.name); });
			ui.worldList.append(row);
			if (world.hasThumbnail) {
				library().thumbnail(world.name).then(function(blob) {
					if (!blob)
						return;
					var url = URL.createObjectURL(blob);
					thumbnailUrls.push(url);
					thumb.replaceChildren(make("img", {"src": url, "alt": ""}));
				}).catch(function() {});
			}
		});
		selectWorld(selectedWorld);
	}

	ui.game.addEventListener("change", function() {
		selectedWorld = null;
		renderWorlds();
	});

	// Only one world menu open at a time; a click elsewhere closes it.
	document.addEventListener("click", function(event) {
		var target = /** @type {Element} */ (event.target);
		Array.prototype.forEach.call(ui.worldList.querySelectorAll(".world-menu[open]"), function(menu) {
			if (!menu.contains(target))
				menu.open = false;
		});
	});

	ui.worldList.addEventListener("click", function(event) {
		var button = /** @type {Element} */ (event.target).closest("button[data-action]");
		if (!button)
			return;
		var menu = button.closest("details");
		if (menu)
			menu.open = false;
		var name = button.getAttribute("data-world");
		var action = button.getAttribute("data-action");
		if (action === "rename")
			renameWorld(name);
		else if (action === "duplicate")
			duplicateWorld(name);
		else if (action === "export")
			exportWorld(name);
		else if (action === "delete")
			deleteWorld(name);
	});

	// --- dialogs

	function showDialogError(error) {
		ui.dialogError.textContent = messageOf(error);
		ui.dialogError.hidden = false;
	}

	/**
	 * Opens the shared modal. `accept` runs on confirm and may return a
	 * promise; a rejection is shown inside the dialog, which stays open.
	 * Resolves with accept's result, or null when cancelled.
	 * @param {{title: string, body: any, confirm?: string|null, cancel?: string, danger?: boolean,
	 *   accept?: () => any}} options
	 * @returns {Promise<any>}
	 */
	function openDialog(options) {
		return new Promise(function(resolve) {
			var working = false;
			ui.dialogTitle.textContent = options.title;
			ui.dialogBody.replaceChildren.apply(ui.dialogBody, [].concat(options.body || []));
			ui.dialogError.hidden = true;
			ui.dialogConfirm.hidden = options.confirm === null;
			ui.dialogConfirm.disabled = false;
			ui.dialogConfirm.textContent = options.confirm || "OK";
			ui.dialogConfirm.classList.toggle("is-danger", !!options.danger);
			ui.dialogCancel.textContent = options.cancel || "Cancel";
			ui.dialogCancel.disabled = false;

			function cleanup() {
				ui.dialogForm.removeEventListener("submit", onSubmit);
				ui.dialogCancel.removeEventListener("click", onCancel);
				ui.dialog.removeEventListener("cancel", onCancel);
			}
			function finish(value) {
				cleanup();
				if (ui.dialog.open)
					ui.dialog.close();
				resolve(value);
			}
			function onCancel(event) {
				event.preventDefault();
				if (!working)
					finish(null);
			}
			function onSubmit(event) {
				event.preventDefault();
				if (working)
					return;
				ui.dialogError.hidden = true;
				working = true;
				ui.dialogConfirm.disabled = true;
				ui.dialogCancel.disabled = true;
				Promise.resolve().then(function() {
					return options.accept ? options.accept() : true;
				}).then(finish, function(error) {
					working = false;
					ui.dialogConfirm.disabled = false;
					ui.dialogCancel.disabled = false;
					showDialogError(error);
				});
			}
			ui.dialogForm.addEventListener("submit", onSubmit);
			ui.dialogCancel.addEventListener("click", onCancel);
			ui.dialog.addEventListener("cancel", onCancel);
			ui.dialog.showModal();
			var first = ui.dialogBody.querySelector("input, select");
			(first || (ui.dialogConfirm.hidden ? ui.dialogCancel : ui.dialogConfirm)).focus();
			if (first && first.select)
				first.select();
		});
	}

	function field(label, control) {
		return [make("label", {"for": control.id, "text": label}), control];
	}

	function modeSelect(id) {
		return make("select", {"id": id}, [
			make("option", {"value": ""}, "Game default"),
			make("option", {"value": "true"}, "On"),
			make("option", {"value": "false"}, "Off")
		]);
	}

	function runLibrary(task) {
		if (libraryBusy)
			return Promise.resolve(null);
		libraryBusy = true;
		showError("");
		return Promise.resolve().then(task).catch(function(error) {
			setLibraryStatus("");
			showError(messageOf(error));
			return null;
		}).finally(function() {
			libraryBusy = false;
		});
	}

	function newWorld() {
		var lib = library();
		if (!lib || libraryBusy)
			return;
		var name = make("input", {"id": "luanti-new-world-name", "maxlength": "48", "autocomplete": "off",
			"spellcheck": "false"});
		var game = make("select", {"id": "luanti-new-world-game"});
		Array.prototype.forEach.call(ui.game.options, function(option) {
			game.append(make("option", {"value": option.value}, option.textContent));
		});
		game.value = ui.game.value;
		var seed = make("input", {"id": "luanti-new-world-seed", "maxlength": "64", "placeholder": "Random",
			"autocomplete": "off", "spellcheck": "false"});
		var creative = modeSelect("luanti-new-world-creative");
		var damage = modeSelect("luanti-new-world-damage");
		var nameEdited = false;
		name.addEventListener("input", function() { nameEdited = true; });
		var suggest = function() {
			if (!nameEdited)
				lib.suggestName(gameTitle(game.value)).then(function(value) { name.value = value; });
		};
		game.addEventListener("change", suggest);
		suggest();
		var mode = function(select) {
			return select.value === "" ? undefined : select.value === "true";
		};
		openDialog({
			"title": "New world",
			"body": [].concat(field("Name", name), field("Game", game), field("Seed (optional)", seed),
				make("div", {"className": "field-grid"}, [
					make("div", {}, field("Creative mode", creative)),
					make("div", {}, field("Damage", damage))
				])),
			"confirm": "Create world",
			"accept": function() {
				return lib.create({"name": name.value, "game": game.value, "seed": seed.value,
					"creative": mode(creative), "damage": mode(damage)});
			}
		}).then(function(world) {
			if (!world)
				return;
			ui.game.value = world.game;
			selectedWorld = world.name;
			return refreshWorlds();
		});
	}

	function renameWorld(from) {
		var lib = library();
		var name = make("input", {"id": "luanti-rename-world", "maxlength": "48", "value": from,
			"autocomplete": "off", "spellcheck": "false"});
		openDialog({
			"title": "Rename world",
			"body": field("New name", name),
			"confirm": "Rename",
			"accept": function() { return lib.rename(from, name.value); }
		}).then(function(world) {
			if (!world)
				return;
			selectedWorld = world.name;
			return refreshWorlds();
		});
	}

	function duplicateWorld(name) {
		runLibrary(function() {
			setLibraryStatus("Copying " + name + "…");
			return library().duplicate(name).then(function(world) {
				selectedWorld = world.name;
				setLibraryStatus("Copied to " + world.name + ".", "ready");
				return refreshWorlds();
			});
		});
	}

	function deleteWorld(name) {
		var lib = library();
		openDialog({
			"title": "Delete " + name + "?",
			"body": make("p", {}, "This removes the world from this browser for good. " +
				"Export it first if you might want it back."),
			"confirm": "Delete world",
			"danger": true,
			"accept": function() { return lib.remove(name); }
		}).then(function(done) {
			if (done === null)
				return;
			if (selectedWorld === name)
				selectedWorld = null;
			setLibraryStatus("Deleted " + name + ".", "ready");
			return refreshWorlds();
		});
	}

	function downloadBlob(blob, fileName) {
		var url = URL.createObjectURL(blob);
		var link = make("a", {"href": url, "download": fileName});
		document.body.append(link);
		link.click();
		link.remove();
		setTimeout(function() { URL.revokeObjectURL(url); }, 60000);
	}

	function exportWorld(name) {
		var lib = library();
		var fileName = name + ".luantiworld";
		var progress = function(done, total) {
			setLibraryStatus("Exporting " + name + "… " + (total ? Math.floor(done / total * 100) : 0) + "%");
		};
		// The save picker needs the click's user activation, so it opens
		// before anything else is awaited.
		var picker = /** @type {any} */ (window)["showSaveFilePicker"];
		var target = typeof picker === "function" ?
			picker({"suggestedName": fileName, "types": [{"description": "Luanti world",
				"accept": {"application/zip": [".luantiworld"]}}]}).then(function(handle) {
				return handle.createWritable();
			}, function(error) {
				if (error && error.name === "AbortError")
					return "cancelled";
				return null; // Picker refused (e.g. in an iframe): fall back to a download.
			}) :
			Promise.resolve(null);
		runLibrary(function() {
			return target.then(function(writable) {
				if (writable === "cancelled")
					return;
				progress(0, 1);
				return lib.exportWorld(name, {"writable": writable || undefined, "onProgress": progress})
					.then(function(blob) {
						if (blob)
							downloadBlob(blob, fileName);
						setLibraryStatus("Exported " + name + " as " + fileName + ".", "ready");
					});
			});
		});
	}

	function planDetails(plan, source) {
		var rows = [
			["Game", plan.gameTitle + (plan.gameAvailable ? "" : " (not available here)")],
			["Size", sizeText(plan.archiveBytes) + " download, " + sizeText(plan.uncompressedBytes) + " unpacked"],
			["Files", String(plan.files)]
		];
		if (plan.exported)
			rows.push(["Exported", new Date(plan.exported).toLocaleString() +
				(plan.engine ? " · Luanti " + plan.engine : "")]);
		var mods = plan.content.filter(function(item) { return item.type === "mod"; });
		if (mods.length)
			rows.push(["Mods", mods.map(function(item) { return item.name; }).join(", ")]);
		if (source)
			rows.push(["From", source]);
		var list = make("dl");
		rows.forEach(function(row) {
			list.append(make("dt", {"text": row[0]}), make("dd", {"text": row[1]}));
		});
		return list;
	}

	// Shows what an archive contains and imports it only if the player
	// confirms. Resolves with the imported world or null.
	function confirmImport(blob, plan, options) {
		var lib = library();
		return lib.suggestName(plan.name).then(function(suggested) {
			var name = make("input", {"id": "luanti-import-name", "maxlength": "48", "value": suggested,
				"autocomplete": "off", "spellcheck": "false"});
			var body = [
				make("p", {}, options.host ?
					"A link asked to add a shared world to this browser. Nothing is saved unless you import it." :
					"Check the world before adding it to this browser.")
			].concat(field("Save as", name), planDetails(plan, options.host || null));
			if (plan.warnings.length)
				body.push(make("ul", {}, plan.warnings.map(function(text) { return make("li", {"text": text}); })));
			if (!plan.gameAvailable)
				body.push(make("p", {"className": "form-error"},
					"This world needs the game \"" + plan.game + "\", which is not available in this browser."));
			return openDialog({
				"title": options.host ? "Open shared world?" : "Import world?",
				"body": body,
				"confirm": plan.gameAvailable ? "Import world" : null,
				"cancel": plan.gameAvailable ? "Cancel" : "Close",
				"accept": function() {
					return lib.importArchive(blob, {
						"name": name.value,
						"fileName": options.fileName,
						"source": options.source,
						"onProgress": function(done, total) {
							ui.dialogConfirm.textContent = "Importing… " + (total ? Math.floor(done / total * 100) : 0) + "%";
						}
					});
				}
			});
		}).then(function(world) {
			if (!world)
				return null;
			ui.game.value = world.game;
			selectedWorld = world.name;
			setLibraryStatus("Imported " + world.name + ".", "ready");
			return refreshWorlds().then(function() { return world; });
		});
	}

	function importFile(file) {
		var lib = library();
		if (!lib || !file || libraryBusy)
			return;
		runLibrary(function() {
			setLibraryStatus("Reading " + file.name + "…");
			return lib.inspectArchive(file, {"fileName": file.name}).then(function(plan) {
				setLibraryStatus("");
				return confirmImport(file, plan, {"fileName": file.name, "source": null});
			});
		});
	}

	ui.newWorld.addEventListener("click", newWorld);
	ui.importWorld.addEventListener("click", function() { ui.importFile.click(); });
	ui.importFile.addEventListener("change", function() {
		var file = ui.importFile.files && ui.importFile.files[0];
		ui.importFile.value = "";
		importFile(file);
	});

	function hasFiles(event) {
		var types = event.dataTransfer && event.dataTransfer.types;
		return !!types && Array.prototype.indexOf.call(types, "Files") !== -1;
	}
	ui.localPanel.addEventListener("dragover", function(event) {
		if (!hasFiles(event) || started)
			return;
		event.preventDefault();
		ui.worldList.classList.add("is-dropping");
	});
	ui.localPanel.addEventListener("dragleave", function(event) {
		if (!ui.localPanel.contains(/** @type {Node} */ (event.relatedTarget)))
			ui.worldList.classList.remove("is-dropping");
	});
	ui.localPanel.addEventListener("drop", function(event) {
		ui.worldList.classList.remove("is-dropping");
		if (!hasFiles(event) || started)
			return;
		event.preventDefault();
		importFile(event.dataTransfer.files[0]);
	});

	// ?world=<https url> offers a shared world. It is downloaded for
	// inspection only, and imported once the player confirms.
	function openSharedWorldFromQuery() {
		var url = query.get("world");
		var lib = library();
		if (!url || !lib)
			return;
		var forget = function() {
			var clean = new URL(location.href);
			clean.searchParams.delete("world");
			history.replaceState(history.state, "", clean.href);
		};
		runLibrary(function() {
			setLibraryStatus("Downloading a shared world…");
			return lib.fetchShared(url, {"onProgress": function(done, total) {
				setLibraryStatus("Downloading a shared world… " + sizeText(done) + (total ? " of " + sizeText(total) : ""));
			}}).then(function(shared) {
				setLibraryStatus("");
				return confirmImport(shared.blob, shared.plan,
					{"fileName": shared.fileName, "source": shared.url, "host": shared.host});
			}).catch(function(error) {
				throw new Error("Could not open the shared world: " + messageOf(error));
			}).finally(forget);
		});
	}

	// Called on Start for a local session: the selected world, or the
	// game's default world (named after the game) created on first use.
	function prepareWorld(session) {
		var lib = library();
		if (!lib) {
			session.world = session.world || session.game;
			return Promise.resolve();
		}
		var name = session.world;
		var made = name ? Promise.resolve() :
			lib.create({"name": session.game, "game": session.game}).catch(function(error) {
				// A directory the engine created but never finished (no
				// world.mt) is reused as is; the engine completes it.
				if (error && error.code !== "exists")
					throw error;
			});
		name = name || session.game;
		session.world = name;
		return made.then(function() {
			return lib.markPlayed(name).catch(function() {});
		});
	}

	// --- thumbnails

	// The first save after the player has been in the world for a few
	// seconds (or 20 s, whichever is first) asks for the next presented frame
	// to be kept as the world's thumbnail.
	function armThumbnail() {
		if (thumbnail.armedAt || !currentSession || currentSession.mode !== "local" || !library())
			return;
		thumbnail.pending = true;
		thumbnail.armedAt = Date.now();
		thumbnail.timer = setTimeout(function() { thumbnail.requested = thumbnail.pending; }, 20000);
	}

	function captureThumbnail() {
		if (!thumbnail.requested || !thumbnail.pending)
			return;
		var source = Module.canvas;
		if (!source || !source.width || !source.height)
			return;
		var out = document.createElement("canvas");
		out.width = 320;
		out.height = 180;
		var context = out.getContext("2d");
		var scale = Math.max(out.width / source.width, out.height / source.height);
		var width = out.width / scale;
		var height = out.height / scale;
		try {
			context.drawImage(source, (source.width - width) / 2, (source.height - height) / 2, width, height,
				0, 0, out.width, out.height);
			// A WebGL drawing buffer that was already presented reads back
			// blank; try again on a later frame rather than keep a black tile.
			var pixels = context.getImageData(0, 0, out.width, out.height).data;
			var low = 255;
			var high = 0;
			for (var i = 0; i < pixels.length; i += 4 * 211) {
				var light = (pixels[i] + pixels[i + 1] + pixels[i + 2]) / 3;
				low = Math.min(low, light);
				high = Math.max(high, light);
			}
			if (high - low < 12) {
				if (++thumbnail.attempts > 600)
					thumbnail.pending = false;
				return;
			}
		} catch (error) {
			thumbnail.pending = false;
			appendLog("library", ["thumbnail capture failed: " + messageOf(error)]);
			return;
		}
		thumbnail.pending = false;
		clearTimeout(thumbnail.timer);
		var world = currentSession.world;
		out.toBlob(function(blob) {
			if (blob)
				library().saveThumbnail(world, blob).catch(function(error) {
					appendLog("library", ["thumbnail save failed: " + messageOf(error)]);
				});
		}, "image/jpeg", 0.82);
	}

	function validatedSession() {
		var remote = activeMode === "remote";
		var name = ui.name.value.trim();
		if (name && !/^[A-Za-z0-9_-]{1,19}$/.test(name))
			throw new Error("Player names may contain up to 19 letters, numbers, dashes, or underscores.");
		if (remote && !name)
			throw new Error("Choose a player name before joining a server.");

		var port = Number(ui.port.value);
		var address = ui.address.value.trim();
		if (remote && !/^[A-Za-z0-9.-]+$/.test(address))
			throw new Error("Enter a hostname or IPv4 address without a URL scheme or path.");
		if (remote && (!Number.isInteger(port) || port < 1 || port > 65535))
			throw new Error("The server port must be between 1 and 65535.");

		return {
			mode: remote ? "remote" : "local",
			address: address,
			port: port,
			name: name,
			game: ui.game.value || "luanti_web",
			world: remote ? null : selectedWorld,
			view: ui.quality.value,
			language: ui.language.value
		};
	}

	function argumentsForSession(session) {
		var args = ["--go", "--viewing-range", session.view];
		if (session.language)
			args.push("--language", session.language);
		if (session.mode === "remote")
			args.push("--address", session.address, "--port", String(session.port), "--name", session.name);
		else
			args.push("--gameid", session.game,
				"--world", "/home/web_user/.luanti/worlds/" + session.world);
		return args;
	}

	function syncCanvasBackingStore() {
		var canvas = Module.canvas;
		if (!canvas)
			return;
		// After the engine owns the WebGL context, only CIrrDeviceSDL may
		// change canvas.width/height. Direct assignment here would desync the
		// OFFSCREEN_FRAMEBUFFER backing store from the CSS box.
		if (started)
			return;
		var rect = canvas.getBoundingClientRect();
		var width = Math.max(1, Math.round(rect.width));
		var height = Math.max(1, Math.round(rect.height));
		if (canvas.width !== width || canvas.height !== height) {
			canvas.width = width;
			canvas.height = height;
		}
	}

	function unlockAudio() {
		// audio_unlock.js (a --pre-js) owns the OpenAL contexts and logs them.
		if (typeof Module !== "undefined" && typeof Module["luantiResumeAudio"] === "function") {
			try {
				Module["luantiResumeAudio"]("launcher");
			} catch (_) {}
		}
		try {
			if (typeof window.AudioContext !== "undefined" || typeof window.webkitAudioContext !== "undefined") {
				if (window.AL && window.AL.contexts) {
					for (var ctxId in window.AL.contexts) {
						var ctx = window.AL.contexts[ctxId];
						if (ctx && ctx.audioCtx && ctx.audioCtx.state === "suspended") {
							ctx.audioCtx.resume().catch(function() {});
						}
					}
				}
				if (typeof Module !== "undefined" && Module["SDL2"] && Module["SDL2"].audioContext && Module["SDL2"].audioContext.state === "suspended") {
					Module["SDL2"].audioContext.resume().catch(function() {});
				}
			}
		} catch (_) {}
	}

	function abortLaunch(error) {
		started = false;
		launchInFlight = false;
		ui.play.disabled = !(runtimeReady && coreReady);
		ui.launcher.hidden = false;
		ui.gameShell.hidden = true;
		showError(String(error && error.message || error));
		// prepareWorld may have created the world before the launch failed.
		refreshWorlds();
	}

	ui.form.addEventListener("submit", function(event) {
		event.preventDefault();
		if (started || launchInFlight)
			return;
		unlockAudio();
		try {
			if (!runtimeReady || typeof Module["callMain"] !== "function")
				throw new Error("The engine is still preparing. Try again in a moment.");
			var session = validatedSession();
			validateProxy(session.mode === "remote");
			currentSession = session;
			safeStorageSet("luanti.playerName", session.name);
			safeStorageSet("luanti.language", session.language);
			safeStorageSet("luanti.viewDistance", session.view);
			ui.play.disabled = true;
			ui.launcher.hidden = true;
			ui.gameShell.hidden = false;
			launchInFlight = true;
			backgroundStarted = false;
			var launchArgs = session.mode === "local" ? null : argumentsForSession(session);
			var label = ui.game.selectedOptions && ui.game.selectedOptions[0] ?
				ui.game.selectedOptions[0].textContent : session.game;
			// A local world needs its game's files in the filesystem; a
			// remote server brings its own content.
			var gameReady = session.mode === "local" ?
				prepareWorld(session).then(function() {
					launchArgs = argumentsForSession(session);
					reportStatus("Downloading " + label + "…", 0, "assets");
					return Module["luantiPackages"]["loadGame"](session.game, function(loaded, total) {
						reportDownload(label, loaded, total);
					});
				}) :
				Promise.resolve();
			gameReady.then(function() {
				reportStatus("Starting Luanti…", 4, "engine");
				requestAnimationFrame(function() {
					syncCanvasBackingStore();
					requestAnimationFrame(function() {
						syncCanvasBackingStore();
						try {
							if (typeof Module["callMain"] !== "function")
								throw new Error("The engine is still preparing. Try again in a moment.");
							started = true;
							Module["callMain"](launchArgs);
						} catch (error) {
							abortLaunch(error);
						}
					});
				});
			}, function(error) {
				if (error && error.name === "LuantiWorldError") {
					abortLaunch(error);
					return;
				}
				abortLaunch(new Error("Could not download " + label + " (" +
					String(error && error.message || error) +
					"). Check your connection and try again."));
			});
		} catch (error) {
			abortLaunch(error);
		}
	});

	function renderServers(payload) {
		var servers = payload && Array.isArray(payload.list) ? payload.list : [];
		servers = servers.filter(function(server) {
			return server && server.address && Number(server.port) > 0;
		}).sort(function(a, b) {
			return Number(b.clients || 0) - Number(a.clients || 0);
		}).slice(0, 16);
		ui.serverList.replaceChildren();
		if (!servers.length)
			throw new Error("The public directory returned no compatible servers.");
		servers.forEach(function(server) {
			var button = document.createElement("button");
			button.type = "button";
			button.className = "server-entry";
			var title = document.createElement("strong");
			title.textContent = server.name || server.address;
			var count = document.createElement("small");
			count.textContent = String(server.clients || 0) + "/" + String(server.clients_max || "?") + (server.password ? " · password" : "");
			var address = document.createElement("span");
			address.className = "server-address";
			address.textContent = server.address + ":" + server.port;
			button.append(title, count, address);
			button.addEventListener("click", function() {
				ui.address.value = server.address;
				ui.port.value = server.port;
				ui.serverList.hidden = true;
				ui.address.focus();
			});
			ui.serverList.appendChild(button);
		});
		ui.serverList.hidden = false;
	}

	ui.serverListButton.addEventListener("click", async function() {
		ui.serverListButton.disabled = true;
		ui.serverListButton.textContent = "Loading server directory…";
		showError("");
		try {
			var response = await fetch(config.serverListUrl || "https://servers.luanti.org/list.json", {mode: "cors"});
			if (!response.ok)
				throw new Error("Server directory request failed (HTTP " + response.status + ").");
			renderServers(await response.json());
			ui.serverListButton.textContent = "Refresh public servers";
		} catch (error) {
			showError("Could not load the public server directory. Direct connect still works. " + String(error.message || error));
			ui.serverListButton.textContent = "Try public servers again";
		} finally {
			ui.serverListButton.disabled = false;
		}
	});

	ui.share.addEventListener("click", async function() {
		if (!currentSession)
			return;
		var url = new URL(location.href);
		url.search = "";
		url.searchParams.set("game", currentSession.game);
		url.searchParams.set("view", currentSession.view);
		if (currentSession.mode === "remote") {
			url.searchParams.set("address", currentSession.address);
			url.searchParams.set("port", currentSession.port);
		}
		try {
			await navigator.clipboard.writeText(url.href);
			ui.share.textContent = "Link copied";
			setTimeout(function() {
				ui.share.textContent = currentSession.mode === "remote" ? "Copy invite link" : "Copy launcher link";
			}, 1800);
		} catch (_) {
			window.prompt("Copy this link", url.href);
		}
	});

	ui.fullscreen.addEventListener("click", function() {
		if (document.fullscreenElement)
			document.exitFullscreen();
		else
			ui.gameShell.requestFullscreen().catch(function(error) { appendLog("error", [error]); });
	});

	ui.consoleButton.addEventListener("click", function() {
		ui.consolePanel.open = !ui.consolePanel.open;
		ui.consoleButton.setAttribute("aria-expanded", String(ui.consolePanel.open));
	});
	element("luanti-console-clear").addEventListener("click", function() {
		logs = [];
		ui.consoleOutput.textContent = "";
	});

	document.addEventListener("pointerlockchange", function() {
		if (document.pointerLockElement === Module.canvas) {
			hadPointerLock = true;
			ui.resume.hidden = true;
		} else if (hadPointerLock && started && activePhase === "playing") {
			ui.resume.hidden = false;
		}
	});

	window.addEventListener("resize", syncCanvasBackingStore);

	// WebGL context loss. The engine pauses rendering (CIrrDeviceSDL.cpp) but
	// cannot rebuild its GL resources, so the recoverable path is a reload.
	// World data is flushed to IndexedDB first.
	var glLostCount = 0;
	var glRestoredCount = 0;
	var glLostSaved = false;
	var glOverlayActive = false;
	// The engine keeps asking for pointer lock while the game runs; with the
	// overlay up a locked pointer would route every click to the canvas and
	// the reload button could never be pressed.
	var nativeRequestPointerLock = Module.canvas.requestPointerLock;
	Module.canvas.requestPointerLock = function() {
		if (glOverlayActive)
			return undefined;
		return nativeRequestPointerLock.apply(this, arguments);
	};
	document.addEventListener("pointerlockchange", function() {
		if (glOverlayActive && document.pointerLockElement)
			document.exitPointerLock();
	});
	function enableGlReload(label) {
		ui.glLostReload.disabled = false;
		ui.glLostReload.textContent = label;
	}
	Module.canvas.addEventListener("webglcontextlost", function(event) {
		// preventDefault lets the browser restore the context later.
		event.preventDefault();
		handleContextLost();
	});
	function handleContextLost() {
		glLostCount++;
		appendLog("error", ["Luanti: WebGL context lost"]);
		if (!started)
			return;
		ui.glLost.hidden = false;
		ui.glLostReload.disabled = true;
		ui.glLostReload.textContent = "Saving…";
		glOverlayActive = true;
		if (document.pointerLockElement === Module.canvas)
			document.exitPointerLock();
		var persistence = Module["luantiPersistence"];
		var saved = persistence && persistence.requestSync ?
			persistence.requestSync({"reason": "webglcontextlost", "urgent": true}) :
			Promise.resolve();
		saved.then(function() {
			glLostSaved = true;
			enableGlReload("Reload and resume");
		}, function() {
			enableGlReload("Reload anyway");
		});
	}
	Module.canvas.addEventListener("webglcontextrestored", function() {
		glRestoredCount++;
		appendLog("info", ["Luanti: WebGL context restored"]);
		if (started)
			ui.glLostText.textContent = "Graphics are back, but the game has to reload its textures. " +
				(glLostSaved ? "Your world has been saved." : "Saving your world…");
	});
	ui.glLostReload.addEventListener("click", function() {
		location.reload();
	});

	Module.canvas.addEventListener("click", function() {
		unlockAudio();
		if (started && activePhase === "playing" && document.pointerLockElement !== Module.canvas) {
			Module.canvas.requestPointerLock();
			Module.canvas.focus();
		}
	});

	ui.resumeButton.addEventListener("click", function() {
		unlockAudio();
		Module.canvas.requestPointerLock();
		Module.canvas.focus();
	});

	document.addEventListener("click", unlockAudio, {capture: true, passive: true});
	document.addEventListener("keydown", unlockAudio, {capture: true, passive: true});
	document.addEventListener("touchstart", unlockAudio, {capture: true, passive: true});

	if (window.matchMedia("(pointer: coarse)").matches)
		element("luanti-mobile-warning").hidden = false;

	initializeProxies();
	setMode(activeMode);

	// Narrow, versioned service worker (service-worker.js): precaches this
	// build as one verified set, caches content-addressed asset packages
	// forever, and never serves a mixed or stale engine. A new build waits
	// until the user accepts it here.
	if ("serviceWorker" in navigator && (location.protocol === "https:" || location.hostname === "localhost" || location.hostname === "127.0.0.1")) {
		var reloadingForUpdate = false;

		/** @param {() => void} apply */
		var offerUpdate = function(apply) {
			ui.update.hidden = false;
			ui.updateReload.onclick = function() {
				ui.updateReload.disabled = true;
				apply();
			};
		};

		// A worker that installed while this page was already controlled is a
		// new build of this same deployment; it activates when told to.
		/** @param {ServiceWorker} worker */
		var offerWorker = function(worker) {
			offerUpdate(function() { worker.postMessage({type: "SKIP_WAITING"}); });
		};

		window.addEventListener("load", function() {
			var hadController = !!navigator.serviceWorker.controller;
			navigator.serviceWorker.addEventListener("controllerchange", function() {
				// First install claims the page; only an update needs a reload.
				if (!hadController || reloadingForUpdate)
					return;
				reloadingForUpdate = true;
				location.reload();
			});
			navigator.serviceWorker.register("service-worker.js", {scope: "./"}).then(function(registration) {
				if (registration.waiting && navigator.serviceWorker.controller)
					offerWorker(registration.waiting);
				registration.addEventListener("updatefound", function() {
					var installing = registration.installing;
					if (!installing)
						return;
					installing.addEventListener("statechange", function() {
						if (installing.state === "installed" && navigator.serviceWorker.controller)
							offerWorker(installing);
					});
				});
				registration.update().catch(function() {});
			}).catch(function(error) {
				appendLog("service-worker", [error]);
			});
		});
	}

	// Versioned deployments (package_demo.py) serve every build from
	// releases/<version>/, each with its own service-worker scope, so a worker
	// update never arrives. Compare with the site's current version instead.
	var release = /\/releases\/([^/]+)\/[^/]*$/.exec(location.pathname);
	if (release && location.protocol !== "file:") {
		fetch(new URL("../../version.json", location.href), {cache: "no-store"}).then(function(response) {
			return response.ok ? response.json() : null;
		}).then(function(site) {
			if (!site || !site.entrypoint || site.version === release[1])
				return;
			ui.update.hidden = false;
			ui.updateReload.onclick = function() {
				location.assign(new URL("../../" + site.entrypoint, location.href).href + location.search);
			};
		}).catch(function() {});
	}
})();
