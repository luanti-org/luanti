# Compiling Luanti for WebAssembly

The `Emscripten` preset builds the browser client in Release mode with
Emscripten 6.0.3. It emits a directly servable HTML shell and its JavaScript,
WebAssembly, and preloaded-data companions.

## Prerequisites

On a fresh Ubuntu system, install Git, CMake, Python, and normal build tools:

```bash
sudo apt update
sudo apt install build-essential cmake git python3 python3-pip
```

Install the same Emscripten SDK revision used by CI:

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
git checkout 1b8b2456bf3f54fd6e47d55a82dde7752978a40f
./emsdk install 6.0.3
./emsdk activate 6.0.3
source ./emsdk_env.sh
cd /path/to/luanti_wasm
```

The checkout pins both the SDK version and installer revision. The first
configure needs internet access: it populates Emscripten's SDL2, zlib, PNG,
JPEG, Freetype, SQLite, Ogg, and Vorbis ports. Luanti also builds the SHA-256-pinned zstd
1.5.7 source archive declared by the repository.

The client also needs libzip, which Emscripten has no port for. Build it once
into the SDK's sysroot (it lands in `EM_CACHE` if you set one, see below)
before the first configure:

```bash
util/wasm/build_libzip.sh
```

If the SDK is installed somewhere this user cannot modify, choose a writable
ports and system-library cache before configuring:

```bash
export EM_CACHE="$PWD/.cache/emscripten-6.0.3"
```

## Build and run

Configure and build with the checked-in preset:

```bash
cmake --preset Emscripten
cmake --build build-wasm --parallel 2
```

For diagnosing crashes or profiling use `cmake --preset EmscriptenDebug`
(`build-wasm-debug`, ASSERTIONS=2 + SAFE_HEAP) or `EmscriptenProfile`
(`build-wasm-profile`, optimised with source maps). The per-profile flag
matrix is in `wasm_porting.md` (Appendix: Flag Matrix).

`util/wasm/packages.json` is the source of truth for the read-only assets:
nothing is preloaded into the engine any more. The `luanti_packages` build
target (part of `all`; it only needs Python 3) turns each package into an
immutable `assets/<name>.<hash>.data` and writes `asset-manifest.json`, and
the page streams them in on demand (core first, a game when it is chosen).
Run it alone with `cmake --build build-wasm --target luanti_packages` or
`python3 util/wasm/build_packages.py --output build-wasm/bin`. A successful
build creates the engine bundle, the packages and launcher/PWA companions:

```text
build-wasm/bin/luanti.html
build-wasm/bin/luanti.js
build-wasm/bin/luanti.wasm
build-wasm/bin/asset-manifest.json
build-wasm/bin/assets/core.<hash>.data
build-wasm/bin/assets/<game>.<hash>.data   # one per game, plus the song packages
build-wasm/bin/launcher.js
build-wasm/bin/launcher.css
build-wasm/bin/launcher-config.js
build-wasm/bin/manifest.webmanifest
build-wasm/bin/service-worker.js   # stamped with the build id by build_packages.py
build-wasm/bin/luanti-web.svg
```

Serve the output with the repository helper:

```bash
python3 util/wasm/serve.py
```

Open <http://127.0.0.1:8000/luanti.html>. Do not open the HTML with a
`file://` URL or use a plain static server. Pthreads and shared WebAssembly
memory require a secure context (HTTPS, or localhost for development) and
these headers on every response:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

The helper also sends `Cross-Origin-Resource-Policy: cross-origin` so an
isolated parent page can embed the demo, disables local caching, and serves
`.wasm` as `application/wasm`.

The page first downloads `core` (with progress on the Start button) and opens
the persistent filesystem, then enables the launcher; the chosen game's
package is fetched when you press Start. Start a local DevTest world or choose Join server. A
remote server additionally requires a configured WebSocket/UDP proxy.

To serve another build directory or accept connections from another machine:

```bash
python3 util/wasm/serve.py --directory build-wasm/bin --bind 0.0.0.0 --port 8000
```

Binding to all interfaces does not provide HTTPS. Use a TLS reverse proxy for
non-localhost access.

## Smoke tests

The generated-client test needs Playwright's Python package and an installed
Google Chrome:

```bash
python3 -m pip install --user playwright==1.59.0
python3 util/wasm/test_client_browser.py
```

