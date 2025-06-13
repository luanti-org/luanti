# Documentation Index

The following is a list of documentation files and a brief description to aid
    in locating documentation (sorted alphabetically per-section).

## Lua

- [builtin_entities.md](builtin_entities.md): Doc for predefined engine
    entities, i.e. dropped items and falling nodes.
- [client_lua_api.md](client_lua_api.md): Client side enviorment's Lua API.
- [fst_api.txt](fst_api.txt): Formspec Toolkit API, this is for the engine's
    menus (Manu menu, world creation dialogs, etc.)
- [menu_lua_api.md](menu_lua_api.md): Lua API for the creation of engine menus,
    not the mod API.
- [lua_api.md](lua_api.md): Lua APIs for server mods. Documents the normal mod
    enviorment and mapgen enviorment and contains infromation related to
    texturing and structure of games, mods, and texture packs.

## Formats and Content

- [texture_packs.md](texture_packs.md): Layout and description of Luanti's
    texture packs structure and configuration. Related information can be
    found in the [mod API](lua_api.md).
- [world_format.md](world_format.md): Layout and description of Luanti's world
    format. Documents the structure of the dirrectories and the files therein,
    including the structure of the internal world related databases.
    **Important**: Due to incompleatness of this documentation, it is
    recommended to read the serialize and deserialize functions in C++ to
    better understand this documentation.
- [protocol.txt](protocol.txt): *Rough* outline of Luanti's network protocol.

## Misc.

- [android.md](android.md) Android related information.
- [breakages.md](breakages.md) List of major breakages.
- [compiling/](compiling/) Compilation information and instructions.
- [developing/](developing/) Extra information about Luanti development, e.g.
    OS-related information.
- [direction.md](direction.md) Information related to the future direction of
    Luanti.
- [docker_server.md](docker_server.md) Infromation about Docker server images.
- [ides/](ides/) Information about IDE configuration.