# [WASM] First Playable Smoke Test — gate status

**Tracks:** #14 · **Gates:** the public demo (#6)

The checklist itself lives in `doc/compiling/wasm.md` ("Smoke tests"). This file
records what has actually been run against it, so the demo gate is decided from
observations rather than from memory. Update it whenever the gate is re-run.

## Latest run

| | |
|---|---|
| Build | `c81c721`, `build-wasm/bin` Release (Emscripten 6.0.3) |
| Date | 2026-09-15 |
| Automation | `util/wasm/test_first_playable.py` |
| Served by | `util/wasm/serve.py` (COOP `same-origin`, COEP `require-corp`) |

| # | Checklist step | Chromium 141 (headless) | Chrome stable | Firefox stable |
|---|---|---|---|---|
| 1 | COOP/COEP headers | pass (`crossOriginIsolated`) | not run | not run |
| 2 | Page loads with no JS/WASM error | pass | not run | not run |
| 3 | IDBFS hydration completes | pass (`persistence.state === "ready"`, not fatal) | not run | not run |
| 4 | Menu renders, accepts input, no tab freeze | pass (launch button enabled; 329 browser-main timer ticks during engine load) | not run | not run |
| 5 | Create a new singleplayer devtest world | pass (world created under `worlds/devtest`) | not run | not run |
| 6 | Load world, move, place 10, break 5 | **partial** — world entered, keyboard input round-trips through chat, 5+ nodes dug; **0 nodes placed (#32)** | not run | not run |
| 7 | Hard reload, load same world, blocks persist | pass (`map.sqlite`, `players.sqlite` byte-identical after reload; world re-enters play) | not run | not run |
| 8 | `minetest.conf` change survives reload | partial — a config file written before launch survives the reload and is read back by the engine; a setting changed through the engine's own settings UI has not been checked | not run | not run |

Headless Chromium runs with `--use-gl=angle --use-angle=swiftshader`, so it is a
software GL path: it proves the GLES2/WebGL code path builds scenes and renders,
but it is not evidence about GPU drivers or frame rate.

## Known failures

| Step | Failure | Issue |
|---|---|---|
| 6 | Right mouse button never places a node; digging with the left button works in the same session | #32 |
| 6 | The "Click to return to the game" overlay covers the canvas whenever the engine releases pointer lock, so the inventory and every formspec are unusable with the mouse | #33 |

Until #32 and #33 are fixed, the gate does **not** pass and the public demo
stays gated. `util/wasm/test_first_playable.py` measures placement but does not
assert on it; run it with `--require-place` to check a candidate fix for #32.

## Re-run notes, 2026-11-05 (local, loaded 8-core host)

Not a gate pass; observations only. Headless Chromium, SwiftShader, host load
average ~9 from unrelated jobs, server "max lag" 7-20 s.

- `test_first_playable.py` already failed on a clean `HEAD` build here, so the
  failures are not caused by the link-profile / WebGL changes in #83.
- With the default game (`luanti_web`) the chat step used to fail because it
  waited for devtest's `Caught command` log line, which `luanti_web` does not
  print. It now waits for the engine's own `invoked /<command>` line.
- Right-click **does** reach the server: the log shows `singleplayer activates
  <item>` (the engine's right-click-on-air action) for every attempt. In the
  place step the view was pointing at air, so that is not by itself evidence
  for or against #32.
- Server-side input is processed many seconds late under load (a `digs` line
  appeared ~7 s after the click). The dig step now polls up to 30 s instead of
  waiting a fixed 2 s.
- The dig step presses hotbar slot 1, which is the mese pick in devtest but
  `lw_tools:param2` in `luanti_web`; with `luanti_web` it "uses" a tool on the
  node instead of digging. The place/dig steps need per-game slots before the
  default game can pass.
- Re-run on an idle host before drawing conclusions about #32 / #33.

## Re-run notes, 2026-11-11 (local, 8-core host, SwiftShader)

Observations only, from the render-thread work (`wasm_porting.md`, "Render
thread" and "Frame-time baseline").

| Render mode | Result |
|---|---|
| `?render=offscreen` | **pass**, 2 of 2 runs: 8 placed, 5 dug, world and config byte-identical after the reload |
| `?render=proxy` (the build default) | fail, `dug 0 of 5` in every run |

The proxied failure, like the "server-side input is processed many seconds
late" note above, matches a measured server stall. In proxy mode the server
managed 0.13 globalsteps per second while the client rendered. Its SQLite I/O
goes through Emscripten's JS filesystem, which runs every file operation
synchronously on the browser main thread, and that thread is busy draining
GL. With the context on the engine worker, the main thread is free and the
server keeps 41-42 steps/s.

These runs do not change the status of #32 and #33, which need the manual
browser pass. They do suggest the automated gate depends on the render mode
on slow GPUs.

## Not yet run

- **Chrome stable and Firefox stable, driven by a human.** Everything above
  comes from headless Chromium in CI-like conditions. The manual gate is what
  covers walking around, mouse-look feel, the pause menu, sound, and the
  settings path in step 8.
- **Firefox automation.** Playwright drives its own Firefox build, which is not
  installed in this environment; `test_first_playable.py --engine firefox
  --browser <path>` is wired up but unverified. Stock Firefox binaries are not
  drivable by Playwright.

## Non-goals for this gate

Multiplayer through the WebSocket proxy (after #5), performance benchmarks, and
mobile browsers (#9) are each tracked separately.
