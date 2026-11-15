// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

(function() {
	"use strict";

	/** @type {Partial<LuantiWebConfig>} */
	var config = window.LUANTI_WEB_CONFIG || {};
	var query = new URLSearchParams(location.search);
	var runtimeReady = false;
	var coreReady = false;
	var backgroundStarted = false;
	var started = false;
	var launchInFlight = false;
	var activeMode = query.get("address") ? "remote" : "local";
	var activePhase = "assets";
	var currentSession = null;
	var hadPointerLock = false;
	var loadingShellDismissed = false;
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
		updateReload: element("luanti-update-reload")
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
			if (started && activePhase === "playing")
				dismissLoadingShell();
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
		if (query.get("autostart") === "1")
			ui.form.requestSubmit();
	}

	Module["onRuntimeInitialized"] = function() {
		runtimeReady = true;
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

	window.addEventListener("luanti:persistence", function(event) {
		var state = event.detail || {};
		if (!started && state.state === "loading")
			reportStatus("Loading saved worlds…", 80, "storage");
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
				"--world", "/home/web_user/.luanti/worlds/" + session.game);
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
			var launchArgs = argumentsForSession(session);
			var label = ui.game.selectedOptions && ui.game.selectedOptions[0] ?
				ui.game.selectedOptions[0].textContent : session.game;
			// A local world needs its game's files in the filesystem; a
			// remote server brings its own content.
			var gameReady = session.mode === "local" ?
				(reportStatus("Downloading " + label + "…", 0, "assets"),
					Module["luantiPackages"]["loadGame"](session.game, function(loaded, total) {
						reportDownload(label, loaded, total);
					})) :
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
