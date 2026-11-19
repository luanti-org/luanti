<!--
Luanti
SPDX-License-Identifier: LGPL-2.1-or-later
Copyright (C) 2026 The Luanti Contributors
-->

# The themed maps

Eleven authored maps ship with the showcase pack, as islands around the hub
plaza. They are design pieces, not procedural terrain: each one is a single
committed schematic, compact enough for the WASM heap and short enough to walk
in about three minutes.

```
                                                 [ concert arena ]
                                                         |  (spur from the
  [ thunder chapel ]---.                  .---[ Around arena ]  arena's east side)
    north-northwest    |                  |     north-northeast
  [ smoke caldera ]---.|    [ hub tour ]  |   .---[ crystal bazaar ]
      northwest       ||         |        |   |        northeast
        [ Halloween ]-++---[ hub plaza ]--'---+---[ fruit garden ]
             west            |    |     |               east
                             |    |     '----------.
                             |  [ snow mountain ]  |
                             |       south         |
       [ lighthouse ]--------'                     '--[ rainbow storm ]
         southwest                                       southeast

  [ cabaret club ]  west of the lighthouse: its causeway leaves the plaza's
   west-southwest   south-west corner, south of the Halloween lane's
```

Every causeway is walkable. Fly, fast and noclip are on for everyone, but the
tour never needs them.

## The maps

| Map | `id` | Schematic | Footprint (`size`) | Origin (min corner) | Spawn | Time of day |
|-----|------|-----------|--------------------|---------------------|-------|-------------|
| Halloween lane | `halloween` | `map_halloween.mts` | 36 × 20 × 36 | `(-88, 5, -26)` | `(-55, 9, -9)` | dusk (`day_night_ratio = 0.16`, fogged sky) |
| Snowy mountain | `snow_mountain` | `map_snow_mountain.mts` | 40 × 28 × 40 |  `(-20, 5, -74)` | `(6, 9, -37)` | bright overcast (`0.85`, pale sky) |
| Giant fruit garden | `fruit_garden` | `map_fruit_garden.mts` | 40 × 20 × 40 | `(53, 5, -28)` | `(55, 9, -7)` | the world's own midday |
| Lighthouse tide | `lighthouse_tide` | `map_lighthouse_tide.mts` | 36 × 24 × 36 | `(-66, 5, -80)` | `(-39, 9, -46)` | late blue hour (`0.28`, dark water sky, light fog) |
| Crystal night bazaar | `crystal_bazaar` | `map_crystal_bazaar.mts` | 36 × 16 × 36 | `(54, 5, 24)` | `(55, 9, 42)` | night (`0.08`, saturated violet sky, no fog of its own) |
| Smoke caldera | `smoke_caldera` | `map_smoke_caldera.mts` | 38 × 22 × 38 | `(-94, 5, 22)` | `(-57, 9, 41)` | overcast red hour (`0.35`, warm close fog) |
| Rainbow storm | `rainbow_storm` | `map_rainbow_storm.mts` | 64 × 8 × 24 | `(40, 5, -120)` | `(41, 9, -109)` | thunderstorm (`0.45`, slate sky, fog at 90, rain and lightning) |
| Around arena | `around_arena` | `map_around_arena.mts` | 25 × 12 × 25 | `(29, 5, 76)` | `(41, 9, 76)` | night (`0.1`, violet sky, light fog at 120) |
| Thunder chapel | `halloween_storm` | `map_halloween_storm.mts` | 36 × 20 × 36 | `(-60, 5, 77)` | `(-41, 9, 77)` | stormy Halloween night (`0.1`, bruised violet sky, close fog at 64, rain, lightning and thunder) |
| Cabaret club | `cabaret` | `map_cabaret.mts` | 27 × 11 × 27 | `(-100, 5, -55)` | `(-74, 9, -41)` | late night (`0.06`, deep red sky, close fog at 56); indoors, lit by its own lamps |
| Concert arena | `concert_arena` | `map_concert_arena.mts` | 49 × 13 × 49 | `(35, 5, 120)` | `(59, 9, 120)` | night (`0.08`, magenta sky, light fog at 140) |

Local `y = 0` of every map schematic is stamped at world `y = 5`
(`lw_maps.base_y`), so local `y = 3` is the world's `GROUND` and local `y = 4`
its `FLOOR` — the same two levels the rest of the showcase is built on.

**Time of day is per player, not per world.** The hub's clock is frozen at
midday (`time_speed = 0` in `minetest.conf`), and freezing it somewhere else
would move the whole world. Instead `lw_maps` watches where each player is and
calls `override_day_night_ratio` and `set_sky` when they cross onto a map, and
resets both when they leave. That is what lets a dusk lane and a bright garden
exist in one world at the same time, and it costs nothing while nobody is on a
map.

The reset is `player:set_sky()` with no argument. `set_sky({})` looks like a
reset and is not one: the engine starts from the player's current sky and
changes only the fields the table names, so an empty table changes nothing and
a lane's fog would follow the visitor home.

