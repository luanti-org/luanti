# Embedding and configuring Luanti Web

The generated `luanti.html` is the supported web entry point. It owns the
launcher, Emscripten module, canvas, persistent filesystem, worker lifecycle,
and service worker. Embed that page rather than loading `luanti.js` directly;
the generated JavaScript is not a stable standalone component API.

## Minimal iframe

```html
<iframe
  src="https://play.example.org/?address=play.example.net&port=30000&game=devtest"
  title="Play Luanti" allow="fullscreen; cross-origin-isolated"
  style="width:100%;height:720px;border:0" allowfullscreen></iframe>
```

Because this build uses pthreads and `SharedArrayBuffer`, every document in the
frame chain must be cross-origin isolated. Serve both the embedding page and
Luanti responses with:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Resource-Policy: cross-origin
```

Do not sandbox the iframe unless the sandbox includes at least
`allow-scripts allow-same-origin allow-pointer-lock`. Add `allow-forms` if the
host wraps the launcher in a form workflow. Fullscreen additionally needs the
iframe `allow`/`allowfullscreen` permission shown above. Confirm
`crossOriginIsolated === true` inside the child frame before debugging an
Emscripten pthread failure.

Cross-origin isolated top-level pages cannot freely embed unrelated third-party
content. If a forum, wiki, or education platform cannot adopt COOP/COEP for its
whole frame chain, link to the standalone demo in a new tab instead.

## Deep links

The launcher accepts these query parameters:

| Parameter | Meaning |
| --- | --- |
| `address` | Prefills a server hostname or IPv4 address and opens Join server. |
| `port` | Server UDP port, default `30000`. |
| `name` | Player name, limited to letters, digits, `_`, and `-`. |
| `game` | Local game ID, currently `devtest`. |
| `view` | View-distance preset: `60`, `100`, or `160`. |
| `language` | Engine locale code when the deployment includes locale packs. |
| `proxy` | Custom `ws://` or `wss://` UDP proxy URL. HTTPS pages require WSS. |
| `autostart=1` | Start after assets and storage are ready if all fields validate. Ignored with `world`. |
| `worldname` | Preselect a saved world by name (it must belong to `game`). |
| `world` | HTTPS (or same-origin) URL of a `.luantiworld` to offer. The launcher downloads it, shows its game, size and mods, and imports a copy only if the player confirms. The host must allow CORS. |

An ordinary shared server link should omit `name`, `proxy`, and `autostart` so
the recipient chooses their identity and uses the deployment's nearest proxy.
The in-game Copy invite link button follows that rule. URLs never include a
password.

## Hosted proxy regions

Edit `launcher-config.js` in the deployed output, or its source at
`client/web/launcher-config.js`, to publish deployment-owned proxy regions:

```js
window.LUANTI_WEB_CONFIG = {
  serverListUrl: "https://servers.luanti.org/list.json",
  defaultProxy: "wss://eu-proxy.example.org/",
  proxies: [
    { id: "eu", label: "Europe", url: "wss://eu-proxy.example.org/" },
    { id: "na", label: "North America", url: "wss://na-proxy.example.org/" }
  ]
};
```

This file is public: do not put credentials in it. The launcher starts with
singleplayer-only networking when no hosted proxy is configured. Direct
connect and the public server list then explain that remote play needs a WSS
proxy rather than failing later in the engine.

## Browser behavior

- Clicking the game lets SDL request pointer lock. Escape releases pointer
  lock or fullscreen; the resume overlay returns focus to the canvas.
- The service worker precaches the coherent HTML, JavaScript, Wasm, data pack,
  and launcher assets. Offline use is available after installation completes;
  multiplayer and the live public server list still require a network.
- Worlds and settings remain origin-scoped in IndexedDB. Embedding the same
  deployment URL preserves that storage; moving to another origin does not.
  Players can export a world as a `.luantiworld` file and import it elsewhere.
  After the first save the launcher asks the browser for persistent storage
  (`navigator.storage.persist()`); browsers may grant it silently or not at all.

## World library API

`Module.luantiPersistence.library` (the same object as `Module.luantiWorlds`)
lets an embedder drive the world library the launcher uses. Every method
returns a promise, waits for storage to hydrate, and saves through the normal
persistence queue. Use it before the engine starts; once `callMain` has run the
engine owns the open world.

| Method | Result |
| --- | --- |
| `list()` | `[{name, game, size, files, created, lastPlayed, modified, creative, damage, hasThumbnail, path}]` |
| `create({name, game, seed?, creative?, damage?})` | The new world. `creative`/`damage` are left to the game when omitted. |
| `rename(from, to)`, `duplicate(from, to?)`, `remove(name)` | The changed world (`remove`: nothing). |
| `exportWorld(name, {writable?, onProgress?})` | A `Blob` (`application/zip`), or `null` after writing to a `FileSystemWritableFileStream`/`WritableStream`. |
| `inspectArchive(blob, {fileName?})` | What an import would do: `{name, game, gameAvailable, files, archiveBytes, uncompressedBytes, content, warnings, …}`. Writes nothing. |
| `importArchive(blob, {name?, fileName?, onProgress?})` | The imported world. Never overwrites; without `name` it picks a free one. |
| `fetchShared(url)` | `{blob, plan, host, …}` for a share link; does not import. |
| `games()`, `storage()`, `thumbnail(name)`, `limits()` | Available games, `{usage, quota, persisted}`, a JPEG `Blob` or `null`, the import caps. |

Failures reject with an `Error` whose `name` is `LuantiWorldError` and whose
`code` is one of `name`, `exists`, `missing`, `invalid`, `unsafe-path`,
`too-large`, `missing-game`, `quota`, `invalid-url`, `download`, `storage`.
The `message` is written for players.

Each completed change is announced with the existing `luanti:persistence`
window event: `event.detail` is the usual persistence state plus
`library: {operation, world, from?}`, where `operation` is `create`,
`rename`, `duplicate`, `delete`, `import`, `export` or `thumbnail`.
- Touch-only devices receive a warning. Browser touch controls remain roadmap
  work; use a keyboard and mouse for this MVP.

See [wasm.md](wasm.md) for building, serving, cache versioning, and deployment
headers.
