# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**luanti_wasm** is a downstream fork of [Luanti](https://github.com/luanti-org/luanti) (formerly Minetest), a C++17 voxel game engine, being ported to WebAssembly via Emscripten to run in web browsers. The primary goal is a playable singleplayer experience (main menu → create/load world → place/break blocks) that persists across page reloads.

WASM-specific code lives in `#ifdef __EMSCRIPTEN__` blocks. Native builds (Linux/Windows/macOS) must remain unchanged.

## Build Commands

### Native Build (Linux)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DRUN_IN_PLACE=TRUE -DBUILD_SERVER=TRUE
cmake --build build --parallel $(($(nproc) + 1))
```

### WASM Build (Emscripten)
```bash
# Activate Emscripten first (CI pins 6.0.3; the preset reads $EMSDK for its toolchain)
source /path/to/emsdk/emsdk_env.sh

util/wasm/build_libzip.sh            # once per SDK/EM_CACHE: libzip is required, no Emscripten port
cmake --preset Emscripten            # binaryDir is build-wasm; Release, client-only
cmake --build build-wasm --parallel $(($(nproc) + 1))
# or: npm run build
# Output: build-wasm/bin/luanti.{html,js,wasm}, asset-manifest.json, assets/*.data + launcher/PWA companions
```
Full guide: `doc/compiling/wasm.md`.

### Serve WASM Output (COOP/COEP required for pthreads/SharedArrayBuffer)
```bash
python3 util/wasm/serve.py   # serves build-wasm/bin; --directory/--bind/--port
```
Open `http://127.0.0.1:8000/luanti.html`. Don't use a plain static server.

### Tests
```bash
# C++ unit tests (native only)
./bin/luanti --run-unittests

# Lua tests
busted builtin
luacheck builtin
luacheck --config=games/devtest/.luacheckrc games/devtest

# Integration tests
./util/test_multiplayer.sh
xvfb-run ./util/test_singleplayer.sh

# WASM browser tests (Playwright 1.59.0 + Chrome; run after a WASM build)
python3 util/wasm/test_client_browser.py --browser "$(command -v google-chrome)"
python3 util/wasm/test_first_playable.py --browser "$(command -v google-chrome)"
python3 util/wasm/test_demo_tools.py          # packaging/deploy tooling, size report, startup compare
node --test util/wasm/test_startup.mjs        # SIMD + memory-ceiling probe logic
python3 util/wasm/test_memory_probe_browser.py --browser "$(command -v google-chrome)"  # 2 GiB refused -> still starts
python3 util/wasm/test_packages_browser.py --browser "$(command -v google-chrome)"        # streamed packages + offline
python3 util/wasm/test_service_worker_browser.py --browser "$(command -v google-chrome)" # stale-binary regression
npm ci && npm run proxy:install               # once: lint/typecheck tooling + proxy deps
npm run lint && npm run typecheck             # ESLint + tsc (checkJs) over client/web and util/wasm
npm test                                      # WebSocket proxy (node --test)
```

### C++ Lint
```bash
# Configures build/, generates GenerateVersion + GenerateBuiltinFilesCpp, runs run-clang-tidy.py
CLANG_TIDY=clang-tidy-15 ./util/ci/clang-tidy.sh
```

## Architecture

### Engine Overview
Even in singleplayer, Luanti runs an internal server. The client and server communicate over a UDP-based protocol. Under Emscripten, `src/network/socket.cpp` implements `UDPSocket` in-process (loopback for the internal server) and tunnels remote traffic through a WebSocket proxy (`websocket_proxy.h` ↔ `client/web/network.js` ↔ `util/wasm/proxy/`).

```
src/
  server.cpp/h          – Server class (main server logic)
  client/               – Client-only (rendering, HUD, sound, input)
  server/               – Server-only (SAOs, bans, client interface, mod loading)
  network/              – Networking protocol; socket.cpp has the WASM WebSocket-proxy UDPSocket
  mapgen/               – Map generation algorithms
  script/cpp_api/       – C++ ↔ Lua bridge (s_*.cpp)
  script/lua_api/       – Lua-callable modules (l_*.cpp)
  database/             – SQLite3/PostgreSQL/LevelDB/Redis backends
  threading/            – pthreads wrappers; setName/bindToProcessor no-op'd for WASM
  util/                 – String, serialization, and other utilities
  porting.cpp/h         – Platform paths & abstractions; WASM returns virtual FS paths
  porting_emscripten.cpp/h  – IDBFS mount + syncfs (WASM-only)
  main.cpp              – Entry point; calls emscripten_init_filesystem() under WASM
  client/clientlauncher.cpp  – Blocking launcher loop (runs off the UI thread via PROXY_TO_PTHREAD)
  client/game.cpp       – Inner game loop
irr/                    – IrrlichtMt rendering engine (embedded CMake subproject)
builtin/                – Lua code loaded by the engine at runtime
lib/                    – Bundled third-party libraries (Lua 5.1, sha256, etc.)
games/luanti_web/       – Default game for the web demo (showcase pack + theater)
games/{flower_island,rollercoaster,log_ride,spire_tunnel}/ – Showcase games, each its own streamed WASM asset package
games/devtest/          – Development test game
client/web/             – Browser shell, launcher/PWA, network.js, persistence.js, theater/videotexture.js
util/wasm/              – serve.py, build_libzip.sh, package_demo.py, proxy/, Playwright tests
client/shaders/         – GLSL shaders (must be valid GLES2/WebGL)
```