### Halloween lane

A lantern-lit lane from the causeway to a porch stage, with a pumpkin patch to
the south and a graveyard and a haunted house to the north.

* The house has a hallway straight through from the front door, side rooms with
  lit windows, an attic up a plank stair, and a cellar down another — three
  levels in fourteen nodes of footprint.
* Lanterns gutter: `lw_maps:flicker` is an airlike node timer that swaps the
  jack-o'-lantern below it for an unlit pumpkin on a fixed pattern, phase-offset
  by position so the lane does not blink in unison. Dig the lantern out and the
  controller leaves the hole alone.
* A weather vane spins on the house ridge (`lw_maps:weather_vane`, an entity
  with `static_save = false`).
* No mobs. The theme is lighting, silhouette and fog.

### Snowy mountain

A flat-topped cone you can climb *and* go through.

* **Up:** a spiral shelf cut into the cone, one node of rise every four steps,
  from the trailhead to a railed overlook on the summit plateau.
* **Through:** a tunnel at trail level runs north–south under the peak and
  comes out on the far face, with a portal and a slab ramp at each mouth.
* **Ice cave:** blue glass walls and a packed-ice floor, with its own mouth on
  the west face and a link east into the through-tunnel.
* **Mineshaft:** a timbered stair climbing from the through-tunnel up to a mouth
  on the trail. Its exit is not a hard-coded position: the generator picks
  whichever tread of the spiral is nearest, so the shaft still meets the trail
  if the cone's slope ever changes.
* Lighting is `lw_maps:torch` (light 12) every few nodes, not ceiling lamps, so
  the caves read as caves.

### Giant fruit garden

Oversized produce as architecture.

* A **walk-in watermelon**, 13 × 11 × 13: striped rind, a course of flesh, seed
  blocks, a hollow room with a doorway on the path side and hidden lights
  inside.
* A **sliced melon** set into the lawn as an amphitheatre: terraces that step
  one node every 2.2 of radius, seeds pressed into them, and two pairs of
  theater seats.
* Giant apple, grape cluster under a canopy, banana, citrus and strawberry.
* A juice channel of translucent nodes from the bowl to a basin.

Every fruit here is dyed wool from the core palette. The map adds no fruit
nodes at all.

### Lighthouse tide

A lagoon at blue hour, with land round three sides of it: the causeway lands on
the north shore, a spine runs down the west side to a headland with the
lighthouse on it, and a rocky ledge runs down the east side.

* **The lighthouse** is a hollow shell, thirteen nodes across, painted in
  bands of white and red. A spiral stair winds round a solid core from the
  door to the lantern: a helicoid, one rise every few treads, each turn four
  nodes above the last, and a stair node on every tread that is one up from
  its neighbour, low half towards it, so the climb is half steps. The last two
  rises come up through a hatch in the gallery rather than through the
  lantern's floor.
* **The lantern room** is a ring of glass on the gallery deck with a door on
  the seaward side and the lamp in the middle. The beam is one entity
  (`lw_maps:lighthouse_beam`): a long glowing translucent sprite that turns
  about the lamp, `static_save = false` like the Halloween vane. Punch that
  lamp and a short horn plays (`lw_lighthouse.ogg`). It stays silent when
  sound is off, or when the file is missing. The other lamps on the island
  are the same node and do not play it.
* **The jetty** stands on piles one node proud of the lagoon, with two tide
  pools set into its deck: water on glass, so you look straight down into the
  lagoon.
* **The tunnel** runs along the lagoon bed from a stairwell on the west spine,
  under the lagoon and under the jetty, to a stairwell on the east ledge — the
  far side of the jetty. Its walls and roof are glass wherever there is water
  outside them, and the first tide pool's glass floor is its roof, so from the
  tunnel you look up into the pool and from the jetty down into the tunnel.
  The lagoon's surface is level with the shore, so a visitor's head in the
  tunnel is below the waterline.

No boats, no flowing water, no mobs: the water is the pack's still
`lw_nodes:water`, and the map adds no nodes.

### Crystal night bazaar

The fruit garden's oversized-architecture idea at night, where glass and a few
warm lamps do what new shaders would otherwise have to.

* **The arcade** runs east–west through the middle of the map from the
  causeway, in two wings either side of a rotunda. Each wing is a barrel vault
  of glass panes between violet wool ribs, springing from the stalls' back
  walls: hollow, one node thick.
* **The stalls** line both sides of both wings: a glass shelf, a counter, and a
  dyed awning. Their posts carry lanterns (`lw_maps:lantern`), and a `chase`
  controller over each one lights them round the arcade in a lap — along the
  south side, back along the north — two at a time. The controllers' places in
  the loop are in their `param2`.
* **The rotunda** is a brick drum with doors on the aisle, a hall under the
  dome lit from below, and a terrace round the dome's foot reached by a stair
  from the north souk. **The dome** is glass, and low: a paraboloid whose
  steepest step is one node, so a visitor walks up it to the crown. A Stage
  Director on the rim plays the theater's `chorus_line_v1` on the crown (see
  [`routines.md`](routines.md)).