It verifies that the page is cross-origin isolated, IndexedDB hydration is
ready before the launch button is enabled, networking starts disabled, a local
world can be launched, C++ runs on an application worker, browser-main timers
remain responsive, and real engine loading status reaches the launcher. This
is the automated CI smoke; it is not a visual regression test.

Two more browser tests cover the streamed packages. Neither needs a game
world to be playable:

```bash
# Real engine: first visit fetches only core; a game fetches only its own
# package; with the network off a cached game still starts and an uncached one
# fails with a clear message; the showcase songs arrive after the world starts.
python3 util/wasm/test_packages_browser.py --browser "$(command -v google-chrome)"

# No engine needed: the service worker against a fake, versioned site. Includes
# the stale-luanti.wasm regression (a deploy, an HTTP error, a half-deployed or
# corrupt build must never be answered with an old binary or a mix of builds).
python3 util/wasm/test_service_worker_browser.py --browser "$(command -v google-chrome)"
python3 util/wasm/test_demo_tools.py    # packager, package format, first-visit budget
```

Before publishing a demo, the build must pass the **First Playable Smoke Test**,
the integration gate for the browser client. Every step has to pass on both
**Chrome** and **Firefox** (latest stable).

1. Serve with COOP/COEP headers (`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp`).
2. Page loads without JS/WASM errors.
3. IDBFS sync completes (the launcher reaches storage state `ready`, non-fatal).
4. Main menu renders and accepts input (no tab freeze).
5. Create a new singleplayer world in the default game (`luanti_web`, the
   showcase pack; `devtest` is still selectable for engine work).
6. Load world, move, place 10 blocks, break 5 blocks.
7. Exit to menu, hard-reload tab, load same world — blocks persist.
8. `minetest.conf` change survives reload.

Steps 2 to 7 are automated:

```bash
python3 util/wasm/test_first_playable.py --browser "$(command -v google-chrome)"
```

It seeds a `minetest.conf`, reloads, creates and enters a world, checks that
keyboard input round-trips through a chat command, digs and places nodes, then
hard-reloads the tab and requires `map.sqlite`, `players.sqlite` and
`minetest.conf` to come back byte-identical before re-entering the same world.
`--screenshot-dir DIR` saves the launcher, the world and the reloaded world;
`--engine firefox --browser PATH` drives a Playwright Firefox build instead of
Chrome. `--game devtest` runs the same gate against the bare engine sandbox
instead of the showcase, which is the useful comparison when a failure might be
content-specific.

What the automation cannot decide stays manual: how the game feels to walk
around in, mouse-look, sound, the pause menu, GPU rendering (the headless run
uses software GL), and step 8 through the engine's own settings UI. Neither
smoke replaces that pass.

The gate is not currently green. `docs/wasm-issues/06-first-playable-gate-results.md`
records the last run step by step, along with the known failures that block it
and the browsers still to be covered. Update that file whenever the gate is
re-run, and keep the demo gated until it passes.

## Frame-time benchmark

```bash
python3 util/wasm/bench_frame_time.py --headed --gpu --output bench.json
```

Creates a fresh `luanti_web` world at view distance 100, flies a fixed path
around the island for 30 s and writes a JSON report: frame time p50/p95/p99,
draw and present (`endScene`) time, draw calls, mesh queue length, a
per-second timeline, the WebGL context and the GPU the browser reports. The
path follows server wall-clock time, so slower builds cover the same ground.
`--setting KEY=VALUE` writes a `minetest.conf` line for the run (for example
`--setting wasm_mesh_threads=0`) and `--query` adds launcher parameters.
Without `--gpu` it runs on SwiftShader, which is what CI does; those numbers
are only comparable with other SwiftShader runs.

Compare render modes with `--query '&render=proxy'` and
`--query '&render=offscreen'`. The report's `server` entry gives the internal
server's globalsteps per second during the path. In proxy mode the server can
starve, because its SQLite I/O runs on the busy browser main thread; the
camera then stays near its start and the frame times describe a different
scene. `--no-map-io` keeps the world in the `dummy` backends to compare
rendering alone. Offscreen mode also reports `presented` (frames shown on the
page) and both report `rafHz`, the page's animation-frame rate after the path.

The numbers come from `Module.luantiPerf` (`client/web/perf.js`), which any
page or the browser console can query in game:

```js
Module.luantiPerf.summary({seconds: 10})   // last 10 s: fps, frameMs.p95, ...
const c = Module.luantiPerf.cursor(); /* ... */ Module.luantiPerf.summary({since: c})
```