### Key WASM Files
| File | Purpose |
|------|---------|
| `CMakePresets.json` | "Emscripten" preset (toolchain + flags) |
| `src/CMakeLists.txt` | `EMSCRIPTEN` branches (~lines 335, 548, 811); link flags and the `luanti_packages` target |
| `util/wasm/packages.json`, `build_packages.py` | Source of truth for the streamed asset packages (core, one per game, songs) → `assets/<name>.<hash>.data` + `asset-manifest.json` |
| `client/web/packages.js`, `service-worker.js` | Package loader (a `--pre-js`) and the versioned, narrow service worker |
| `src/porting_emscripten.cpp/h` | IDBFS mount, `emscripten_sync_filesystem()` |
| `src/porting.cpp` | Sets `path_share="/"`, `path_user="/home/web_user/.luanti"` under WASM |
| `src/network/socket.cpp` | Emscripten `UDPSocket` (loopback + WebSocket proxy) under `#ifdef __EMSCRIPTEN__` |
| `src/threading/thread.cpp` | No-op `setName()` / `bindToProcessor()` for Emscripten |
| `client/web/shell.html`, `launcher.js`, `network.js`, `persistence.js` | Page shell, launcher, proxy transport, IDBFS glue |
| `client/web/startup.js`, `build-info.js.in` | First `--pre-js`: SIMD check and shared-memory ceiling probe (hands the engine its `WebAssembly.Memory`); CMake generates the build-info half |
| `client/web/theater.js` / `videotexture.js` | Showcase theater: `<video>` overlay (Tier B) / video-to-texture spike (Tier C) |
| `.github/workflows/wasm.yml` | WASM CI: build, browser smoke, first-playable gate, Cloudflare Pages deploy |
| `util/wasm/package_demo.py` | Packages a versioned, cache-safe deploy directory |
| `util/content/generate_luanti_web_textures.py` | Regenerates every `games/luanti_web` texture |

### WASM Port Status (see `wasm_porting.md` — authoritative)
- Compile, FS init, IDBFS persistence, streamed asset packages (no monolithic `luanti.data`): done
- Main loop: `-sPROXY_TO_PTHREAD=1` + `-sOFFSCREEN_FRAMEBUFFER=1` (not ASYNCIFY / `emscripten_set_main_loop`)
- Networking: WebSocket proxy (`util/wasm/proxy/`, Node `ws`), not WebRTC
- Sound (Ogg/Vorbis ports, `ENABLE_SOUND=TRUE` in preset) and input/launcher: implemented
- Outstanding: deployed TLS (WSS) proxy + public-server multiplayer, cross-browser persistence acceptance

## Code Style

### C++ Conventions
- Every `.cpp`/`.h` starts with:
  ```cpp
  // Luanti
  // SPDX-License-Identifier: LGPL-2.1-or-later
  // Copyright (C) <year> <author>
  ```
- Indentation: **tabs** (width 4). No spaces in C++, CMake, Lua, or GLSL.
- Use `#pragma once` (never `#ifndef` guards).
- Use `DISABLE_CLASS_COPY(ClassName)` / `ALLOW_CLASS_MOVE(ClassName)` macros.
- Logging: `infostream`, `verbosestream`, `warningstream`, `errorstream` — never `std::cout`/`std::cerr`.
- Client-only headers: `#if !IS_CLIENT_BUILD` / `#error` guard. Server-only: `IS_SERVER_BUILD`.

### WASM Change Discipline
- Guard every WASM-specific change with `#ifdef __EMSCRIPTEN__` (C++) or `if(EMSCRIPTEN)` (CMake).
- Keep native builds working — changes to shared code must not break Linux/Windows/macOS.
- Call `emscripten_sync_filesystem()` after any write to `path_user` (world save, settings, player data).

### Commit Messages
- Present tense, capital first letter, ≤70 chars on first line, no trailing period.
- One logical change per branch (not `master`).

## Known Issues / Blockers

- Recent commit history is dominated by WebGL-specific bugfixes (worker context creation, black wedges when looking down, leftover-vertex-buffer triangle artifacts, a WebGL shutdown crash). The GLES2/WebGL path through IrrlichtMt's Emscripten support is still the most fragile part of the port.
- `src/threading/thread.cpp:123` — `FIXME: what if this fails, or if already locked by same thread?` in the lock-acquisition path.
- `util/wasm/deploy_1ink.py` uploads `build-wasm/bin` to test.1ink.us via storage.noahcohn.com (token via `DEPLOY_TOKEN`); CI deploys separately to Cloudflare Pages.
- Build output (`build-*/`) and `node_modules/` are git-ignored and must never be committed; the deployable client is produced by CI (`util/wasm/package_demo.py`).
- Upstream merges land on `master` periodically ("Merge upstream/master into the WASM fork"); keep WASM changes guarded so merges stay clean.

## Do Not Modify Manually
- `minetest.conf.example` and `settings_translation_file.cpp` — regenerated pre-release.
- `po/*.po` / `luanti.pot` — update with `util/updatepo.sh`.
- `VERSION_MAJOR/MINOR/PATCH` in `CMakeLists.txt` — use `util/bump_version.sh`.

## Reference Docs
- `wasm_porting.md` — authoritative WASM phase plan, blocker analysis, build flag reference
- `doc/compiling/wasm.md` / `doc/compiling/wasm-embedding.md` — WASM build, smoke tests, deploy, embedding
- `docs/wasm-issues/` — roadmap issues (persistence, proxy, main loop, launcher, CI, showcase, theater)
- `grok.md` — WASM context and current priorities
- `AGENTS.md` — full Luanti contributor rules, directory structure, test strategy
- `doc/lua_api.md` — server Lua API reference
- `doc/protocol.md` — network protocol specification
- `minetest.conf.example` — all configuration options
