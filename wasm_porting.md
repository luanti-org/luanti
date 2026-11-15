# Luanti Emscripten / WebAssembly Porting Guide

This document describes the current state, blockers, and step-by-step plan for building Luanti (formerly Minetest) with Emscripten to run in web browsers via WebAssembly (WASM).

---

## Table of Contents

1. [Current State](#current-state)
2. [High-Level Architecture](#high-level-architecture)
3. [Blockers & Challenges](#blockers--challenges)
4. [Step-by-Step Implementation Plan](#step-by-step-implementation-plan)
5. [Build Instructions (Target)](#build-instructions-target)
6. [Files Requiring Changes](#files-requiring-changes)
7. [Dependency Notes](#dependency-notes)
8. [Networking Deep Dive](#networking-deep-dive)
9. [File System Strategy](#file-system-strategy)
10. [Rendering & Shaders](#rendering--shaders)
11. [Testing Strategy](#testing-strategy)
12. [Appendix: Useful Emscripten Flags](#appendix-useful-emscripten-flags)

---

## Current State

### What Already Works

- **IrrlichtMt** (the embedded graphics engine in `irr/`) already has **partial Emscripten support**:
  - `irr/src/CMakeLists.txt` detects `EMSCRIPTEN` and sets `_IRR_EMSCRIPTEN_PLATFORM_` and `_IRR_COMPILE_WITH_EGL_MANAGER_`
  - `irr/src/CIrrDeviceSDL.cpp/h` contain Emscripten-specific pointer-lock and event handling code
  - `irr/src/os.cpp` uses `emscripten_log()` and `emscripten_get_now()` for logging and timing
  - `irr/src/CEGLManager.cpp` has Emscripten EGL stubs
  - GLES2 is enabled by default for Emscripten
  - SDL2 device backend is used (Emscripten has excellent SDL2 port support)

### Remaining Work

- The proxy is hardened for public exposure (connection caps, shared rate limit, Origin and port/server-list allowlists, health and metrics, Docker + Caddy recipe). A deployed TLS proxy and public-server multiplayer acceptance are still outstanding.
- Full world persistence acceptance across Chrome/Edge/Firefox is outstanding.
- CI and public demo hosting are not yet configured.
- Sound and complete keyboard, pointer-lock, and touch acceptance remain.

---

## High-Level Architecture

Luanti is a C++17 client-server engine. Even singleplayer runs an internal server and communicates over a UDP-based network protocol. Key subsystems relevant to WASM:

```
┌─────────────────────────────────────────┐
│  Browser (WASM + WebGL)                 │
│  ┌─────────────────────────────────────┐│
│  │  Luanti Client (C++17 → WASM)       ││
│  │  ┌─────────┐ ┌─────────┐ ┌────────┐ ││
│  │  │Rendering│ │  Game   │ │  GUI   │ ││
│  │  │(Irrlicht│ │  Loop   │ │        │ ││
│  │  │  + GLES2│ │         │ │        │ ││
│  │  └────┬────┘ └────┬────┘ └───┬────┘ ││
│  │       │           │          │      ││
│  │  ┌────┴───────────┴──────────┴────┐ ││
│  │  │  Internal Server (same binary)  │ ││
│  │  │  - Lua scripting                │ ││
│  │  │  - Mapgen (SQLite3)             │ ││
│  │  └─────────────────────────────────┘ ││
│  └─────────────────────────────────────┘│
└─────────────────────────────────────────┘
```

For a first WASM port, the simplest approach is to keep the internal server model but replace or bridge the UDP socket layer.

---

## Blockers & Challenges

### 1. Networking — THE BIGGEST BLOCKER

Luanti uses **raw UDP sockets** for all client-server communication (including singleplayer). Browsers **cannot do UDP**.

**Used in:**
- `src/network/socket.cpp` — `socket()`, `bind()`, `sendto()`, `recvfrom()`, `poll()`
- `src/network/address.cpp` — `getaddrinfo()`, `inet_ntop()`, `inet_pton()`
- `src/network/connection.cpp` — Reliable-UDP implementation, packet fragmentation, congestion control

**Options:**

| Approach | Pros | Cons |
|----------|------|------|
| **WebRTC DataChannels** | UDP-like, low latency, supported in all modern browsers | Complex signaling required; Luanti protocol would need STUN/TURN or a signaling server |
| **WebSockets** | Simple, widely supported | TCP-only; changes game feel (head-of-line blocking); requires proxy server or protocol rewrite |
| **WebTransport** | Modern UDP-like API | Still emerging; not universally supported; requires HTTP/3 server |
| **In-process loopback** | No browser transport overhead for singleplayer | Does not reach remote servers |

**Implemented MVP:** Keep singleplayer UDP in an in-process queue and route only
remote IPv4 destinations through a versioned WebSocket-to-UDP proxy. This leaves
Luanti's reliable-UDP layer unchanged and works in current browsers. It adds one
TCP/WebSocket hop, so head-of-line blocking and proxy placement still affect
latency. See `docs/wasm-issues/02-websocket-proxy-networking.md`.

### 2. Main Game Loop

Luanti uses blocking `while` loops:

```cpp
// src/client/clientlauncher.cpp
while (m_rendering_engine->run() && !*kill) { ... }

// src/client/game.cpp
while (m_rendering_engine->run()) { ... }
```

Browsers require yielding control to the event loop. A blocking loop freezes the page.

**Options:**
- **`-sASYNCIFY`**: Emscripten can automatically transform the binary to yield at `emscripten_sleep()` calls. Easy but has significant runtime overhead and binary size cost.
- **`emscripten_set_main_loop()`**: Refactor the game loop into a callback function. This is the cleanest approach but requires restructuring `ClientLauncher::run()` and `Game::run()`.
- **`-sPROXY_TO_PTHREAD`**: Run the main application on a web worker thread. Allows blocking code but requires `SHARED_MEMORY` and has compatibility constraints.

**Implemented MVP:** Use `PROXY_TO_PTHREAD` with an offscreen framebuffer so the full
synchronous launcher/menu/game lifecycle runs on an application worker. Keep a
complete `emscripten_set_main_loop()` state-machine conversion as a measured
follow-up if worker-side canvas stalls remain.

### 3. Threading

Luanti uses `std::thread` via `src/threading/thread.cpp` and `std::mutex`, `std::condition_variable`, etc.

Emscripten supports pthreads, but:
- Requires `-pthread` at compile AND link time
- Requires `-sSHARED_MEMORY=1`
- Requires the hosting page to send `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp` headers
- Some pthread APIs are missing or limited (`pthread_setaffinity_np`, `pthread_setname_np` on some platforms)

**Files to modify:**
- `src/threading/thread.cpp` — `setName()`, `bindToProcessor()` need `__EMSCRIPTEN__` fallbacks

### 4. File System

Luanti expects a POSIX/Windows file system with directories like:
- `builtin/` (engine Lua)
- `games/` (games like devtest)
- `mods/`
- `textures/`
- `worlds/` (save data)
- `minetest.conf`
- `debug.txt`

Emscripten provides virtual file systems:
- **MEMFS**: In-memory, lost on refresh
- **IDBFS**: Persists to IndexedDB, but must be explicitly synced
- **WORKERFS**: Read-only access to files dropped by user
- **NODEFS**: Only for Node.js

**Strategy:**
1. At build time, use `--preload-file` or `--embed-file` to bundle read-only assets (`builtin/`, `games/devtest/`, `textures/`, `fonts/`)
2. At runtime, mount an `IDBFS` on `/home/web_user/.luanti/` for `worlds/`, `minetest.conf`, and `debug.txt`
3. Hydrate with `FS.syncfs(true)` from the `Module.preRun` bootstrap before `main()`
4. Submit committed saves to the serialized `FS.syncfs(false)` service

**Files to modify:**
- `src/filesys.cpp` — Path logic, directory listing
- `src/porting.cpp` — `path_share`, `path_user`, `path_cache` determination
- `src/main.cpp` — Initial FS setup

### 5. Build System

The CMake build assumes native targets. Key issues:

- `src/CMakeLists.txt` line 283: `find_package(Threads REQUIRED)` — works with Emscripten pthreads but needs flag propagation
- `src/CMakeLists.txt` lines 287-352: Platform library linking (`ws2_32`, `winmm`, `dl`, `rt`, `iconv`, `android`) — needs an `EMSCRIPTEN` branch
- `src/CMakeLists.txt` line 601: `CMAKE_CROSSCOMPILING` sets executable output to `build/bin` — okay, but install logic needs to output `.js`/`.wasm`/`.html`
- `src/CMakeLists.txt` lines 1003-1074: Windows DLL installation logic — irrelevant, needs skipping
- `irr/src/CMakeLists.txt` line 163: `find_library(EGL_LIBRARY NAMES EGL)` — Emscripten provides EGL via `-sEGL_LIBRARY` or system libs

---

## Step-by-Step Implementation Plan

### Phase 0: Preparation & Tooling

1. **Install Emscripten SDK** (emsdk) and verify with `emcc --version`
2. **Build or obtain Emscripten ports** for dependencies:
   - Lua 5.1 (or LuaJIT if WASM support works)
   - GMP
   - JsonCPP
   - SQLite3
   - ZLIB
   - Zstd
   - Freetype
   - SDL2 (use Emscripten's built-in: `-sUSE_SDL=2`)
   - Optionally: OpenAL-soft (`-sUSE_OPENAL=1`)
3. **Create a CMake toolchain file** or Emscripten preset in `CMakePresets.json`

### Phase 1: CMake & Compilation (Client Only, No Network)

Goal: Get `luanti` to compile and link as a `.js` + `.wasm` blob, even if it crashes at runtime.

1. **Create `cmake/Modules/EmscriptenOptions.cmake`** or modify `src/CMakeLists.txt`:
   - Add `elseif(EMSCRIPTEN)` branch alongside `WIN32`, `APPLE`, `UNIX`
   - Set `PLATFORM_LIBS` to Emscripten-specific libs
   - Disable `ENABLE_SOUND`, `ENABLE_CURL`, `ENABLE_GETTEXT`, `ENABLE_CURSES`
   - Disable PostgreSQL, LevelDB, Redis backends (keep SQLite3 only)
   - Disable `BUILD_SERVER`
   - Disable `BUILD_UNITTESTS` (re-enable later)
   - Set `ENABLE_GLES2=TRUE`, `ENABLE_OPENGL=FALSE`, `ENABLE_OPENGL3=FALSE`
   - Add Emscripten linker flags for SDL2, FS, memory, threading

2. **Patch `irr/src/CMakeLists.txt`**:
   - Ensure `ENABLE_WEBGL1` stays `FALSE` (broken upstream)
   - Ensure `ENABLE_GLES2` is `TRUE`
   - Verify SDL2 detection works with Emscripten's SDL2 port

3. **Patch `src/CMakeLists.txt`**:
   - Wrap Windows-specific resource compilation (`winresource.rc`) in `if(WIN32)` only
   - Wrap `sockets_init()` call or make it a no-op for Emscripten
   - Adjust `EXECUTABLE_OUTPUT_PATH` if needed
   - Remove or guard `CreateLegacyAlias` (symlinks don't exist on Emscripten FS)

4. **Use the browser `UDPSocket` transport in `src/network/socket.cpp`:**
   - Deliver loopback packets through a shared-memory queue for the internal
     singleplayer server.
   - Submit remote packets to `client/web/network.js`, which connects to the
     configured WebSocket-to-UDP proxy.
   - Register Emscripten's synthetic DNS addresses from `Address::Resolve()` so
     the proxy resolves the original hostname.

5. **Patch `src/porting.cpp` and `src/porting.h`**:
   - Add `__EMSCRIPTEN__` includes where needed
   - Implement `path_share` and `path_user` to point to virtual FS paths
   - Stub out `signal_handler_killstatus()` (SIGINT doesn't apply)
   - Stub out `secure_rand_fill_buf()` if needed, or use `getrandom` via Emscripten

6. **Patch `src/threading/thread.cpp`**:
   - Add `#ifdef __EMSCRIPTEN__` fallbacks for `setName()` (no-op) and `bindToProcessor()` (return false)

7. **Build and iterate** until linking succeeds.

### Phase 2: File System & Asset Bundling

Goal: The game can load its Lua builtins, textures, and fonts; save data goes to IDBFS.

1. **Bundle read-only assets** at link time:
   ```bash
   --preload-file ${CMAKE_SOURCE_DIR}/builtin@/builtin
   --preload-file ${CMAKE_SOURCE_DIR}/games/devtest@/games/devtest
   --preload-file ${CMAKE_SOURCE_DIR}/textures@/textures
   --preload-file ${CMAKE_SOURCE_DIR}/fonts@/fonts
   --preload-file ${CMAKE_SOURCE_DIR}/client/shaders@/client/shaders
   --preload-file ${CMAKE_SOURCE_DIR}/clientmods@/clientmods
   ```
   Or use `--embed-file` if you want them inside the `.wasm` binary.

2. **Mount and hydrate IDBFS before `main()`.**
   `client/web/persistence.js` is linked as `--pre-js`. Its `Module.preRun`
   callback holds a run dependency until `FS.syncfs(true)` succeeds, then creates
   the writable user directories. A startup storage failure intentionally keeps
   `main()` gated. See `docs/wasm-issues/01-persistence-mvp.md` for the runtime
   state contract and error behavior.

3. **Set default paths** in `src/porting.cpp`:
   - `path_share = "/"` (or wherever preloaded files are mounted)
   - `path_user = "/home/web_user/.luanti"`

4. **Service committed saves.** Engine save paths record atomic dirty
   generations. The application-worker frame pump submits them to the
   single-flight JavaScript `FS.syncfs(false)` service, with urgent disconnect,
   shutdown, and page lifecycle requests.

### Phase 3: Async Main Loop

Goal: The game renders without freezing the browser tab.

**Implemented MVP:** link with `-sPROXY_TO_PTHREAD=1` and
`-sOFFSCREEN_FRAMEBUFFER=1`. Luanti's synchronous launcher, menu, gameplay,
server-start, and shutdown stacks run away from the browser UI thread while the
SDL/EGL GL calls are proxied to the browser-owned canvas. Direct OffscreenCanvas
transfer is not enabled because it conflicts with that context-creation path in
Emscripten 6.0.3. No Asyncify or native behavior changes are required.

`porting::emscripten_validate_main_loop()` enforces this build contract at
startup. Every `RenderingEngine::run()` call services browser-platform work;
this currently submits persistence dirty generations. During final durable
shutdown, only the application worker waits, so the browser can keep showing
storage state and completing IndexedDB callbacks.

This architecture keeps the page and launcher overlays responsive during
engine work. A single long Lua callback can still stall the game canvas on the
application worker. If profiling proves that boundary unacceptable, the
follow-up is a complete `ClientLauncher` + `GUIEngine` + `Game` lifecycle state
machine for `emscripten_set_main_loop()`, not a gameplay-loop-only extraction.
See `docs/wasm-issues/03-async-main-loop.md` for the contract and focused browser
smoke.

### Phase 4: WebSocket Proxy Networking

Goal: Join native Luanti servers without changing those servers.

**Implemented transport:** The Emscripten `UDPSocket` branch presents the same
blocking interface used by `Connection`. Remote packets cross the browser main
thread through `client/web/network.js`, use a 12-byte destination/source header
over WebSocket, and are forwarded by the self-hostable Node server under
`util/wasm/proxy/`. The generated shell and `?proxy=` query parameter configure
the URL. IPv6 is intentionally rejected for this MVP.

The remaining phase gate is deployed acceptance: serve the built client with
COOP/COEP, deploy the proxy behind WSS near the test server, join by hostname,
and validate at least ten minutes of movement and chat. The proxy's shell status
and `Module.luantiNetwork.getState()` expose link RTT and packet counters.

### Phase 5: Sound

Goal: In-game audio works.

- **Implemented:** Enabled OpenAL and Vorbis via Emscripten ports (`-sUSE_OPENAL=1`, `-sUSE_OGG=1`, `-sUSE_VORBIS=1`).
- `EmscriptenDependencies.cmake` prepares `ogg` and `vorbis` ports and registers static library cache paths.
- Browser autoplay unlock handler added to `launcher.js` to resume the Web Audio context on user interactions.
- Threaded OpenAL audio pipeline runs seamlessly via Emscripten pthreads.

### Phase 6: Input & Browser Integration

Goal: Keyboard, mouse, touch, and gamepad work correctly in the browser.

1. **Mouse capture:**
   - SDL2 on Emscripten handles `SDL_SetRelativeMouseMode()` → calls `emscripten_request_pointerlock()`
   - Already partially handled in `irr/src/CIrrDeviceSDL.cpp`
   - Test and verify pointer-lock behavior when clicking into the canvas

2. **Touch controls:**
   - Luanti has touch controls for Android (`src/gui/touchcontrols.cpp`)
   - These may work out-of-the-box or need minor adjustments for browser touch events

3. **Keyboard:**
   - SDL2 maps browser key events to SDL keycodes
   - Verify that text input (`SDL_StartTextInput`) works for chat

4. **Fullscreen:**
   - Use `emscripten_request_fullscreen()` or SDL's fullscreen toggle

The MVP web experience now lives in `client/web/`. It holds `main()` with
`Module.noInitialRun` until preload-file and IDBFS startup complete, then calls
the exported Emscripten `callMain` once with validated launcher arguments. It
includes local/direct launch flows, an opt-in public server list, deployment-
configured WSS regions, real engine loading status, deep links, sharing,
fullscreen/pointer-lock affordances, a runtime console, and release-scoped PWA
caching. See `doc/compiling/wasm-embedding.md` for the supported iframe contract.

Touch-only gameplay and a stable pause/unpause API remain follow-up work. The
launcher intentionally does not expose provisional setters that would race the
synchronous application-worker lifecycle.

### Phase 7: Polish & Optimization

1. **Memory tuning**:
   - Start with `-sINITIAL_MEMORY=256MB -sALLOW_MEMORY_GROWTH=1`
   - Profile with browser DevTools to find a stable size
   - Large worlds may require 512MB+; consider chunk unloading strategies

2. **Binary size**:
   - Use `-O3` or `-Oz` for release builds
   - Use `-sMODULARIZE=1` to wrap the output in a factory function
   - Use `-sEXPORT_NAME="LuantiModule"`
   - Strip debug info: `-g0`
   - Consider `WASM=1` (default) and disable unnecessary Emscripten runtime features

3. **Asset loading**:
   - Instead of `--preload-file` (which base64-encodes data into JS), consider:
     - Fetching assets at runtime via `emscripten_async_wget()` or the Fetch API
     - Using a `.data` file with `--preload-file` (kept separate from `.wasm`)

4. **CI/CD**:
   - Add a GitHub Actions workflow that installs emsdk and builds the WASM target
   - Host the output on GitHub Pages for easy testing

---

## Build Instructions

The checked-in Emscripten preset is now the supported build path:

```bash
source /path/to/emsdk/emsdk_env.sh
cmake --preset Emscripten
cmake --build build-wasm --parallel 2
python3 util/wasm/serve.py
```

Then open `http://127.0.0.1:8000/luanti.html`. The server helper sends the
COOP/COEP headers required by pthreads and shared WebAssembly memory. See
`doc/compiling/wasm.md` for the pinned SDK revision, emitted files, browser
smoke, Cloudflare Pages deployment, cache strategy, and troubleshooting.

---

## Files Requiring Changes

### CMake / Build
- `CMakeLists.txt` — Packaging (`CPACK_GENERATOR`), executable suffixes
- `src/CMakeLists.txt` — Platform libs, install rules, `find_package` logic, Emscripten branch
- `irr/src/CMakeLists.txt` — SDL2 detection for Emscripten, EGL library finding
- `cmake/Modules/*.cmake` — Possibly new `FindEmscriptenLua.cmake`, etc.

### Platform Abstraction
- `src/porting.cpp` — Paths, signals, `secure_rand_fill_buf()`, `get_sysinfo()`, `getTimeNs()`
- `src/porting.h` — `sleep_ms()`, `sleep_us()` macros (already work, but verify)
- `src/porting_emscripten.cpp/h` — dirty generations and application-thread persistence service
- `client/web/persistence.js` — pre-main hydration, IDBFS sync serialization, retries, and public API
- `client/web/shell.html` — generated-shell persistence status and sticky errors

### File System
- `src/filesys.cpp` — `GetDirListing()`, `CreateDir()`, `RemoveDir()`, `CopyFile()`, `DeleteSingleFile()`

### Networking
- `src/network/socket.cpp` — Stub or replace UDP socket implementation
- `src/network/socket.h` — Interface is fine
- `src/network/address.cpp` — `Resolve()` may need localhost-only fallback
- `src/network/connection.cpp` — May need `#ifdef __EMSCRIPTEN__` guards around raw socket assumptions
- **NEW:** `src/network/websocket_socket.cpp/h` — Optional WebSocket transport
- **NEW:** `src/network/webrtc_transport.cpp/h` — Optional WebRTC DataChannel transport

### Threading
- `src/threading/thread.cpp` — `setName()`, `bindToProcessor()`

### Main Loop & Entry Point
- `src/main.cpp` — Submit the urgent final persistence generation on graceful shutdown
- `src/porting_emscripten.cpp/h` — Enforce and service the worker-main contract
- `src/client/renderingengine.h` — Service browser-platform work once per frame
- `src/client/clientlauncher.cpp`, `src/gui/guiEngine.cpp`, `src/client/game.cpp` — Preserve synchronous lifecycle under `PROXY_TO_PTHREAD`; a complete state-machine conversion remains optional follow-up work

### Rendering
- `src/client/renderingengine.cpp/h` — Verify GLES2 context creation
- `src/irrlicht_changes/` — Any patches may need Emscripten guards
- `client/shaders/` — Test shaders under WebGL/GLES2 constraints (no `GL_QUADS`, limited texture formats, etc.)

### Sound
- `src/sound/sound_openal.cpp` — Test with Emscripten's OpenAL port or stub out

### HTTP
- `src/httpfetch.cpp` — Replace cURL with `emscripten_fetch` or disable (`USE_CURL=FALSE`)

### Dependencies (in `lib/`)
- `lib/lua/` — Ensure Lua 5.1 builds with Emscripten
- `lib/sha256/` — Pure C, should work
- `lib/bitop/` — Pure C, should work
- `lib/lstrpack/` — Pure C, should work
- `lib/tiniergltf/` — Header-only C++, should work

---

## Dependency Notes

| Dependency | WASM Status | Action |
|------------|-------------|--------|
| **Lua 5.1** | Builds with Emscripten | Use bundled `lib/lua/` or compile upstream with `emcc` |
| **LuaJIT** | Experimental WASM support | Not recommended for first port; stick with PUC Lua 5.1 |
| **GMP** | Supported | Compile with `emconfigure ./configure && emmake make` |
| **JsonCPP** | Supported | Use bundled `lib/jsoncpp/` or compile upstream |
| **SQLite3** | Supported | Use bundled amalgamation or compile upstream |
| **ZLIB** | Emscripten port | `-sUSE_ZLIB=1` or compile upstream |
| **Zstd** | Supported | Compile upstream with `emcc` |
| **Freetype** | Emscripten port | `-sUSE_FREETYPE=1` or compile upstream |
| **SDL2** | Emscripten port | `-sUSE_SDL=2` (strongly recommended) |
| **OpenAL** | Emscripten port | `-sUSE_OPENAL=1` for OpenAL-soft wrapper |
| **cURL** | Complex | Disable initially; use `emscripten_fetch` or browser `fetch()` |
| **Gettext** | Not supported | Disable (`ENABLE_GETTEXT=FALSE`) |
| **Vorbis/Ogg** | Emscripten ports | `-sUSE_VORBIS=1 -sUSE_OGG=1` if using OpenAL |

---

## Networking Deep Dive

### Option A: In-Memory Loopback (Singleplayer Only)

For the fastest path to a playable WASM build, implement a fake `UDPSocket` that delivers packets directly to an in-memory queue:

```cpp
#ifdef __EMSCRIPTEN__
class LocalUDPSocket : public UDPSocket {
    std::queue<std::pair<Address, std::vector<u8>>> m_queue;
public:
    void Send(const Address &dest, const void *data, int size) override {
        // Push to a global queue that the "server" reads from
        g_loopback_queue.emplace(dest, std::vector<u8>((u8*)data, (u8*)data + size));
    }
    int Receive(Address &sender, void *data, int size) override {
        if (m_queue.empty()) return -1;
        // Pop from queue
    }
};
#endif
```

This lets you test the full client + internal server stack without any real network code.

### Option B: WebRTC DataChannels

WebRTC provides unreliable & ordered/unordered data channels (UDP-like) in browsers.

**Architecture:**
```
Browser (Luanti WASM)
  ↕ WebRTC DataChannel
Signaling Server (WebSocket, lightweight)
  ↕ WebRTC DataChannel
Native Luanti Server
```

**Implementation sketch:**
1. Write a small signaling server in Python/Node that exchanges SDP offers/answers over WebSockets
2. In `src/network/`, add `WebRTCDataChannel` class using `EM_ASM` / `emscripten::val` to call JS WebRTC APIs
3. Map `UDPSocket` interface to `RTCDataChannel.send()` / `onmessage`
4. Handle connection state changes and ICE candidates

### Option C: WebSocket Proxy

Write a proxy that sits between the browser and native Luanti servers:
```
Browser (WebSocket)  →  WS-to-UDP Proxy  →  Native Luanti Server
```

This is the implemented multiplayer path. `client/web/network.js` and
`util/wasm/proxy/` implement protocol version 1 while the C++ adapter keeps the
normal `UDPSocket` contract. TCP head-of-line blocking can still affect
real-time gameplay. The proxy can inspect unencrypted game traffic, so users
must trust its operator or self-host it.

---

## File System Strategy

### Read-Only Assets

Read-only data is **not** preloaded into the engine. `util/wasm/packages.json`
lists the packages, `util/wasm/build_packages.py` (the `luanti_packages`
target, run by every build) writes each one as a content-addressed
`assets/<name>.<sha256[:12]>.data` plus `asset-manifest.json`, and
`client/web/packages.js` downloads, hash-checks and mounts them into MEMFS on
demand. `core` is eager (the launcher waits for it); a game's package is
fetched when that game is chosen; a game's `background` packages (the showcase
songs) are fetched once its world is running.

| Package | Mount point(s) | When it is fetched |
|---------|----------------|--------------------|
| `core`: `builtin/`, `textures/`, `fonts/`, `client/shaders/`, `clientmods/` | `/builtin`, `/textures`, `/fonts`, `/client/shaders`, `/clientmods` | first visit, in parallel with compiling `luanti.wasm` |
| `luanti_web`: `games/luanti_web/` without the songs | `/games/luanti_web` | when "Luanti Web Showcase" is started |
| `luanti_web_songs_main`, `luanti_web_songs_takes`: `lw_amps/songs/*.ogg` | `/games/luanti_web/mods/lw_amps/songs` | in the background, after that world is running (the default song first) |
| `flower_island`, `rollercoaster`, `log_ride`, `spire_tunnel`, `devtest` | `/games/<id>` | when that game is started |
| `client/web/media/` | served at `media/` | copied next to `luanti.html`, fetched at runtime |

`devtest` is deliberately not in `core` (it is 1.4 MB and only used for engine
work). A remote server brings its own content, so joining one fetches no game
package.

Package format ("LWP1", described in `build_packages.py`): the file bodies,
then a JSON index, a little-endian u32 index length and the magic. Output is
deterministic, so an unchanged tree keeps its hash, its URL and its cache entry
from one release to the next. `package_demo.py` publishes the packages once in
`/assets/` (`immutable`) and points each release's `asset-manifest.json` at
them; only the manifest is revalidated.

The service worker (`client/web/service-worker.js`) precaches the shell and
`core` for offline use, serves packages cache-first and the shell network-first
(see the header comment there for the rules, and
`util/wasm/test_service_worker_browser.py` for the stale-`luanti.wasm`
regression test). A new build installs next to the old one and waits for the
launcher's "new version available" button.

### Content Budget

`luanti.data` no longer exists, so there are two budgets: what every visitor
pays before the first frame, and what one game costs when it is chosen.

| Budget | Limit | Measured (2026-11-06) |
|--------|-------|-----------------------|
| **First visit**: `core` (builtin, fonts, textures, shaders, clientmods) | **≤ 12 MB**, enforced by `test_demo_tools.py` | 9.33 MB (4.39 MB gzipped) |
| First frame of the default game: `core` + `luanti_web` | follows from the two rows | 10.1 MB, down from the 39.1 MB `luanti.data` it replaced (about 4.7 MB gzipped) |
| **Per game**: one game's package | small demo games under ~100 KB; `luanti_web` under ~1 MB; `devtest` is the exception at 1.5 MB | `luanti_web` 0.77 MB, `devtest` 1.50 MB, the four demos 14–38 KB each |
| **Streamed optional content**: the showcase songs | under 30 MiB in total (`test_luanti_web_amps.py`) | 27.5 MB, in two packages that never delay the first frame |
| Engine: `luanti.wasm` + `luanti.js` | not budgeted yet | 10.2 MB + 0.4 MB |
| Heap | 256 MB initial, 2 GiB maximum | `-sINITIAL_MEMORY` / `-sMAXIMUM_MEMORY` |
| Texture size | 16 or 32 px tiles; no single texture over 2048 px | GLES2 limits and atlas pressure |
| Runtime media | unbounded, but fetched on demand | `client/web/media/`, never packaged |

Growing `core` is a deliberate decision: every visitor pays for it. Content
for one game belongs in that game's package; anything big and optional belongs
in a `background` package (or runtime media).

Bundle:

* `games/luanti_web` (the showcase pack) and `games/devtest` (engine work).
* Small, purpose-built demo games under a few hundred KB each — currently
  `games/flower_island`, `games/rollercoaster`, `games/log_ride`, and
  `games/spire_tunnel` (84–184 KB apiece). Each new demo game added to the
  package in `util/wasm/packages.json` counts against the per-game budget
  above, and costs nothing for visitors who never pick it.
* Showcase art as small generated PNGs — the whole `luanti_web` palette,
  theater filmstrips, map-pack props and menu art is under 90 KB, regenerated
  by `util/content/generate_luanti_web_textures.py`.
* Authored themed maps as committed `.mts` schematics in
  `games/luanti_web/mods/lw_world/schems/map_*.mts` — a few KB each on disk.
  `lw_world` keeps each loaded schematic run-length encoded, a row at a time,
  in two Lua strings (three bytes per run, three per row), so a map costs its
  runs rather than its box. Measured with `lw_world`'s own loader under Lua 5.1,
  the eleven maps keep **110 KiB** between them (17 KiB for the busiest, the
  Halloween lane; 16 KiB for the 44,800-cell snow
  mountain), where the previous one-slot-per-cell arrays (node and `param2`,
  air included, 16 bytes a slot, rounded to a power of two) kept 10.0 MiB.
  Loading one still peaks at about 250 bytes a cell (10.9 MiB for the
  mountain) until the engine's `read_schematic` table is collected; that is
  now what limits a single map. `util/content/test_luanti_web_maps.py`
  repeats the measurement, round-trips every cell through the encoding, and
  fails past 512 KiB kept (64 KiB for one map), a 16 MiB load peak, 65,536
  cells in one map, or 512,000 cells in the pack (a creep guard; the eleven
  maps are 271,268). The history of the old caps is in
  `games/luanti_web/docs/maps.md` ("Budget").
* Short sound effects as mono Vorbis under a few KiB each: the lighthouse horn
  and the thunder chapel's three thunder rolls
  (`games/luanti_web/mods/lw_maps/sounds/lw_thunder.{1,2,3}.ogg`, under
  24 KiB together, each under 3 s, so decoded whole rather than streamed).
  The chapel also has an empty slot for an optional pipe-organ loop in
  `games/luanti_web/mods/lw_maps/organ/`. That folder is outside the engine's
  join-time media: the track is pushed with `core.dynamic_add_media` to
  visitors of the chapel only. It is still inside the `luanti_web` package,
  though, so a committed track grows that package (every showcase start) by its size. Keep
  it a short mono loop, or leave it out of the repository (see that folder's
  README).
* The cabaret club's songs: an empty slot in
  `games/luanti_web/mods/lw_amps/cabaret/`, for mono Ogg pairs registered in
  `songs.lua` there with `media_dir = "cabaret"`. Like the organ, they are
  outside the join-time media: `lw_amps` pushes each pair with
  `core.dynamic_add_media` (`client_cache = false`) to listeners of the
  cabaret's cabs only. Also like the organ, anything committed there is
  inside the `luanti_web` package, so it grows that package by its size. Add
  a few small pairs at most, or give a large set its own `background`
  package in `util/wasm/packages.json` the way the Around songs have (see
  that folder's README).
* Short, same-origin theater clips dropped into `client/web/media/` and fetched
  at runtime.
* The Lace Waterfall songs for the stage amps
  (`games/luanti_web/mods/lw_amps/songs/`). That is seven mixes of "Around",
  each a pair of mono Vorbis files (27.5 MB in all) — the one deliberate
  exception to "small". They are no longer part of anything a visitor must
  download before playing: `util/wasm/packages.json` puts them in two
  `background` packages (the default mix first, then the six alternate takes)
  that the launcher fetches one at a time once the showcase world is running.
  The amps can only place the song on the stage if it is game media, so it is
  written into the game's `songs/` folder rather than fetched by the engine;
  `lw_amps` registers these songs with `lazy_media = true`, checks for the
  files when a listener needs them, stays quiet until they exist and then
  starts the pair in step with the clock. The files sit in `songs/`, outside
  every media folder, so the server never hashes them at start-up or announces
  them on join, and `lw_amps` pushes a pair with `core.dynamic_add_media` only
  when it plays for a listener (only on the concert arena, in the pack).
  Long sounds are streamed, not decoded whole (`SOUND_DURATION_MAX_SINGLE` is
  3 s in `src/client/sound/sound_constants.h`). The songs are sent with
  `client_cache = false`, so the client does not keep a second copy under
  `path_cache` (which on the web is inside the IDBFS-backed `path_user`).
  `util/content/test_luanti_web_amps.py` fails past 30 MiB of songs.
* An optional tiny authoring mod (e.g. a clone-stick tool) — not a full
  modpack zoo.

Do not bundle yet:

* VoxeLibre, Mineclonia, or other large mob-pack/gameplay mods.
* Full `minetest_game` as the *default* web game (it may ship later as an
  opt-in, non-default download, in its own package and never in `core`).
* A runtime ContentDB client as a demo dependency: the showcase must work with
  networking switched off.
* 128 px / HD texture packs.
* Infinite mapgen as the landing world. The showcase world is authored and
  bounded (`games/luanti_web/mods/lw_world/init.lua`).
* Content that assumes OpenAL is production-ready. Phase 5 shipped OpenAL as
  a playable MVP (see below and #8), but it hasn't been hardened for heavy or
  long-running audio. Don't bundle content that leans on that. The stage amps
  are the one exception, and they keep it light: one song at a time, two
  sources per listener, and a listener's sources are released once they walk
  out of earshot. A missing or failing `sound_play` leaves the band dancing in
  silence instead of erroring.
* Large MP4s inside an asset package (above all `core`). Theater clips are
  runtime fetches precisely so the download cost falls only on visitors who
  walk into the theater.

When a change grows the web pack, say by how much in the pull request — see
the PR template checklist item.

Theater has three tiers: Tier A (engine-native animated tiles) and Tier B (an
HTML `<video>` overlay) are shipped and playable. Tier C (decoding frames onto
a node's texture instead of an overlay) is a feasibility spike, not demo
content — see `docs/wasm-issues/08-theater-tier-c-video-texture-spike.md`.

### Read-Write User Data
Use `IDBFS` mounted at runtime.

The implementation is `client/web/persistence.js`, linked through `--pre-js` and
`-lidbfs.js`. It hydrates in `Module.preRun`, exposes
`Module.luantiPersistence`, and serializes every `FS.syncfs(false)` call. Engine
save paths record committed work; they do not call asynchronous JavaScript from
worker threads. See `docs/wasm-issues/01-persistence-mvp.md`.

---

## Rendering & Shaders

### WebGL / GLES2 Constraints

Luanti's shaders in `client/shaders/` use GLSL. Under WebGL/GLES2:
- No `GL_QUADS` — must use triangles or triangle strips (IrrlichtMt handles this internally)
- No `gl_FragColor` in WebGL2 without `out` declaration; WebGL1 uses it
- Precision qualifiers required in fragment shaders: `precision mediump float;`
- Texture format limitations — no `GL_RGB10_A2` in WebGL1
- Max texture size may be 4096 or 8192 depending on GPU

### What to Test
- Menu rendering (GUI)
- Block/node rendering
- Entity rendering
- Sky, clouds, shadows
- Post-processing (FXAA)

---

## Testing Strategy

1. **Compile-time:** Get a clean build with zero errors
2. **Link-time:** Resolve all missing symbols
3. **Startup test:** Does `main()` run? Does the virtual FS mount?

### First Playable Smoke Test (Integration Gate)
Before a public demo can be approved, the build must pass this end-to-end checklist on Chrome and Firefox:
1. Serve with COOP/COEP headers (`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp`)
2. Page loads without JS/WASM errors
3. IDBFS sync completes
4. Main menu renders and accepts input (no tab freeze)
5. Create a new singleplayer devtest world
6. Load world, move, place 10 blocks, break 5 blocks
7. Exit to menu, hard-reload tab, load same world — blocks persist
8. `minetest.conf` change survives reload

`util/wasm/test_first_playable.py` drives steps 2-7 in a real browser; see
`doc/compiling/wasm.md` for how to run it and what stays manual.
`docs/wasm-issues/06-first-playable-gate-results.md` records the result of the
last run, the failures blocking the gate, and the browsers still to cover.

*Note: Multiplayer testing (WebSocket proxy) is tracked as a separate milestone.*

---

## Appendix: Useful Emscripten Flags

| Flag | Purpose |
|------|---------|
| `-sUSE_SDL=2` | Use Emscripten's SDL2 port |
| `-sUSE_ZLIB=1` | Use Emscripten's ZLIB port |
| `-sUSE_FREETYPE=1` | Use Emscripten's Freetype port |
| `-sUSE_OPENAL=1` | Use Emscripten's OpenAL-soft port |
| `-sFULL_ES2=1` | Full OpenGL ES 2.0 emulation |
| `-sALLOW_MEMORY_GROWTH=1` | Grow heap dynamically |
| `-sINITIAL_MEMORY=256MB` | Starting heap size |
| `-sMAXIMUM_MEMORY=1GB` | Max heap size |
| `-pthread` | Enable pthread support |
| `-sSHARED_MEMORY=1` | Required for pthreads |
| `-sPROXY_TO_PTHREAD=1` | Run the synchronous application main on a worker |
| `-sOFFSCREEN_FRAMEBUFFER=1` | Proxy worker-side SDL/EGL rendering to the browser canvas |
| `-sFETCH=1` | Enable `emscripten_fetch` API |
| `-sFORCE_FILESYSTEM=1` | Include FS support even if not auto-detected |
| `-sASYNCIFY` | Optional blocking-yield transform; not used by the current target |
| `-sEXIT_RUNTIME=0` | Keep browser callbacks and final persistence retries alive after `main()` returns |
| `-lidbfs.js` | Explicitly retain the legacy JavaScript IDBFS backend |
| `-sMODULARIZE=1` | Wrap output in a factory function |
| `-sEXPORT_NAME="LuantiModule"` | Name of the factory function |
| `--preload-file src@dst` | Bundle files into a `.data` payload |
| `--embed-file src@dst` | Embed files into the wasm binary |
| `--shell-file template.html` | Custom HTML shell |
| `--pre-js file.js` | Install the browser networking and persistence contracts before engine startup |
| `-sASSERTIONS=1` | Enable runtime assertions (debug builds) |
| `-sSAFE_HEAP=1` | Check for memory errors (debug builds) |
| `-g` / `-gsource-map` | Debug info / source maps |

## Appendix: Flag Matrix

Link flags live in one place (`src/CMakeLists.txt`, `if(EMSCRIPTEN)`): a shared
base plus a per-`CMAKE_BUILD_TYPE` profile. Presets: `Emscripten` (Release,
`build-wasm`), `EmscriptenDebug` (`build-wasm-debug`) and `EmscriptenProfile`
(RelWithDebInfo, `build-wasm-profile`).

| Flag | Release / MinSizeRel | RelWithDebInfo (`EmscriptenProfile`) | Debug (`EmscriptenDebug`) |
|------|----------------------|--------------------------------------|---------------------------|
| `-sASSERTIONS` | `0` | `1` | `2` |
| `-sSAFE_HEAP=1`, `-sSTACK_OVERFLOW_CHECK=2` | off | off | on |
| `--profiling-funcs`, `-gsource-map` | off | on | on |
| `-O3` at link (runs `wasm-opt`) | on | via CMake flags | n/a (`-O0`/`-g`) |
| `-flto` | opt-in: `-DWASM_ENABLE_LTO=ON` (experimental) | off | off |
| `-msimd128` | opt-in: `-DWASM_ENABLE_SIMD=ON` | same | same |
| `-sMALLOC` | opt-in: `-DWASM_MALLOC=mimalloc` | same | same |
| `-sPTHREAD_POOL_SIZE` | `-DWASM_PTHREAD_POOL_SIZE=10` (integer or JS expression) | same | same |

`-sDISABLE_EXCEPTION_CATCHING=0` was dropped: `-fexceptions` already implies it.

### Thread workarounds

Three `__EMSCRIPTEN__` workarounds predate the pthread pool and are now
settings (defaults keep the old behaviour until measured):

| Setting | Was | Default |
|---------|-----|---------|
| `wasm_mesh_app_thread_items` | `processOnThisThread(4)` every client step (`src/client/client.cpp`) | `4` (`0` = off) |
| `wasm_mesh_threads` | forced one mesh thread (`mesh_generator_thread.cpp`) | `1` (`0` = normal auto) |
| `wasm_server_min_receive_ms` | 5 ms `Receive()` floor (`server.cpp`) | `5` (`0` = off) |

Removing one: set it to `0` in `minetest.conf`, run
`test_async_main_loop.py` and `test_first_playable.py`, and compare frame
times and mesh latency before flipping the default.

### WebGL context

`CIrrDeviceSDL.cpp` tries WebGL2+explicit swap, WebGL2, WebGL1+explicit swap,
WebGL1 (`?webgl=1|2` forces one version). Attributes: `powerPreference:
high-performance`, `preserveDrawingBuffer: false`, `antialias: false` (the
offscreen back buffer is what is rendered to). The context actually created is
logged and exposed as `Module.luantiWebGLInfo`. `desynchronized` is not set
(not available in the pinned Emscripten 6.0.3 attribute struct).

On `webglcontextlost` the engine pauses presenting and the launcher shows an
overlay, flushes IndexedDB, and offers a reload. The engine has no GL resource
recreation path, so `webglcontextrestored` does not resume rendering in place.

### Render thread

`WASM_RENDER_THREAD` (CMake, default `proxy`) picks which thread owns the
WebGL context; `?render=proxy|offscreen|auto` overrides it per page load, and
`Module.luantiWebGLInfo.render` reports what was actually created.

| Mode | WebGL owner | GL calls | Presenting a frame |
|------|-------------|----------|--------------------|
| `proxy` | browser main thread | proxied from the engine worker (draw calls synchronously under `-sFULL_ES2`) | `emscripten_webgl_commit_frame()` blits the `OFFSCREEN_FRAMEBUFFER` FBO |
| `offscreen` | engine worker, on an `OffscreenCanvas` it creates | direct | `transferToImageBitmap()` posted to `client/web/present.js`, shown through a `bitmaprenderer` context on `requestAnimationFrame` |
| `auto` | `offscreen` if the browser can create the worker context, otherwise `proxy` | | |

The context ladder in `CIrrDeviceSDL::createBrowserWebGLContext()` tries
offscreen WebGL2 and WebGL1 first (not in `proxy` mode), then the four proxied
attempts. A forced `offscreen` never falls back, so tests can tell the modes
apart.

Why not `-sOFFSCREENCANVAS_SUPPORT` and `transferControlToOffscreen()`, as
first planned:

- A canvas transferred to a worker presents only when that worker returns to
  its event loop (`commit()` is gone from browsers). The engine loop never
  returns, which is why the earlier attempt left the game blank.
- With `OFFSCREENCANVAS_SUPPORT`, Emscripten transfers `#canvas` on the main
  thread as soon as a context with `explicitSwapControl` is created, which is
  the first attempt of the proxied ladder; the fallback path would no longer
  be the path that ships today.
- A transferred canvas can no longer be resized from the main thread, and the
  DOM event targets SDL registers on would need rerouting.

The worker-created `OffscreenCanvas` avoids all three: `#canvas` stays an
ordinary DOM canvas (input, pointer lock, fullscreen and layout are
unchanged), the fallback is still possible after a failed worker context
because `#canvas` is untouched until the first frame, and no link flags
change. The context is registered with `GL.registerContext()`, after which
Emscripten's GL trampolines call WebGL directly because the calling thread
owns the context.

Transport: the worker posts `{luantiFrame: ImageBitmap, luantiAck: ptr}` with
the bitmap transferred, not through Emscripten's worker command channel,
whose message format changed between releases (string commands in 3.1.x,
numeric ones in 6.x). `present.js` wraps `window.Worker` before the pthread
pool exists and listens for that field on every worker; Emscripten's own
handler ignores messages without a `cmd`. The context is torn down on the
worker too (`luanti_offscreen_destroy_context()`): without
`OFFSCREENCANVAS_SUPPORT`, `emscripten_webgl_destroy_context()` always runs
on the main thread, which has no such context.

Post-processing stays off by default in both modes. Forced on, it crashes
the proxied mode (`RuntimeError: null function` on the engine worker); in
offscreen mode the first frames look right, but within seconds of flying the
`bench_frame_time.py` path the terrain fades into sky colour until only the
sky, a stray sky-coloured triangle and the HUD remain (SwiftShader,
2026-11-11). `test_webgl_context.py`'s colour check does not catch that, as
the sky gradient and HUD alone pass it; any change that turns post-processing
on needs a screenshot along the benchmark path.

Pacing: one frame waits for the display while the worker renders the next.
`present.js` acknowledges frames on `requestAnimationFrame` through a counter
in shared memory, so the worker runs no faster than the display. Letting it
run ahead (two in flight and a 100 ms cap on the wait) rendered frames the
page then dropped, and on SwiftShader the extra GPU work starved the
compositor: `requestAnimationFrame` fell from 13 Hz to 2 Hz. A hidden page
acknowledges immediately; the 1 s cap on the wait is only a safety net.

Context loss in offscreen mode: the worker never runs the `webglcontextlost`
event, so `SwapWindow()` polls `isContextLost()` and calls
`Module.luantiLauncher.notifyContextLost()`, which shows the same overlay as
the DOM event. The context is never restored in this mode (nothing calls
`preventDefault()` on the event); reloading is the recovery either way.
`Module.luantiLauncher.debugLoseContext()` triggers a loss in either mode.

Per-frame main-thread queries are gone in both modes: canvas layout,
visibility and pointer-lock state are published into shared memory by
main-thread listeners (`installBrowserPageState()`), instead of synchronous
`MAIN_THREAD_EM_ASM` / `emscripten_get_*_status` calls from the frame loop.
OpenAL calls from the sound thread are still proxied synchronously.

### Frame-time baseline

Measured with `util/wasm/bench_frame_time.py` (fresh `luanti_web` world, view
distance 100, 1280x720, 30 s path). `present` is `endScene()`, which under the
proxied design is where the browser main thread drains the queued GL calls
and runs `emscripten_webgl_commit_frame()`.

| Date | Revision | Render path | GPU | Map I/O | fps | frame p50 / p95 / p99 ms | present p95 ms | draw calls p50 | server steps/s |
|------|----------|-------------|-----|---------|-----|--------------------------|----------------|----------------|----------------|
| 2026-11-11 | `perf/wasm-frame-stats` | proxy | SwiftShader, headless, 8 cores | SQLite | 9.2 | 108 / 231 / 308 | 217 | 133 | not measured |
| 2026-11-11 | render-thread work | proxy | same | SQLite | 8.3-8.7 | 111-119 / 234-267 / 327-383 | 228-254 | 132-185 | 0.13 |
| 2026-11-11 | render-thread work | offscreen | same | SQLite | 4.0-4.2 | 216-237 / 440-527 / 491-663 | 138-187 | 327-330 | 41-42 |
| 2026-11-11 | render-thread work | proxy | same | dummy | 2.9-3.2 | 289-345 / 430-501 / 520-605 | 151-200 | 329-331 | 46-49 |
| 2026-11-11 | render-thread work | offscreen | same | dummy | 4.3-4.4 | 207-213 / 439-452 / 537-574 | 129-178 | 330-334 | 48 |

Ranges are over two to four runs, all with post-processing off.

The first two proxy rows did not measure the scene the offscreen rows did.
The server was starved: 0.13 globalsteps per second, with gaps of up to 19 s.
The rig that carries the camera barely moved, so the camera stayed at its
start with only nearby blocks loaded (a third of the primitives and half the
draw calls). The cause is file I/O. This build uses Emscripten's JS
filesystem, not WASMFS, so every SQLite read and write from the server and
emerge threads is a synchronous call run on the browser main thread, which in
proxy mode is also draining GL. Evidence:

- With map, player and mod storage in the `dummy` backends (`--no-map-io`),
  the proxied server runs at 46-49 steps/s.
- Sleeping 20 ms after each commit (`?swapsleep=20`), which gives the main
  thread idle time, only raises it to 0.97 steps/s.
- The offscreen mode, whose main thread is idle apart from showing frames,
  keeps the server at 41-42 steps/s with SQLite.

Like for like (`dummy` rows), the worker-owned context is about 40% faster
at the median frame and no worse at p95 on SwiftShader. SwiftShader
rasterises on the CPU and contends with the page for it, so neither number
predicts a real GPU. The rows show that the proxy costs main-thread time
that other threads need, not only frame time.

No real-GPU numbers have been recorded yet. Record them before changing the
default render mode, for example:

	python3 util/wasm/bench_frame_time.py --headed --gpu --query '&render=proxy'
	python3 util/wasm/bench_frame_time.py --headed --gpu --query '&render=offscreen'

Compare the `server` entries as well as the frame times.

Changes measured in these rows besides the render mode:

- The engine uses its real frame time; the fixed 16 ms `dtime` is removed,
  so `fps_max` limiting works.
- `sleep_us` no longer rounds up to whole milliseconds.
- The per-frame canvas size, visibility and pointer-lock queries are gone.
- The `webgl_compat.js` copies reuse scratch buffers. Chrome hands WebGL a
  non-growable heap here, so the shim copied nothing in any run.

`test_first_playable.py` passes with `--render offscreen` (2 of 2 runs). With
`--render proxy` it fails with "dug 0 of 5", as it did before this work: the
dig waits for the stalled server (see
`docs/wasm-issues/06-first-playable-gate-results.md`).

The `wasm_mesh_*` knobs have not been re-benchmarked. On SwiftShader, mesh
generation is not what limits a frame; measure them on a real GPU.

### Not yet done

Tracked in the originating issue: SIMD second flavour + launcher feature
detection, `mimalloc` evaluation, Brotli/`wasm-opt` in `package_demo.py`, a
smaller-heap fallback build with `WebAssembly.Memory` probing, re-enabling
`enable_post_processing` (see "Render thread"), and dynamic resolution
scaling / perf overlay.

---

## Summary Checklist

- [x] Phase 0: Emscripten SDK installed; required dependencies prepared for WASM
- [x] Phase 1: CMake configures and links a client-only build
- [x] Phase 1: Browser UDP interface has in-process loopback and proxy routing
- [x] Phase 1: `porting.cpp` returns correct virtual paths
- [x] Phase 2: Asset preload bundle is emitted and available at startup
- [x] Phase 2: IDBFS hydration and sentinel persistence pass browser automation
- [x] Phase 3: Synchronous engine loop is isolated from the browser main thread
- [x] Phase 3: Rendering pipeline runs (menu visible)
- [x] Phase 4: WebSocket/UDP transport, launcher configuration, and self-hostable proxy implemented
- [ ] Phase 4: Public-server browser play acceptance completed
- [x] Phase 5: (Optional) Sound enabled via OpenAL + Vorbis
- [x] Phase 6: Input functional (keyboard, mouse look & pointer-lock, text input, touch/gamepad)
- [ ] Phase 7: Optimized release build; CI workflow added
- [x] **Shaders**: Added integer precision qualifiers (`int`, `uint`, `sampler2DArray`) in `src/client/shader.cpp` to ensure WebGL 2 / GLES 3 compilation passes for advanced features like FXAA and 3D UI nodes.

---

*This document is a living plan. Update it as the port progresses.*
