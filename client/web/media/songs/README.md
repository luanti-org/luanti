<!--
Luanti
SPDX-License-Identifier: LGPL-2.1-or-later
Copyright (C) 2026 The Luanti Contributors
-->

# Lace Waterfall — "Around"

The song the stage amps play, and the one the Lace Waterfall band and crowd
dance to (`games/luanti_web/routines/lace_waterfall_around.lua`). There are seven
mixes: the main "happy hard" mix and six alternate takes.

| Song (`/amp play <song>`) | Length |
|---------------------------|--------|
| `around_happy_hard`       | 159.02 s |
| `around_happy_hard_1`     | 199.33 s |
| `around_happy_hard_2`     | 152.23 s |
| `around_happy_hard_3`     | 162.84 s |
| `around_happy_hard_4`     | 152.93 s |
| `around_happy_hard_5`     | 155.52 s |
| `around_happy_hard_6`     | 160.46 s |

All of them are 175 bpm. The lengths in `lw_amps/init.lua` are measured from
these files. Late arrivals are synced with `start_time = elapsed % length`, so
if a file is replaced, its length has to be re-measured too:

```sh
ffprobe -v error -show_entries format=duration -of csv=p=0 lw_amps_around_happy_hard_l.ogg
```

## Source and rights

The original MP3s and the ogg conversions were supplied by the repository owner
for this showcase. The rights to the recording belong to its owner. These files
are **not** covered by the LGPL that covers the code. The licence for
redistributing them is still to be confirmed by the rights holder; do not reuse
them outside this pack until it is.

## Why these are runtime media, not game files

Everything in a mod's `sounds/` is game media: the server hashes it at
start-up and every client is told about it on join, whether or not a song
ever plays. Everything in the game directory is also part of the game's asset
package, which every visitor downloads before the menu. At 26 MiB the mixes are
most of the web demo, and only the concert arena plays them.

So they are not in `games/luanti_web` at all. They sit here, next to the page
(like the theater clips in `client/web/media/`), and are fetched at runtime:

1. `lw_amps` registers each song with `web_media = "songs/<song>"`. When a
   listener first comes into range, it calls `core.web_media_request` with that
   id (it answers at once: `ready`, `loading`, `busy`, `missing`, `failed`).
2. `client/web/runtime_media.js` finds the id in `asset-manifest.json`
   (`"media"`), downloads the two files (size and SHA-256 checked, one download
   at a time, so a listener cannot start a second), and writes them into the
   virtual filesystem at `/games/luanti_web/mods/lw_amps/songs/`.
3. `lw_amps` hands them on with `core.dynamic_add_media` (`client_cache =
   false`), then gives the files back (`core.web_media_release`) once the
   engine has them.

The engine still decodes the Oggs (`-sUSE_VORBIS`); the page only moves bytes.
A missing file leaves the band silent and does not throw. A native build has no
page to fetch from; copy the files to `games/luanti_web/mods/lw_amps/songs/`
(git-ignored) to hear them there.

`util/content/test_luanti_web_amps.py` fails if a song file turns up under a
media folder of a mod, or in `lw_amps/songs/`, or in any asset package.
Put any future song (or a lossless experiment) here too, named
`lw_amps_<song>_l.ogg` / `_r.ogg`; the build lists it in the manifest by itself.

## Why two mono files per song

Luanti only places a sound in the world if it is a single-channel Ogg Vorbis.
A stereo file is played with no position at all, and the engine warns
"Creating positional stereo sound". So every song is split into its two
original channels, and each channel plays from its own cab:

* `lw_amps_<song>_l.ogg` is the original **left** channel only. It plays from
  the **audience-left** cab.
* `lw_amps_<song>_r.ogg` is the original **right** channel only. It plays from
  the **audience-right** cab.

Levels are not cut by 6 dB. Both files of a pair are the same length. L−R is
not silence, so the stereo image is still there when you stand between the
cabs.

The files are named `_l` / `_r` and not `.1` / `.2`. Luanti treats
`<name>.<digit>.ogg` as a random sound group and would play only one file of
the pair. The `lw_amps_` prefix is the mod's own: sound `lw_amps_<song>_l` is
the file `lw_amps_<song>_l.ogg`.

Regenerate from a stereo master (Vorbis q6, 44.1 kHz):

```sh
ffmpeg -i around_happy_hard.mp3 -af "pan=mono|c0=c0" -ar 44100 -c:a libvorbis -q:a 6 \
	lw_amps_around_happy_hard_l.ogg
ffmpeg -i around_happy_hard.mp3 -af "pan=mono|c0=c1" -ar 44100 -c:a libvorbis -q:a 6 \
	lw_amps_around_happy_hard_r.ogg
```

Do **not** downmix to `(L+R)/2` on both channels of a stereo file. That
centres the bed, but it throws away the stereo difference, and Luanti still
will not place a stereo file on the amps.

## Budget

The 14 files are about 26 MiB. They are not part of any download before the
menu, the game package or a background package: the first listener who comes
into range of a song pays for that song's pair (about 3.5 MiB) and nobody else
does. The browser caches the file (a release publishes it by content hash under
`assets/`). A set may not be larger than 6 MiB (`maxSetBytes` in
`util/wasm/packages.json`), and the 30 MiB cap on the lot is a runtime-media
cap checked by `util/content/test_luanti_web_amps.py`. See the content budget in
`wasm_porting.md`.
