<!--
Luanti
SPDX-License-Identifier: LGPL-2.1-or-later
Copyright (C) 2026 The Luanti Contributors
-->

# `songs/`: where lw_amps looks for song files

This folder ships with no audio, on purpose. `util/content/test_luanti_web_amps.py`
fails if a mix turns up here, because everything in the game directory is
downloaded before the menu by every visitor to the web build.

The Lace Waterfall "Around" mixes (26 MiB) live in
[`client/web/media/songs/`](../../../../../client/web/media/songs/README.md),
next to the page. In the web build `lw_amps` asks for a song by id
(`core.web_media_request("songs/around_happy_hard")`), the page fetches the
pair and writes it into this folder in the virtual filesystem, and `lw_amps`
sends it on with `core.dynamic_add_media`. A missing pair leaves the stage
silent.

A native install has no page to fetch from. To hear the songs there, copy the
`lw_amps_*.ogg` files from `client/web/media/songs/` into this folder (they are
git-ignored here) and `lw_amps` reads them as before.