Baselines are recorded in `wasm_porting.md` ("Frame-time baseline").

## CI and demo deployment

`.github/workflows/wasm.yml` runs one Release build on relevant pull requests,
default-branch pushes, and manual dispatches. It installs Emscripten 6.0.3,
runs the browser smoke, packages the result, and uploads a 14-day artifact.
Once stable, make the `wasm / Release build and browser smoke` check required
in branch protection.

Production deploys use a pre-created Cloudflare Pages Direct Upload project.
Set that project's production branch to this repository's default branch.
Configure these GitHub repository settings:

- Variable `WASM_DEMO_DEPLOY_ENABLED`: set to `true` only after the manual
  menu/world gate passes on the candidate hosting setup.
- Variable `CLOUDFLARE_PAGES_PROJECT`: the Pages project name.
- Secret `CLOUDFLARE_ACCOUNT_ID`: the owning Cloudflare account ID.
- Secret `CLOUDFLARE_API_TOKEN`: a token allowed to deploy that Pages project.

Unless the enable variable is exactly `true` and the project variable is set,
CI still builds, tests, and uploads the demo artifact but deliberately skips
external deployment. Cloudflare Pages reads the generated `_headers` file and
applies the required isolation headers to the static responses. Verify a
deployed build with:

```bash
curl -I https://PROJECT.pages.dev/
curl -I https://PROJECT.pages.dev/releases/BUILD_ID/luanti.wasm
```

Both responses must include COOP and COEP before the demo is announced.

## Versioning and caches

`util/wasm/package_demo.py` places every generated build under
`releases/<git-sha>/`. The engine bundle and launcher/PWA files therefore share
one immutable build namespace and cannot be mixed with companions from another
revision. Root `index.html` and `version.json` are served with `no-store`; the
build-scoped files are served with a one-year immutable cache policy. The
service-worker script is the exception and is revalidated so browsers can
discover an update; its cache is scoped to the immutable release URL.

Create the same layout locally with:

```bash
python3 util/wasm/package_demo.py \
  --version "$(git rev-parse HEAD)" --output wasm-demo
python3 util/wasm/serve.py --directory wasm-demo
```

The output directory must not already exist. A user who keeps an old tab open
continues to use that tab's coherent build. Opening the demo root again selects
the current build. If an older deployment was cached before this strategy was
introduced, close old tabs, clear site data, and hard-refresh once.

## Runtime constraints and common failures

- The current target uses `PROXY_TO_PTHREAD` and an offscreen framebuffer to
  keep the synchronous engine stack off the browser UI thread. It does not use
  Asyncify or `emscripten_set_main_loop()`.
- Which thread owns the WebGL context is chosen per page load with
  `?render=proxy|offscreen|auto`; the default comes from the CMake option
  `WASM_RENDER_THREAD` (default `proxy`). `proxy` keeps the context on the
  browser main thread and proxies every GL call to it. `offscreen` creates it
  on the engine worker and posts each finished frame to the page. `auto`
  tries `offscreen` and falls back to `proxy`. `Module.luantiWebGLInfo.render`
  reports what was created. Details and measurements: `wasm_porting.md`,
  "Render thread".
- Initial memory is 256 MiB and may grow to 1 GiB. Mobile browsers and
  memory-constrained devices can still terminate the tab.
- An SDL version probe that reports SDL is too old can mask a failed linker
  probe. Inspect `build-wasm/CMakeFiles/CMakeConfigureLog.yaml`; a read-only
  SDK cache is fixed by setting `EM_CACHE` to a writable directory, then
  reconfiguring from a clean build directory or clearing the failed probe.
- A missing `SharedArrayBuffer` normally means HTTPS/localhost or COOP/COEP is
  wrong. Check both the document and subresource response headers.
- A 404 for `.data`, `.wasm`, or a launcher/PWA companion means the generated
  output was separated or stale HTML referenced another deployment. Deploy the
  packaged directory as one unit.
- Startup storage errors are fail-closed by design. The shell reports the
  IndexedDB error instead of silently starting with an ephemeral filesystem.
- Remote multiplayer additionally needs a configured WSS proxy; it is not
  required for the singleplayer build or CI smoke.

See `wasm_porting.md` for architecture details and the remaining browser MVP
work.

See [wasm-embedding.md](wasm-embedding.md) for deep links, proxy-region
configuration, iframe permissions, PWA behavior, and embedding limitations.
