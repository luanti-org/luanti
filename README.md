
<div align="center">
    <img src="textures/base/pack/logo.png" width="32%">
    <h1>ExA-Client</h1>
    <p>A free and open-source voxel game engine with easy modding and game creation.</p>
    <a href="https://github.com/luanti-org/luanti/"><img src="https://img.shields.io/badge/based%20on-Luanti%205.18.0-blue" alt="Based on Luanti 5.18.0"></a>
    <a href="https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html"><img src="https://img.shields.io/badge/license-LGPLv2.1%2B-blue.svg" alt="License"></a>
</div>
<br>

# ExA-Client

ExA-Client is based on Luanti 5.18.0, a free and open-source voxel game engine with easy modding and game creation.

Copyright (C) 2010-2026 Perttu Ahola and contributors.

## Documentation

- Luanti documentation: https://docs.luanti.org/
- Community forum: https://forum.luanti.org/
- Original source repository: https://github.com/luanti-org/luanti/
- [Developer documentation](doc/developing/)
- [Documentation directory](doc/)

## Default Controls

All controls can be rebound in settings.

| Button | Action |
|---|---|
| Move mouse | Look around |
| W, A, S, D | Move |
| Space | Jump / move up |
| Shift | Sneak / move down |
| Q | Drop itemstack |
| Shift + Q | Drop single item |
| Left mouse button | Dig / punch / use |
| Right mouse button | Place / use |
| I | Inventory menu |
| T | Chat |
| / | Command |
| Esc | Pause menu / exit |
| F1 | Hide / show HUD |
| F2 | Hide / show chat |
| F5 | Debug information |
| F10 | Show / hide console |
| F12 | Take screenshot |

## Paths

- `bin` — Compiled binaries
- `share` — Distributed read-only data
- `user` — User-created and modifiable data

Worlds are stored in `user/worlds/`.

## Configuration File

Default configuration file: `user/minetest.conf`

Use `--config <path-to-file>` to specify a configuration file.

## Command-line Options

Use `--help` to view supported command-line options.

## Compiling

ExA-Client is based on the Luanti source code.

- [Common compilation information](doc/compiling/README.md)
- [Android build files](android/)

## Docker

- [Docker development documentation](doc/developing/docker.md)
- [Docker server documentation](doc/docker_server.md)

## Version Scheme

ExA-Client is currently based on **Luanti 5.18.0**.

The upstream project uses a `major.minor.patch` version scheme. ExA-Client may use its own version identifiers for future releases.
