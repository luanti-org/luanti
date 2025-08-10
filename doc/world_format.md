# Luanti World Format 22...29

This applies to a world format carrying the block serialization version
22...27, used at least in
* `0.4.dev-20120322` ... `0.4.dev-20120606` (22...23)
* `0.4.0` (23)
* 24 was never released as stable and existed for ~2 days
* 27 was added in `0.4.15-dev`
* 29 was added in `5.5.0-dev`

The block serialization version does not fully specify every aspect of this
format; if compliance with this format is to be checked, it needs to be
done by detecting if the files and data indeed follows it.

# Files

Everything is contained in a directory, the name of which is freeform, but
often serves as the name of the world.

Currently, the authentication and ban data is stored on a per-world basis.
It can be copied over from an old world to a newly created world.

    World
    ├── auth.txt ───── Authentication data
    ├── auth.sqlite ── Authentication data (SQLite alternative)
    ├── env_meta.txt ─ Environment metadata
    ├── ipban.txt ──── Banned IPs/users
    ├── map_meta.txt ─ Map metadata
    ├── map.sqlite ─── Map data
    ├── players ────── Player directory
    │   │── player1 ── Player file
    │   └── Foo ────── Player file
    └── world.mt ───── World metadata

## `auth.txt`

Contains authentication data, one player per line.

    <name>:<password hash>:<privilege1,...>

Legacy format (until 0.4.12) of password hash is `<name><password>` SHA1'd,
in base64.

Format (since 0.4.13) of password hash is `#1#<salt>#<verifier>`, with the
parts inside `<>` encoded in base64.

`<verifier>` is an RFC 2945 compatible SRP verifier,
of the given salt, password, and the player's name lowercased,
using the 2048-bit group specified in RFC 5054 and the SHA-256 hash function.

Example lines:
* Player "celeron55", no password, privileges "interact" and "shout":
    ```
    celeron55::interact,shout
    ```
* Player "Foo", password "bar", privilege "shout", with a legacy password hash:
    ```
    foo:iEPX+SQWIR3p67lj/0zigSWTKHg:shout
    ```
* Player "Foo", password "bar", privilege "shout", with a 0.4.13 password hash:
    ```
    foo:#1#hPpy4O3IAn1hsNK00A6wNw#Kpu6rj7McsrPCt4euTb5RA5ltF7wdcWGoYMcRngwDi11cZhPuuR9i5Bo7o6A877TgcEwoc//HNrj9EjR/CGjdyTFmNhiermZOADvd8eu32FYK1kf7RMC0rXWxCenYuOQCG4WF9mMGiyTPxC63VAjAMuc1nCZzmy6D9zt0SIKxOmteI75pAEAIee2hx4OkSXRIiU4Zrxo1Xf7QFxkMY4x77vgaPcvfmuzom0y/fU1EdSnZeopGPvzMpFx80ODFx1P34R52nmVl0W8h4GNo0k8ZiWtRCdrJxs8xIg7z5P1h3Th/BJ0lwexpdK8sQZWng8xaO5ElthNuhO8UQx1l6FgEA:shout
    ```
* Player "bar", no password, no privileges:
    ```
    bar::
    ```

## `auth.sqlite`

Contains authentication data as an SQLite database. This replaces auth.txt
above when `auth_backend` is set to `sqlite3` in world.mt.

This database contains two tables: `auth` and `user_privileges`:

```sql
CREATE TABLE `auth` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT,
    `name` VARCHAR(32) UNIQUE,
    `password` VARCHAR(512),
    `last_login` INTEGER
);
CREATE TABLE `user_privileges` (
    `id` INTEGER,
    `privilege` VARCHAR(32),
    PRIMARY KEY (id, privilege),
    CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES auth (id) ON DELETE CASCADE
);
```

The `name` and `password` fields of the auth table are the same as the auth.txt
fields (with modern password hash). The `last_login` field is the last login
time as a Unix time stamp.

