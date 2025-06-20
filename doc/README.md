# Documentation

This directory contains mostly reference documentation for the Luanti engine.
For a less prescriptive and more guiding documentation, also look at:
https://docs.luanti.org

Note that the inner workings of the engine are not well documented. It's most
often better to read the code.

Markdown files are written in a way that they can also be read in plain text.
When modifying, please keep it that way!

Here is a list with descriptions of relevant files:

## Server Modding

- [lua_api.md](lua_api.md): Server Modding API reference. (Not only the Lua part,
    but also file structure and everything else.)
    If you want to make a mod or game, look here!
    A rendered version is also available at <https://api.luanti.org/>.
- [builtin_entities.md](builtin_entities.md): Doc for entities predefined by the
    engine (in builtin), i.e. dropped items and falling nodes.

## Client-Side Content

- [texture_packs.md](texture_packs.md): Layout and description of Luanti's
    texture packs structure and configuration.
- [client_lua_api.md](client_lua_api.md): Client-Provided Client-Side Modding
    (CPCSM) API reference.

## Mainmenu scripting

- [menu_lua_api.md](menu_lua_api.md): API reference for the mainmenu scripting
    environment.
- [fst_api.txt](fst_api.txt): Formspec Toolkit API, included in builtin for the
    main menu.

## Formats and Protocols

- [world_format.md](world_format.md): Structure of Luanti world directories and
    format of the files therein.
    Note: If you want to write your own deserializer, it will be easier to read
    the `serialize()` and `deSerialize()` functions of the various structures in
    C++, e.g. `MapBlock::deSerialize()`.
- [protocol.txt](protocol.txt): *Rough* outline of Luanti's network protocol.

## Misc.

- [compiling/](compiling/): Compilation instructions, and options.
- [ides/](ides/): Instructions for configuring certain IDEs for engine development.
- [developing/](developing/): Information about Luanti development.
    Note: [developing/profiling.md](developing/profiling.md) can be useful for
    modders and server owners!
- [android.md](android.md): Android quirks.
- [direction.md](direction.md): Information related to the future direction of
    Luanti. Commonly referred to as the roadmap document.
- [breakages.md](breakages.md): List of planned breakages for the next major
    release, i.e. 6.0.0.
- [docker_server.md](docker_server.md): Information about our Docker server
    images in the ghcr.
