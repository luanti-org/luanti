<!--
Luanti
SPDX-License-Identifier: LGPL-2.1-or-later
Copyright (C) 2026 The Luanti Contributors
-->

# Stage amps and the Lace Waterfall band

`lw_amps` plays a song out of two amp cabinets. `lw_dance` puts Rubella,
Lillith and six fans on stage to dance to it. The first song is Lace
Waterfall's "Around", in seven mixes. The quickest way to hear it is the
**concert arena** (`/maps concert_arena`, or through the Around arena north-east
of the plaza and out of its east side): it has a stage with the two cabs
already on it, and the song starts when you walk in (see
[maps.md](maps.md#concert-arena)). It is the only map that plays "Around"; the
Around arena next door is lights and lasers only. Two cabs on any other floor
work too, as below. The **cabaret club** (`/maps cabaret`) is a
second stage, built and wired the same way, waiting for its own songs (see
[The cabaret club](#the-cabaret-club)).

## Trying it

1. Take two **Amp Cabinets** from the inventory (`i`). Put them on the stage's
   two front corners, a good few nodes apart (six or more). If they are only a
   couple of nodes apart, the floor hears both at the same distance and the
   stereo collapses to mono.
2. Look at the cab on the **audience's left**, as seen from the floor facing
   the stage, and type `/amp left`. Look at the other one and type
   `/amp right`. Each sound comes from one node above the cab you pointed at.
3. **Punch either cab.** "Around" starts, and the band and crowd appear:
   Rubella by the left cab, Lillith by the right, and six fans in two rows off
   the front of the stage, facing the band.
4. Punch a cab again to stop. The band stays on stage, standing idle, until
   the song starts again.

From chat:

```
/amp                    where the cabs are, what is playing
/amp left | right       set the cab you are looking at (server privilege)
/amp play [<song>]      play a song (default: around_happy_hard)
/amp stop               stop it
/amp list               the songs
/amp hear <nodes>       how far the cabs carry (default 80; server privilege)
/amp gain <g>           source gain (default 3; server privilege)
```

`/routine start lace_waterfall_around` also works. If the cabs are set up, it
starts the song as well. `/routine stop lace_waterfall_around` stops both.

## How the song is played

Luanti only places a single-channel Ogg in the world. A stereo file plays with
no position. So each song is **two mono files**: the original left channel and
the original right channel (see `mods/lw_amps/songs/README.md`). They are
routed so that the audience-left cab plays `_l` and the audience-right cab
plays `_r`.

For each listener the pair is two `core.sound_play` calls in the **same server
step**. Both have `loop = true`, the same `start_time` and `to_player` set to
that listener. A visitor who walks in later is given the pair at
`start_time = elapsed % length`, so they hear the same point in the song as
everyone else, and the same point the band is dancing to.

Things that are easy to get wrong:

* `max_hear_distance` only admits players who are inside it when a playback
  *starts*. So the amps check every second for anyone who has walked into
  range, and start the pair for them. A listener who walks well out of range
  (16 nodes past it) has their pair stopped, which frees two OpenAL sources in
  the browser tab. Walking back in restarts it, in time.
* Positional gain is the level at **3 nodes** from the source. It does not get
  louder inside 1 node, and OpenAL clamps the final gain at 1. So a gain above
  1 does not make the cab louder close up. It carries full volume further out.
  The default of 3 keeps the floor in front of the stage at full level.
* The song clock counts **server steps**, not wall time. In singleplayer the
  pause menu stops the server and pauses every sound, so a wall-clock song
  would come back ahead of the music.

With no sound at all (no OpenAL, a missing file, a `sound_play` that throws),
the song still "plays". The clock runs and the band dances in silence.

## The band and the dance map

The dance is `routines/lace_waterfall_around.lua`, a routine with
`songs = {...}`. When one of its songs starts, the director runs it with its
clock slaved to the song. The details are in
[routines.md](routines.md#dancing-to-a-song). In short:

* **Cast:** `rubella` and `lillith` (group `band`) on stage, and `crowd_1` …
  `crowd_6` (group `crowd`) on the floor. That is eight, the director's cap.
* **Stage:** the routine has no registered stage, so it is built from the
  cabs. The band stands one node back from the line between them, and the
  audience is on the other side. The crowd `settle`s onto the floor in front,
  even if that floor is lower than the stage. A map that registers a stage
  (`lw_dance.register_stage`) or places `lw_dance:mark_*` nodes overrides
  this.
* **Tempo:** 175 bpm. Every move is stretched so that its cycle lands on the
  beat.
* **Sections:** intro, verses, choruses, bridge, build and outro, on eight-bar
  lines. This is a **placeholder** structure until the song is timed. Each
  section is one line with its starting `beat`.
* **Moves:** the pad's `groove`, `wave`, `spin`, `stomp` and `bow`, plus the
  concert moves `sway`, `pogo`, `clap` and `headbang`.

All seven mixes share this one map. A new song only needs its pair in
`client/web/media/songs/` (not `sounds/`, and not in the game: see Cost), a `register_song` line with the length measured from
the files, and a routine naming it in `songs`. It can reuse the same cast.

## Map stages

A map with `amps` in its `lw_maps` row registers its two cabs with
`lw_amps.register_stage`, together with what that stage plays: a `song`
(the concert arena: `around_happy_hard`), or a `playlist` (the cabaret: `cabaret`,
meaning the first song registered with `playlist = "cabaret"`). Punching one
of a stage's cabs while nothing is playing points the amps at that stage and
plays its song, wherever `/amp left` and `/amp right` last pointed. Cabs you
place yourself are not a stage, and behave as described under
[Trying it](#trying-it).

`lw_amps.stage_song(stage)` is what a stage plays: its `song` if that is
registered, else the first song in its `playlist`, else nothing. A stage with
nothing to play stays quiet, and punching its cab says why.

## The cabaret club

The cabaret (`/maps cabaret`, or the causeway from the plaza's south-west
corner) has a proscenium stage with two cabs on its front corners, twelve
nodes apart. It is built for Rubella and Lillith but ships with **no songs**:
the slot is `mods/lw_amps/cabaret/`.

### Adding a cabaret song

1. Cut the stereo master into a mono pair, 44.1 kHz Ogg Vorbis, named
   `lw_amps_<name>_l.ogg` (the original left channel) and
   `lw_amps_<name>_r.ogg` (the right channel). Use a `<name>` of lowercase
   letters, digits and `_`, for example `cabaret_velvet`:

   ```sh
   ffmpeg -i velvet.wav -af "pan=mono|c0=c0" -ar 44100 -c:a libvorbis -q:a 4 lw_amps_cabaret_velvet_l.ogg
   ffmpeg -i velvet.wav -af "pan=mono|c0=c1" -ar 44100 -c:a libvorbis -q:a 4 lw_amps_cabaret_velvet_r.ogg
   ```

2. Put both files in `games/luanti_web/mods/lw_amps/cabaret/`, **not**
   `mods/lw_amps/sounds/`.
3. Measure the length:
   `ffprobe -v error -show_entries format=duration -of csv=p=0 lw_amps_cabaret_velvet_l.ogg`.
4. Add one line at the bottom of `mods/lw_amps/cabaret/songs.lua`:

   ```lua
   cabaret("cabaret_velvet", {title = "Velvet", length = 183.42, bpm = 112})
   ```

   `cabaret()` calls `lw_amps.register_song` with `media_dir = "cabaret"` and
   `playlist = "cabaret"`. If either file is missing, the song is skipped with
   a warning in the log.
5. Restart the server (on the web: rebuild and reload).

From then on, walking into the cabaret plays the first song in `songs.lua`,
punching a cab stops or restarts it, and `/amp play <name>` plays any of
them. The dance is `routines/cabaret_revue.lua`. It takes every song in the
`cabaret` playlist when the server starts, so it needs no edit: Rubella and
Lillith go on the cabaret stage with four fans on the dance floor and follow
the song's clock, and leave the stage when it stops. Its moves are a
placeholder revue at a nominal 112 bpm, the routine's own tempo. Retime its
sections against the real songs, or copy it per song if their tempos differ
a lot. The Stage Director on the cabaret floor runs the same routine by
hand, before any song is added or with one.

### Why the songs are not join-time media

A `sounds/` folder is part of the media list. Every client downloads that
list and checks its hashes before the world loads, which is why the "Around"
pairs moved out of it, and later out of the game altogether (see Cost). `cabaret/` is not a media folder. Instead,
`lw_amps` sends each cabaret pair with `core.dynamic_add_media`
(`to_player`, not ephemeral, `client_cache = false`), once per visitor, when
that visitor first comes within earshot of the cabaret's cabs. Until a
listener's client has both files, that listener is not started. The next
once-a-second sweep starts them at `start_time = elapsed % length`, in time
with the song and the band.

That keeps the songs out of the engine's join-time media. In the web build
the cabaret's files are still inside the `luanti_web` package (everything
under `games/luanti_web` except the Around songs), so committed pairs grow it
by their size. Keep cabaret pairs few and small, or make a large set runtime
media (`client/web/media/`, a `media` entry in `util/wasm/packages.json`) and
register its songs with `lazy_media = true` and `web_media`, as the Around songs do. `mods/lw_amps/cabaret/README.md` has the details.

## Cost

* **Not on the default path.** The songs are not in the game at all (they are
  runtime media in `client/web/media/songs/`; the web build writes a pair into
  `mods/lw_amps/songs/`, which is not a media folder, when a listener needs
  it), so the server does not hash them at start-up and a client joining the
  showcase is not sent or told about them. Before this,
  everything under `sounds/` (about 26 MiB) was hashed and announced on every
  start. `lw_amps` sends a pair with `core.dynamic_add_media` (`to_player`,
  `client_cache = false`) only when that song is about to play for a listener
  in range — in the pack, only on the concert arena — and starts their two
  sources on the next sweep, once the client has the files, at
  `elapsed % length` like any late arrival.
* **Download:** the 14 files are about 26 MiB, but no part of what a visitor
  waits for, and nobody downloads one until a listener comes into range of a
  stage that plays it: then the page fetches that song's pair (about 3.5 MiB,
  one download at a time) and the mod sends it on. The songs are registered
  with `lazy_media = true` and `web_media`, so a stage punched before its
  files have arrived stays quiet and starts, in time, on a later sweep; a
  missing file leaves it silent. See "Runtime media" in `wasm_porting.md`.
* **Audio:** one song at a time, and two streamed sources per listener in
  range.
* **Entities:** eight dancers. The director already holds that to ten pose
  keys a second, with unchanged bones dropped, and sends nothing when nobody
  is within 48 nodes. Catching up (a song starting part-way, a loop, a visitor
  walking back in) sends each dancer one move, not every step in the map.