* **The cistern** is under the rotunda, down a stairwell in the hall floor: a
  pillared brick vault with a still pool, lit only by `lw_maps:torch`, in the
  snow mountain's cave language.
* **The souks** north and south of the arcade are open squares with crystal
  clusters — glass cores in dyed wool, lit from inside — awnings on poles and
  lamp posts.

Everything is glass and dyed wool from the core palette; the only new nodes are
the lantern and its unlit twin, both the palette's lamp tile re-tinted.

### Smoke caldera

A volcano in the snow mountain's grammar — up *and* through — built as a shell.

* **Up:** a ring trail cut into the flank from the trailhead on the east face,
  one node of rise every four steps, to the rim: a walkway about three nodes
  wide that goes all the way round the crater.
* **The observation lip** is a glass cantilever off the north rim, out over the
  crater, with a glass parapet at its tip. Stand on it and the lava is under
  your feet.
* **Down:** a switchback ledge cut into the crater's inner wall, starting a
  quarter turn round the rim from where the trail tops out, down to the crater
  floor: a ring of basalt round a lava lake.
* **The lava** is dyed wool that glows (`lw_maps:lava`), with a crusted twin
  (`lava_crust`) and a `flicker` controller over about one cell in three — the
  Halloween lanterns' pattern, on a lake. It is solid, it does not hurt, and it
  does not flow.
* **Through:** a packed-basalt tube from the crater floor out through the west
  face — the far side from the causeway — stepping down twice on half slabs to
  a portal on the ash field, torch-lit.
* **A shell, not a fill.** The cone is generated solid, the trail, ledge, lip
  and tube are cut into it, and then every node of rock more than two from
  open space is emptied. Every cut therefore has walls two nodes thick and
  nothing behind them, and the hollow is sealed by construction; the test
  checks both, and that no visitor can see into it. No particles: the smoke is
  the fog.

### Rainbow storm

A walk over rainbows, under a thunderstorm. The storm is the sky, not the
schematic: the map is two landings, a lagoon and the arches between them, and
nothing stands over the deck.

* **The arches** are two, low and long, sharing a foot in the middle of the
  lagoon. Their feet are level with the causeway, so the landing is not a step,
  and they rise three nodes to the crest one node every four steps, so the
  whole walk is level ground and single steps. The deck is one node thick,
  seven wide, and a band of wool per row: red, orange, yellow, green, cyan,
  blue, violet. Where it comes down to the water it stands on piers of the same
  bands.
* **Lightning rods** stand on a gold plinth beside each crest, one node proud
  of the deck, on alternating sides. Every bolt lands on a rod, so the path is
  never the target.
* **The storm** runs only for someone on the island. Rain is one particle
  spawner attached to the visitor — a few dozen streaks in a box over their
  head, not over the island — and it is deleted when they leave. Every 6, 8
  and 7 seconds in turn the sky flashes white for about a tenth of a second,
  the day/night ratio jumps to full daylight with it, and a zigzag bolt comes
  down onto the next rod. The beat is `core.get_gametime()`, so everyone on the
  island sees the same flash on the same rod, and a reload needs nothing saved.
* The flash calls `set_sky` directly and then puts the storm's own sky back.
  It does not go through the atmosphere helper: that does nothing when the map
  has not changed, and its reset would drop the storm's fog. A flash that is
  still in the air when the visitor leaves does not put the storm back on them.
