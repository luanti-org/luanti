# Luanti Major Breakages List

This document contains a list of breaking changes to be made in the next major version.
This list is largely advisory and items may be reevaluated once the time comes.

* remove attachment space multiplier (*10)
* remove space multiplier for models (*10)
* remove player gravity multiplier (*2)
* `get_sky()` returns a table (without arg)
* `game.conf` name/id mess
* remove `depends.txt` / `description.txt` (would simplify ContentDB and Luanti code a little)
* rotate moon texture by 180°, making it coherent with the sun
  * https://github.com/luanti-org/luanti/pull/11902
* remove undocumented `set_physics_override(num, num, num)`
* remove special handling of `${key}` syntax in metadata values
* remove old_move
* change physics_override `sneak` to disable the speed change and not just the node clipping
  * https://github.com/luanti-org/luanti/issues/13699
* migrate from player names to UUIDs, this would enable renaming of accounts and unicode player names (if desired)
* harmonize use_texture_alpha between entities & nodes, change default to 'opaque' and remove bool compat code
* merge `sound` and `sounds` table in itemdef
* remove `DIR_DELIM` from Lua
* stop reading initial properties from bare entity def
* change particle default blend mode to `clip`
* remove built-in knockback and related functions entirely
* remove `safe` parameter from `core.serialize`, always enforce `safe = true`.
  possibly error when `loadstring` calls are encountered in `core.deserialize`.
* introduce strict type checking for all instances of `v3s16` / `v3f` read from Lua
