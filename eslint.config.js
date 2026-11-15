// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 The Luanti Contributors

// Lint config for the browser-side JavaScript (client/web) and the WASM
// tooling (util/wasm). The client scripts are classic scripts, not modules:
// network.js/persistence.js/webgl_compat.js/audio_unlock.js/perf.js/present.js are Emscripten
// --pre-js files and the rest are loaded with plain <script> tags.

import js from "@eslint/js";
import globals from "globals";

export default [
	{
		ignores: ["node_modules/**", "**/node_modules/**", "build*/**", "irr/**", "lib/**"],
	},
	js.configs.recommended,
	{
		files: ["client/web/**/*.js"],
		languageOptions: {
			ecmaVersion: 2022,
			sourceType: "script",
			globals: {
				...globals.browser,
				// Provided by Emscripten (--pre-js) or by the shell page.
				Module: "writable",
				FS: "readonly",
				IDBFS: "readonly",
				AL: "readonly",
				HEAPU8: "readonly",
				_malloc: "readonly",
				_free: "readonly",
				addRunDependency: "readonly",
				removeRunDependency: "readonly",
				LUANTI_WEB_CONFIG: "readonly",
			},
		},
	},
	{
		// Runs in the service-worker scope, not a window.
		files: ["client/web/service-worker.js"],
		languageOptions: {
			globals: { ...globals.serviceworker },
		},
	},
	{
		files: ["util/wasm/*_pre.js"],
		languageOptions: {
			ecmaVersion: 2022,
			sourceType: "script",
			globals: { ...globals.browser, Module: "writable" },
		},
	},
	{
		files: ["**/*.mjs", "eslint.config.js"],
		languageOptions: {
			ecmaVersion: 2022,
			sourceType: "module",
			globals: { ...globals.node },
		},
	},
	{
		files: ["client/web/**/*.js", "util/wasm/**/*.{js,mjs}", "eslint.config.js"],
		rules: {
			eqeqeq: ["error", "always", { null: "ignore" }],
			"no-var": "off", // legacy classic scripts predate let/const
			"no-unused-vars": ["error", { args: "none", caughtErrors: "none" }],
			"no-undef": "error",
			// `catch (_) {}` is the deliberate "best effort" idiom here
			// (localStorage, socket close); empty if/else blocks are not.
			"no-empty": ["error", { allowEmptyCatch: true }],
		},
	},
];