* No thunder here: the flash is the beat. The pack's thunder is the
  [thunder chapel](#thunder-chapel)'s, and turning it on for this island is
  one `thunder = "lw_thunder"` line in its `storm`.

The map adds no nodes. The rain is the pack's one texture that is not a node:
`lw_rain.png`, a thin streak, and the bolts are the same streak brightened.

### Around arena

Lace Waterfall's stage, at night: a light show, with no music of its own. It
is the smallest map in the pack, 25 × 25, and the causeway comes in under a
marquee on its south side. Out of its east side, between two truss columns, a
spur causeway runs on to the [concert arena](#concert-arena), which is where
the song plays.

* **The stage** is at the north end, a black deck one node up with a violet
  lip, facing south down the floor, with a stair at each side of the front.
  Two dummy speaker stacks (black wool) stand on its front corners where the
  amp cabs used to be.
* **No song.** The arena has no `amps` in its registration and no
  `lw_amps:cab` in its schematic, so walking in starts nothing and sends no
  song files. It used to start "Around"; that moved to the concert arena so
  the heavy audio has one home (see [`amps.md`](amps.md#cost)).
* **The truss** runs round the whole edge of the map, six nodes up on
  columns, with the backdrop wall as its north side. It carries 32 truss
  lights (`lw_maps:stage_light`), one every third cell from a corner, and a
  `chase` controller over each one. 96 cells round, 32 lights, 4 chase
  periods of 8, so the lit pairs lap the arena with no seam.
* **The lasers.** Four emitters, glowing cubes in the band's violets and a
  white, race round the top of the truss the same way the lights chase, a lap
  every eight bars of "Around" (32 beats at 175 bpm, about 11 s). Each one
  throws a beam down to a point that circles the dance floor the other way,
  twice a lap, so the beams sweep across the crowd while the emitters race.
  A beam is one thin `upright_sprite` that the server stretches and aims ten
  times a second: its height is laid along the beam with
  `vector.dir_to_rotation`, face side-on, so the floor sees it broadside. They
  exist only while someone is on the arena, are spawned on the first
  arrival and removed with the last visitor, and are never saved
  (`static_save = false`). A lost one (its block unloaded) is put back.
* **The banners.** The backdrop is black cloth with the band's banners set
  into it, facing the floor: "AROUND!" across the top, full-length portraits
  of Rubella (audience left) and Lillith (audience right) either side of the
  stage, and three violet-and-black Lace Waterfall banners — one behind
  the middle of the band. The same "AROUND!" sign is the marquee over the
  entrance, facing the causeway, and four more Lace Waterfall banners hang
  from the side trusses, facing in.
* **The floor** is a two-node checker of black and violet wool, lit by four
  hidden lamps under the truss so the crowd is seen between the chasing
  lights.

The portraits are drawn from the dancers' own skins: the front of every box of
the character, as she stands. A still exported from the band's footage can
replace either one, under the same name, at any size twice as tall as it is
wide, because `lw_maps` cuts it 2 × 4.

### Thunder chapel

The Halloween lane's props under the rainbow storm's sky: a stormy Halloween
night on an island of its own, north-west of the hub. It is a separate map
from the lane, and the lane is unchanged. The island is the lane's own box,
36 × 20 × 36. The causeway comes up from the south, under a dead-wood arch
hung with cobwebs, onto a lantern-lined path to the chapel door.

* **The chapel** is a roofless brick shell, 13 × 17, its front and east wall
  broken off at the top in a fixed pattern, black rafters across the nave, and
  a jack-o'-lantern in every window. There are pews either side of the aisle,
  and a bone-white altar under the loft.
* **The organ loft** runs across the north end, five nodes up, reached by a
  plank stair up the west wall. It has a rail along its front and a rank of
  gold-capped pipes against the back wall, tallest in the middle, with a lit
  console in front of them. This is the island's high platform: from it you
  watch the strikes over the ruin. It is also where the optional organ track
  plays from (below).
* **The bell tower** stands behind the chapel. It has a belfry arch through
  every face and a black spire, and the island's tallest lightning rod is on
  top.
* **The graveyard** fills the west side: a fenced dirt yard of bone-white
  gravestones, a gate onto the path, and a dead oak in the far corner that
  carries the second rod.
* **The mausoleum** to the east is a walk-in bone-white tomb with columns at
  its corners, torches and cobwebs inside, and the third rod on a gold plinth
  on its roof.
* **A pumpkin patch** with jack-o'-lanterns among the pumpkins, hay bales and
  bare trees, by the causeway.
* The lanterns gutter, using the same `flicker` controller as the lane. No
  mobs and no water. The map adds no nodes and no textures.

**Lightning.** The storm is `lw_maps`' own, the same code as the rainbow
storm, given three rods instead of two. While someone is on the island they
get rain from one particle spawner attached to them. Every 6, 8 and 7 seconds
in turn their sky flashes white, the day/night ratio jumps to full daylight
for about a tenth of a second, and a zigzag bolt comes down onto the next rod:
spire, oak, mausoleum, round and round. The beat is `core.get_gametime()`, so
everyone on the island sees the same strike on the same rod. All of it is
per player, and it stops when they leave. The rain spawner is deleted, no
more flashes come, and a flash still in the air does not follow them home.

**Thunder.** Each strike is followed by a roll of thunder, played
positionally at the rod that was hit and only to the player who saw the
flash. It comes 0.25 s after the flash at the rod, 0.02 s later for every
node between the visitor and the strike, and never more than 1.5 s late.
Stand by the spire and it cracks overhead; stand by the causeway and it
rumbles in from across the island. The sound is `lw_thunder`, three short
mono Ogg Vorbis variants in `lw_maps/sounds/` (`lw_thunder.1.ogg` to
`.3.ogg`, 2.4–2.8 s each, under 24 KiB together). Luanti picks one at random
for each strike. Each is a fire-and-forget sound (`ephemeral`). If the
visitor leaves before it is due it is not played, so there is nothing to
clean up. The rainbow storm does not use it.

Thunder needs the browser's sound path to be working: the OpenAL build, and
the Web Audio context unlocked by the first click or key press (the
launcher's `unlockAudio`). Until then, or without OpenAL, or if the files are
missing, the storm is silent and nothing errors. `lw_maps` feature-detects
`core.sound_play` and calls it under `pcall`, as it does for the lighthouse
horn.

**The organ slot.** The chapel is built for a spooky pipe-organ soundtrack.
The pack does not include one. The slot is the `organ` field of the
`halloween_storm` registry row:

```lua
organ = {
	at = {x = 19, y = 12, z = 28},              -- map-local: over the loft console
	sound = "lw_maps_halloween_storm_organ",
	gain = 0.6,
	max_hear_distance = 48,
},
```

To fill it, save a **mono** Ogg Vorbis file as
`games/luanti_web/mods/lw_maps/organ/lw_maps_halloween_storm_organ.ogg` and
restart the server (on the web: rebuild and reload). No code change or
register call is needed. When a visitor arrives, `lw_maps` pushes the file to
that player alone with `core.dynamic_add_media`. Once their client has it,
`lw_maps` loops it from the loft for them (`loop = true`, positional,
`to_player`), and fades it out when they walk off. If the file is not there,
the chapel is quiet apart from its thunder.

The file goes in `organ/`, not `sounds/`, and that is what keeps it isolated.
The engine collects join-time media only from a mod's `textures`, `sounds`,
`models` and `media` folders. A track in `organ/` is therefore not in the
media list and hashes that every client downloads before the world loads, and
nobody who never walks onto the chapel receives it. The web build is a
separate cost. Everything under `games/luanti_web` except the Around songs
is in the `luanti_web` package, so a committed track still adds its size to
every showcase start. Keep it to a short mono loop, or keep it out of the repository and
copy it into the build that should have it.
[`mods/lw_maps/organ/README.md`](../mods/lw_maps/organ/README.md) has the
encoding command, the size guidance and the details.

### Cabaret club

A dim, warm club for Lace Waterfall's Rubella and Lillith, on its own island
west of the lighthouse. It is the Around arena's stage idea indoors, at a
smaller scale and lower volume. The box is 27 × 11 × 27 (8,019 cells, a small box
like the arena's). The causeway comes in from the
east, under a red awning on gold posts, with a lantern on a pedestal either
side of the door.

* **The room** is a brick box with a black roof. Inside, the light is its own:
  warm lanterns set high in the walls, a candle on every table, a lit back
  bar and the stage's footlights. Outside, the sky is a deep red night with
  close fog (`day_night_ratio = 0.06`).
* **The stage** runs along the west wall and faces east, down the room. It
  has a plank deck one node up with a gold lip, a velvet backdrop and wings,
  and a velvet valance under a gold proscenium header. A stair goes up at
  each end of the front. Eight lanterns hang in a row in front of the header,
  each with a `chase` controller above it, so the bulbs chase along the
  proscenium two at a time, like the bazaar's stall lanterns.
* **The cabs.** Two `lw_amps:cab`s stand on the front corners of the deck,
  twelve nodes apart, with their grilles to the room (`facedir` 3). Facing
  the stage means looking west, so the audience-left cab is the south one,
  at local `(8, 5, 7)`. The audience-right cab is at `(8, 5, 19)`. The sound
  sources are one node above each.
* **The floor** is a two-node checker of black and red wool in front of the
  stage. Beyond it are six candlelit tables with a chair either side, and a
  bar along the north wall with stools. An aisle runs clear from the door to
  the dance floor.
* **A Stage Director** (`lw_dance:director`) stands on the floor south of the
  stage stair. Punch it and Rubella and Lillith come on stage with four fans
  on the dance floor, dancing the placeholder `cabaret_revue` routine on its
  own clock. Punch it again to send them off. It works before any song has
  been added.

**The songs.** The cabaret ships with no audio. Its `amps` row names a
playlist, not a song:

```lua
amps = {
	left = {x = 8, y = 6, z = 7},       -- audience-left (south) source
	right = {x = 8, y = 6, z = 19},     -- audience-right (north) source
	playlist = "cabaret",
	empty = "The cabaret has no songs yet: ...",
},
```

`lw_maps` registers the pair with `lw_amps` as a stage. So:

* walking in plays the first song in the `cabaret` playlist from these cabs,
  if nothing else is playing anywhere;
* punching either cab stops it, or plays the first song again, from these
  cabs, even if the amps last pointed at the arena's;
* when the last visitor leaves, the song stops.

While the playlist is empty, walking in plays nothing and punching a cab
says so. The playlist is filled from `mods/lw_amps/cabaret/songs.lua`. Each
song there is a mono Ogg pair in that folder and one `cabaret(...)` line.
[`amps.md`](amps.md#the-cabaret-club) has the exact steps. When a cabaret
song plays, the `cabaret_revue` routine follows its clock: the band and
fans come on with the song, and leave the stage when it stops.

The songs are not join-time media. They are pushed to each visitor with
`core.dynamic_add_media` the first time that visitor comes within earshot of
the cabaret's cabs, so nobody who never visits downloads them.

The map adds no nodes or textures. Going straight from the arena to the
cabaret with `/maps` stops "Around" before the cabaret's song starts: `lw_maps`
stops the songs on stages nobody is on before it starts the one a visitor
has just arrived at.

### Concert arena

Lace Waterfall live: the one map that plays the songs. It is 49 × 13 × 49
(31,213 cells), about twice the Around arena on each side, so the stage, the
floor and the audience each get room. Reach it by walking through the Around
arena and out of its east side (the gate there points north), or with
`/maps concert_arena`.

* **The stage** runs across the north end, 33 nodes wide (local x 8–40),
  a black deck one node up with a violet lip and a stair either side of the
  front, six uplights along the lip, and the backdrop wall behind it. Two
  `lw_amps:cab`s stand on its front edge twenty nodes apart, audience-left to
  the west.
* **The song starts as you walk in.** Arriving — by the spur, by `/maps`, or
  by joining there — points `lw_amps` at the arena's cabs and plays
  `around_happy_hard` if nothing is playing anywhere. `lw_dance` hears the song
  start and puts Rubella and Lillith on the deck between the cabs and the six
  fans on the floor in front (the band itself is [`amps.md`](amps.md)'s).
  Punch a cab to stop it, or to start it again; staying does not undo a stop.
  When the last visitor walks off, a song still coming out of those cabs
  stops. A song already playing from someone else's cabs is left alone, and
  so is one started by hand while nobody is there. `/amp` and
  `/routine start lace_waterfall_around` work here as anywhere.
* **Only here are the song files sent.** They are not game media
  (runtime media in `client/web/media/songs/`, not `sounds/`), so the showcase
  never hashes or announces them; `lw_amps` sends the pair being played to a listener when it starts for
  them. A later lossless or otherwise heavier audio experiment belongs on
  this map only.
* **The floor** is a 35 × 24 checker of black and magenta wool, edged violet,
  with a front-of-house desk (planks, gold trim, a lamp at each end) on its
  centre line. **Risers** run down both sides, two one-node tiers each, so the
  back of the crowd sees over the front.
* **The truss** is the Around arena's, doubled: round the whole edge, seven
  nodes up on columns every eight, 192 cells and 64 chasing lights (eight
  whole chase periods, no seam). A second bar across the room in front of the
  stage carries lit stage lights every fourth cell, the wash on the band.
* **The lasers.** Six emitters on a truss twice as long, a lap every sixteen
  bars (64 beats at 175 bpm), so they move as fast as the Around arena's,
  each throwing a beam to a point circling the floor at radius 8.
* **The banners** are the Around arena's set, spread across a wider wall: the
  "AROUND!" sign over the stage and as the marquee, both portraits, and eleven
  Lace Waterfall banners on the backdrop and the side trusses.

## Getting there

* Walk. A causeway leaves the plaza for every map, each with a gate whose
  posts are topped with something from the map it points at, and a poster that
  says what is over there. The islands sit in one sea, so between them there is
  water rather than a view of the void.
* `/maps` lists the maps; `/maps <id>` travels to one (`/maps halloween_storm`
  for the thunder chapel, `/maps cabaret` for the cabaret club). It emerges the area
  around the spawn first and moves the visitor only once that is done, because
  the islands are far enough out that a visitor moved straight away can arrive
  before the chunk does and fall through the place it will be.

The islands are placed when their chunks are first generated, like the rest of
the showcase, so an island added to the pack appears in worlds created after
it. A world that already generated that stretch of sea keeps it;
`/lw_schem place map_<id>` stamps the committed schematic there anyway.

## Editing them

The maps are generated, not authored in game:

```sh
python3 util/content/generate_luanti_web_maps.py
python3 util/content/generate_luanti_web_maps.py --dump map_halloween   # character map
```

It is deterministic and uses only the standard library, like every other asset
in the pack; rerunning it on an unchanged tree produces an empty diff.

Geometry is why: the maps are cones, ellipsoids and a helical ramp, and doing
that arithmetic once at build time keeps it off the browser's application
worker, where chunk generation competes with the compositor. `.mts` is the
committed form because it keeps `param2` (a theater seat still faces the stage,
a chase controller knows its place in the loop) and compresses all nine maps
to about 17 KiB.

Once they are stamped into a world, `/lw_schem place map_halloween` puts the
committed file back over an existing world, the same as any other piece.

`util/content/test_luanti_web_maps.py` flood-fills each map from its spawn
under a player's movement rules and asserts that every landmark is standable
and connected — the tunnel mouths, the cellar, the attic, the summit, the
inside of the watermelon, the lantern room, the dome's crown, the observation
lip. If a change walls off a route, that test says so. The walker moves across
at its own height with body and head clear, falls at most six nodes, and steps
up one node only with room to jump or onto a stair or slab — so a wall is a
wall, even with a cave on the far side of it.

## Adding a map

A new map is a generator function and a registry row; nothing else in the pack
has to learn about it. In order:

1. **Generator.** Write `build_<name>() -> Build` in
   `util/content/generate_luanti_web_maps.py` and add a `MapSpec` for it to
   `SPECS` with its size and local spawn. Build from `lw_nodes` tiles and the
   `lw_maps` props; a new node goes in `lw_maps`, never in the `lw_palette`
   group (see below). Keep the box at or under 65,536 cells (see Budget); a
   map of a few tens of thousands is the norm.
2. **Schematic.** Run the generator and commit `map_<id>.mts`. Rerunning it on
   an unchanged tree must be an empty diff.
3. **Registry.** One `lw_maps.register` row in `mods/lw_maps/init.lua`, in the
   same order as `SPECS`: `id`, `size`, `origin` (the footprint's minimum
   corner, at `y = BASE`) and `spawn` are all it needs. Atmosphere is optional
   — `day_night_ratio` plus a `sky = lw_maps.sky({...})` built from a handful
   of colours and a fog distance. Moving props go in `entities` with map-local
   positions. Weather is `storm = {rods = {...}}`: rain while someone is on
   the map, and lightning onto those map-local rods (see Rainbow storm), with
   `thunder = <sound>` in it for a roll of thunder after each strike (see
   Thunder chapel). An optional looping soundtrack that is not join-time
   media is `organ = {at, sound}` (see Thunder chapel). A
   stage is `amps = {left, right, song}`, the two sound sources one node above
   its cabs, or `amps = {left, right, playlist}` for a stage whose songs are
   added later (see Cabaret club), and racing lights are `lasers = {track, target, colors, ...}`
   (see Around arena). The schematic defaults to `map_<id>`.
4. **Causeway.** A `maps.causeways()` entry in `mods/lw_world/maps.lua`: a list
   of legs from the plaza's edge to the map's, a gate inside the plaza, a post
   and a poster. The north side of the plaza is the hub tour, so a map that is
   not due west, south or east leaves by a side of the plaza and turns in open
   water. The water, the beach and the rails follow from the legs.
5. **`/maps`.** Nothing to do: it lists every registered map and emerges the
   spawn's neighbourhood before it moves anyone.
6. **Landmarks.** An entry in `LANDMARKS` in `test_luanti_web_maps.py` naming
   every place a visitor has to be able to walk to from the spawn. The test
   refuses a map without one.
7. **Budget.** `python3 util/content/test_luanti_web_maps.py` prints the pack's
   cells and the Lua heap it keeps, checks that every cell reads back through
   the loader's encoding, and fails past the budget.
8. **Docs.** A row in the table above and a walkthrough.

The harness `util/content/test_luanti_web_maps.lua` then checks the registry
(spawn inside the footprint, no overlaps with other islands or the hub) and
walks the causeway: it has to reach the spawn from the plaza without crossing
another island, another causeway, a hub building or the hub tour, and every
deck cell has to be paved with head room once the whole atlas is built.

## Not in this pack

`games/rollercoaster` and `games/log_ride` stay their own games. Each keeps
its `game.conf` and its singlenode world. Track segments and a flume stay
there: this atlas does not stamp them, `lw_world` ships no coaster or flume
schematic, and `lw_maps` registers no track, cart, or flume nodes.

Carts and flowing water are whole-system demos. A plaza island made of either
one would fight the three rules these maps are built to:

* the authored-node budget — about 120,000 cells when this boundary was set;
  Budget below is the cap that applies now (512,000)
* a walk of about three minutes
* no new system on an island

`games/spire_tunnel` and `games/flower_island` stay separate for the same
reason. None of the four is an island in this atlas.

`/maps` travels between islands in this atlas. Entering one of those games is
world switching, and that belongs to the launcher. A later plaza poster may
name them. It offers no `/maps` trip into another world until that switch
exists.

`util/content/test_luanti_web_maps.py` refuses a committed schematic, a
generated map, or an `lw_maps` node from those games.

## Nodes the pack adds

`lw_maps` registers fifty-two nodes. They are deliberately **not** in the
`lw_palette` group — that group builds the material library's pedestals and the
room is full — so they appear in the palette inventory (`i`) but not in the
library. Everything else the maps are built from is a `lw_nodes` tile.

| Node | For |
|------|-----|
| `pumpkin`, `jack_o_lantern` | the lane, the patch, the gate posts |
| `hay` | bales in the patch and the attic |
| `cobweb` | corners, branches, the cellar |
| `bone` | gravestones |
| `dead_wood` | bare trees, lantern posts, the entrance arch |
| `flicker`, `chase` | lamp controllers (not in the inventory) |
| `snow`, `packed_snow`, `slab_packed_snow`, `stair_packed_snow` | the mountain, the trail, the ramps |
| `ice`, `packed_ice` | the ice cave |
| `torch` | cave lighting |
| `leaves`, `pine_leaves` | the garden canopy and the pines |
| `vine` | the patch and the canopy |
| `juice_red`, `juice_green` | the juice channel |
| `lantern`, `lantern_off` | the bazaar's chasing stall lanterns and its lamp posts |
| `basalt`, `packed_basalt` | the caldera's cone, its trail and its tube |
| `lava`, `lava_crust` | the caldera's lava lake and the gate posts that point at it |
| `stage_light`, `stage_light_off` | the arena's chasing truss lights and its gate posts |
| `around_sign_1` … `_5` | "AROUND!", over the arena's stage and its entrance |
| `lace_banner_1` … `_3` | the Lace Waterfall banners on the backdrop and the side trusses |
| `portrait_rubella_1` … `_8`, `portrait_lillith_1` … `_8` | the band's portraits on the backdrop |

The banner cells are cut from one image each by `lw_maps.register_banner`
with `[sheet`, in reading order from the top left, and are not in the
inventory: a single cell of a portrait is not something anyone builds with.

Ten new textures cover all of it: the rest are the existing tiles re-tinted
with `[multiply`, the same trick the sixteen wool cubes use. An eleventh,
`lw_rain.png`, is the rainbow storm's rain and lightning, and no node uses it.
Four more are the arena's banners: `lw_around_sign.png`, `lw_lace_banner.png`
and the two portraits.
They are regenerated by `util/content/generate_luanti_web_textures.py` along with
everything else.

### Lamp controllers

Both controllers are airlike nodes that drive the lamp one node below them,
from a node timer, which only runs while the block is loaded — a map nobody is
on costs nothing. They switch a lamp for its twin from a registered pair
(`lw_maps.register_lamp_pair(lit, unlit)`); the jack-o'-lantern and the
pumpkin are the first pair, the bazaar's lantern, the caldera's lava and the
arena's truss light the others, and a map that wants its own kind of lamp registers two nodes and
reuses both controllers. Neither will grow a lamp where a visitor dug one
out.

* `flicker` gutters on a fixed pattern, phase-offset by position.
* `chase` lights lamps one after another round a loop. A controller's place in
  the loop is its `param2`, which the schematic keeps, and every controller
  reads the same clock, so lamps in different map blocks agree on whose turn it
  is without any wiring between them.

## Budget

`lw_world` keeps each schematic **run-length encoded**, one X row at a time,
in two Lua strings (`lw_world/schems.lua`): three bytes per run (node,
`param2`, length) and three bytes per row to find it. A map is mostly air and
runs of the same block, so it costs its runs, not its box. A string costs its
bytes, where the two arrays this replaced (node and `param2`, one slot per cell,
air included, sixteen bytes a slot, rounded up to a power of two) cost
**1 MiB up to 32,768 cells and 2 MiB up to 65,536**. Measured with the real
loader under Lua 5.1:

| | arrays (before) | run-length (now) |
|---|---|---|
| eleven maps kept for the session | 10.0 MiB | **110 KiB** |
| busiest map (halloween, 25,920 cells) | 1.0 MiB | 17 KiB |
| snow mountain (44,800 cells) | 2.0 MiB | 16 KiB |
| concert arena (31,213 cells) | 1.0 MiB | 9 KiB |
| Around arena (7,500 cells) | 0.25 MiB | 5 KiB |
| load peak, snow mountain | 12.6 MiB | 10.9 MiB |

What is left of the load peak is the engine's: while a map loads,
`read_schematic` hands the loader one small table per cell (about 250 bytes
each), and that exists in full for a moment before the loader packs it and the
collector takes it back. It is now the ceiling on how big one map can be.

`util/content/test_luanti_web_maps.py` measures both, by running `lw_world`'s
own loader over every committed map under Lua 5.1
(`measure_luanti_web_maps_heap.lua`), and first checks that every cell of
every map reads back through the encoding as the node and `param2` it was
saved with (plus a synthetic piece with runs past 255 cells, never-place and
`ignore` cells). It fails when:

* the maps keep more than **512 KiB** between them, or one keeps more than
  **64 KiB** (a map that does not compress, like noise, would);
* loading one map peaks past **16 MiB**;
* one map's box passes **65,536** cells (`MAX_MAP_VOLUME`);
* the pack's total box volume passes **512,000** cells (`NODE_BUDGET`). This
  is a creep guard on how much world has to be generated and meshed, no longer
  a heap figure: the eleven maps are 271,268 cells, room for about eight more
  the size of the concert arena.

The limits used to be set by the box: a 120,000-cell cap before the cost was
measured, then 200,000 to 272,000 cells against an 8 to 10.5 MiB kept-heap cap,
raised one map at a time (the Around arena, thunder chapel, cabaret club and
concert arena each took a MiB or a quarter of one). With run-length storage the
heap stopped being the constraint, so the heap cap was re-based on the
measurement (110 KiB, capped at 512) and the cell cap at 512,000. Raise them on
purpose, with a new measurement in `wasm_porting.md`, rather than squeeze a map
to fit. The schematics themselves are 20.0 KiB on disk and the fifteen textures
under 8 KiB. The sounds are the lighthouse horn (4 KiB) and the chapel's three
thunder rolls (24 KiB). The chapel's organ slot and the cabaret's song slot are
empty and cost nothing until files are dropped into them (see Thunder chapel
and Cabaret club).
See the content budget in `wasm_porting.md`.