The `user_privileges` table contains one entry per privilege and player.
A player with "interact" and "shout" privileges will have two entries, one
with `privilege="interact"` and the second with `privilege="shout"`.

## `env_meta.txt`

Simple global environment variables.

Example content:

    game_time = 73471
    time_of_day = 19118
    EnvArgsEnd

## `ipban.txt`

Banned IP addresses and usernames.

Example content:

    123.456.78.9|foo
    123.456.78.10|bar

## `map_meta.txt`

Simple global map variables.

Example content:

    seed = 7980462765762429666
    [end_of_params]

## `map.sqlite`

Map data.

See [Map File Format](#map-file-format) below.

## `player1`, `Foo`

Player data.

Filename can be anything.

See [Player File Format](#player-file-format) below.

## `world.mt`

World metadata.

    gameid = mesetint             - name of the game
    enable_damage = true          - whether damage is enabled or not
    creative_mode = false         - whether creative mode is enabled or not
    backend = sqlite3             - which DB backend to use for blocks (sqlite3, dummy, leveldb, redis, postgresql)
    player_backend = sqlite3      - which DB backend to use for player data
    readonly_backend = sqlite3    - optionally read-only seed DB (DB file _must_ be located in "readonly" subfolder)
    auth_backend = files          - which DB backend to use for authentication data
    mod_storage_backend = sqlite3 - which DB backend to use for mod storage
    server_announce = false       - whether the server is publicly announced or not
    load_mod_<mod> = false        - whether <mod> is to be loaded in this world

For `load_mod_<mod>`, the possible values are:

* `false` - Do not load the mod.
* `true` - Load the mod from wherever it is found (may cause conflicts if the same mod appears also in some other place).
* `mods/modpack/moddir` - Relative path to the mod
    * Must be one of the following:
        * `mods/`: mods in the user path's mods folder (ex. `/home/user/.minetest/mods`)
        * `share/`: mods in the share's mods folder (ex. `/usr/share/minetest/mods`)
        * `/path/to/env`: you can use absolute paths to mods inside folders specified with the `MINETEST_MOD_PATH` `env` variable.
    * Other locations and absolute paths are not supported.
    * Note that `moddir` is the directory name, not the mod name specified in mod.conf.

`PostgreSQL` backend specific settings:

    pgsql_connection = host=127.0.0.1 port=5432 user=mt_user password=mt_password dbname=minetest
    pgsql_player_connection = (same parameters as above)
    pgsql_readonly_connection = (same parameters as above)
    pgsql_auth_connection = (same parameters as above)
    pgsql_mod_storage_connection = (same parameters as above)

`Redis` backend specific settings:

    redis_address = 127.0.0.1  - Redis server address
    redis_hash = foo           - Database hash
    redis_port = 6379          - (optional) Connection port
    redis_password = hunter2   - (optional) Server password

# Player File Format

Should be pretty self-explanatory.
> **Note**: Position is in `nodes * 10`

Example content:

    hp = 11
    name = celeron55
    pitch = 39.77
    position = (-5231.97,15,1961.41)
    version = 1
    yaw = 101.37
    PlayerArgsEnd
    List main 32
    Item default:torch 13
    Item default:pick_steel 1 50112
    Item experimental:tnt
    Item default:cobble 99
    Item default:pick_stone 1 13104
    Item default:shovel_steel 1 51838
    Item default:dirt 61
    Item default:rail 78
    Item default:coal_lump 3
    Item default:cobble 99
    Item default:leaves 22
    Item default:gravel 52
    Item default:axe_steel 1 2045
    Item default:cobble 98
    Item default:sand 61
    Item default:water_source 94
    Item default:glass 2
    Item default:mossycobble
    Item default:pick_steel 1 64428
    Item animalmaterials:bone
    Item default:sword_steel
    Item default:sapling
    Item default:sword_stone 1 10647
    Item default:dirt 99
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    EndInventoryList
    List craft 9
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    EndInventoryList
    List craftpreview 1
    Empty
    EndInventoryList
    List craftresult 1
    Empty
    EndInventoryList
    EndInventory

# Map File Format

Luanti maps consist of `MapBlock`s, chunks of 16x16x16 nodes.

In addition to the bulk node data, `MapBlock`s stored on disk also contain
other things.

## History

Initially, Luanti stored maps in a format called the "sectors" format.
It was a directory/file structure like this:

    sectors2/XXX/ZZZ/YYYY

For example, the `MapBlock` at `(0, 1, -2)` was this file:

    sectors2/000/ffd/0001

Eventually Luanti outgrew this directory structure, as filesystems were
struggling under the number of files and directories.

Large servers seriously needed a new format, and thus the base of the
current format was invented, suggested by celeron55 and implemented by
JacobF.

`SQLite3` was implemented. Blocks files were directly inserted as blobs
in a single table, indexed by integer primary keys, which were hashed
coordinates.

Today, we know that `SQLite3` allows multiple primary keys (which would allow
storing coordinates separately), but the format has been kept unchanged for
that part.

## `map.sqlite`
`map.sqlite` is an `SQLite3` database, containing a single table, called
`blocks`. It looks like this:

```sql
CREATE TABLE `blocks` (
    `x` INTEGER, `y` INTEGER, `z` INTEGER,
    `data` BLOB NOT NULL,
    PRIMARY KEY (`x`, `z`, `y`)
);
```

Before 5.12.0 it looked like this:

```sql
CREATE TABLE `blocks` (`pos` INT NOT NULL PRIMARY KEY, `data` BLOB);
```

## Position Encoding

Applies to the pre-5.12.0 schema:

`pos` (a node position encoding) is created from the three coordinates of a
`MapBlock` using the following simple equation:

```C
pos = (z << 24) + (y << 12) + x;
```
or, equivalently, `pos = (z * 0x1000000) + (y * 0x1000) + x`.

A position can be decoded using:

```C
pos = pos + 0x800800800;
x = (pos & 0xFFF) - 0x800;
y = ((pos >> 12) & 0xFFF) - 0x800;
z = ((pos >> 24) & 0xFFF) - 0x800;
```

Positions are sequential along the x axis (as easily seen from the position equation above).
It is possible to retrieve all blocks from an interval using the following SQL statement:

```sql
SELECT
`pos`,
`data`,
( (`pos` + 0x800800800)        & 0xFFF) - 0x800 as x,
(((`pos` + 0x800800800) >> 12) & 0xFFF) - 0x800 as y,
(((`pos` + 0x800800800) >> 24) & 0xFFF) - 0x800 as z
FROM `blocks` WHERE
( (`pos` + 0x800800800)        & 0xFFF) - 0x800 >= ? AND -- minx
( (`pos` + 0x800800800)        & 0xFFF) - 0x800 <= ? AND -- maxx
(((`pos` + 0x800800800) >> 12) & 0xFFF) - 0x800 >= ? AND -- miny
(((`pos` + 0x800800800) >> 12) & 0xFFF) - 0x800 <= ? AND -- maxy
`pos` >= (? << 24) - 0x800800 AND -- minz
`pos` <= (? << 24) + 0x7FF7FF; -- maxz
```

## Blob

The blob is the data that would have otherwise gone into the file.

See below for description.

# MapBlock Serialization Format

> **Notes**:
>  * NOTE: Byte order is MSB first (big-endian).
>  * NOTE: Zlib data is in such a format that Python's `zlib` at least can
>          directly decompress.
>  * NOTE: Since version 29 zstd is used instead of zlib. In addition, the
>          **entire block** is first serialized and then compressed (except version byte).

`u8` version
* map format version number, see serialization.h for the latest number

`u8` flags
* Flag bitmasks:
    * `0x01`: `is_underground`: Should be set to 0 if there will be no light
      obstructions above the block. If/when sunlight of a block is updated
      and there is no block above it, this value is checked for determining
      whether sunlight comes from the top.

    * `0x02`: `day_night_differs`: Whether the lighting of the block is different
      on day and night. Only blocks that have this bit set are updated when
      day transforms to night.

    * `0x04`: `lighting_expired`: Not used in version 27 and above. If true,
      lighting is invalid and should be updated. If you can't calculate
      lighting in your generator properly, you could try setting this 1 to
      everything and setting the uppermost block in every sector as
      `is_underground=0`. It most likely won't work properly.

    * `0x08`: `generated`: True if the block has been generated. If false, block
      is mostly filled with `CONTENT_IGNORE` and is likely to contain e.g. parts
      of trees of neighboring blocks.

`u16` lighting_complete
* Added in version 27.

* This contains 12 flags, each of them corresponds to a direction.

* Indicates if the light is correct at the sides of a map block.
  Lighting may not be correct if the light changed, but a neighbor
  block was not loaded at that time.
  If these flags are false, Luanti will automatically recompute light
  when both this block and its required neighbor are loaded.

* The bit order is:
  nothing,  nothing,  nothing,  nothing,
  night X-, night Y-, night Z-, night Z+, night Y+, night X+,
  day X-,   day Y-,   day Z-,   day Z+,   day Y+,   day X+.
  Where 'day' is for the day light bank, 'night' is for the night
  light bank.
  The 'nothing' bits should be always set, as they will be used
  to indicate if direct sunlight spreading is finished.

* Example: if the block at `(0, 0, 0)` has `lighting_complete = 0b1111111111111110`,
  Luanti will correct lighting in the day light bank when the block at
  `(1, 0, 0)` is also loaded.

Timestamp and node ID mappings come here if map format version >= 29.
* `u32` timestamp
    * Timestamp when last saved, as seconds from starting the game.
    * `0xffffffff` = invalid/unknown timestamp, nothing should be done with the time
                     difference when loaded

* `u8` `name_id_mapping_version`
    * Should be zero for map format version 29.

* `u16` `num_name_id_mappings`
    * foreach `num_name_id_mappings`:
        * `u16` `id`
        * `u16` `name_len`
        * `u8[name_len]` `name`

`u8` content_width
* Number of bytes in the content (`param0`) fields of nodes
* 1 byte before map format version 24, 2 bytes since

`u8` params_width
* Number of bytes used for parameters per node
* Always 2

## Node Data
> **Note**: Zlib-compressed before map format version 29

* If `content_width` is 1:
    * `u8[4096]`: `param0` fields
    * `u8[4096]`: `param1` fields
    * `u8[4096]`: `param2` fields

* If `content_width` is 2:
    * `u16[4096]`: `param0` fields
    * `u8[4096]`: `param1` fields
    * `u8[4096]`: `param2` fields

* The location of a node in each of those arrays is `(z*16*16 + y*16 + x)`.

### Node Metadata List
> **Note**: Zlib-compressed before map version format 29
* Before map format version 23:
    * `u16` version (=1)
    * `u16` count of metadata
    * foreach count:
        * `u16` position (`(p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X)`)
        * `u16` type_id
        * `u16` content_size
        * `u8[content_size]` content of metadata. Format depends on `type_id`, see below.

* Since map format version 23:
    * `u8` version
        > **Note**: Type was `u16` before map format version 23
        * = 1 before map format version 28
        * = 2 since map format version 28
    * `u16` count of metadata
    * foreach count:
        * `u16` `position` (`(p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X)`)
        * `u32` `num_vars`
        * foreach `num_vars`:
            * `u16` `key_len`
            * `u8[key_len]` `key`
            * `u32 val_len`
            * `u8[val_len]` `value`
            * `u8` `is_private`
            * only since map format version 2. 0 = not private, 1 = private
        * serialized inventory

## Node Timers
* Map format version 23:
    * `u8` unused version (always 0)

* Map format version 24:
    > **Note**: Not released as stable
    * `u8` `nodetimer_version`
    * if `nodetimer_version` == 1:
        * `u16` `num_of_timers`
        * foreach `num_of_timers`:
            * `u16` timer position (`(z*16*16 + y*16 + x)`)
            * `s32` timeout * 1000
            * `s32` elapsed * 1000

* Map format version >= 25: see below

`u8` static object version:
* Always 0

`u16` `static_object_count`

foreach `static_object_count`:
* `u8` type (object type-id)
* `s32` `pos_x_nodes` * 10000
* `s32` `pos_y_nodes` * 10000
* `s32` `pos_z_nodes` * 10000
* `u16` `data_size`
* `u8[data_size]` `data`

Before map format version 29:
* `u32` `timestamp`
    * Same meaning as the timestamp further up

* `u8` `name-id-mapping` version
    * Always 0

* `u16` `num_name_id_mappings`
    * foreach `num_name_id_mappings`:
        * `u16` `id`
        * `u16` `name_len`
        * `u8[name_len]` `name`

Since map format version 25, node timers come here:
    * `u8` length of the data of a single timer (always 2+4+4=10)
    * `u16` `num_of_timers`
    * foreach `num_of_timers`:
        * `u16` timer position (`(z*16*16 + y*16 + x)`)
        * `s32` timeout * 1000
        * `s32` elapsed * 1000

End of File (EOF).

# Format of Nodes

A node is composed of the `u8` fields `param0`, `param1` and `param2`.

Before map format version 24:
* The content id of a node is determined as so:
    * If `param0` < `0x80`, `content_id = param0`
    * Otherwise, `content_id = (param0<<4) + (param2>>4)`

Since map format version 24:
* The content id of a node is `param0`.

The purpose of `param1` and `param2` depend on the definition of the node.

# Name-ID-Mapping

The mapping maps node content IDs to node names.

# Node Metadata Format (Before Map Format Version 23)

The node metadata is serialized depending on the `type_id` field.

`1`: Generic metadata
* serialized inventory
* `u32` `len`
* `u8[len]` `text`
* `u16` `len`
* `u8[len]` `owner`
* `u16` `len`
* `u8[len]` `infotext`
* `u16` `len`
* `u8[len]` inventory drawspec
* `u8` `allow_text_input` (bool)
* `u8` `removal_disabled` (bool)
* `u8` `enforce_owner` (bool)
* `u32` `num_vars`
* foreach `num_vars`:
    * `u16` `len`
    * `u8[len]` `name`
    * `u32` `len`
    * `u8[len]` `value`

`14`: Sign metadata
* `u16` `text_len`
* `u8[text_len]` `text`

`15`: Chest metadata
* serialized inventory

`16`: Furnace metadata
* To be determined

`17`: Locked Chest metadata
* `u16` `len`
* `u8[len]` `owner`
* serialized inventory

# Static Objects

Static objects are persistent freely moving objects in the world.

Object types:
`1`: Test object
`7`: LuaEntity:
* `u8` `compatibility_byte` (always 1)
* `u16` `len`
* `u8[len]` entity name
* `u32` `len`
* `u8[len]` static data
* `s16` `hp`
* `s32` velocity.x * 10000
* `s32` velocity.y * 10000
* `s32` velocity.z * 10000
* `s32` yaw * 1000

Since protocol version 37:
* `u8` `version2` (=1 or 2)
* `s32` pitch * 1000
* `s32` roll * 1000
* if version2 >= 2:
  * `u8[16]` guid

# Itemstring Format

Examples:
* `'default:dirt 5'`
* `'default:pick_wood 21323'`
* `'"default:apple" 2'`
* `'default:apple'`

Older formats:
* `'node "default:dirt" 5'`
* `'NodeItem default:dirt 5'`
* `'ToolItem WPick 21323'`

The wear value in tools is 0...65535.

# Inventory Serialization Format

* The inventory serialization format is line-based.
* The newline character used is `\n`
* The end condition of a serialized inventory is always `EndInventory\n`
* All the slots in a list must always be serialized.

## Example

    List foo 4
    Item default:sapling
    Item default:sword_stone 1 10647
    Item default:dirt 99
    Empty
    EndInventoryList
    List bar 9
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    Empty
    EndInventoryList
    EndInventory
