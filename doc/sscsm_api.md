# Server-sent client-side modding (SSCSM) API reference

**Warning:** SSCSM is very experimental. The API will break. Always start your
mod with a version check (i.e. at least check if `core.get_version().proto_max`
is (less or) equal to (any of) the tested version(s)).

In SSCSM, the server sends scripts to the client, which it executes
client-side (in a sandbox, see also `sscsm_security.md`).
As modder, you can add these scripts to your server-side mod, and tell the engine
to send them.

Please refer to `lua_api.md` for server-side modding.
(And refer to `client_lua_api.md` for client-provided client-side modding (CPCSM).)



## Loading mods

### Paths

SSCSM uses a virtual file system (just a dictionary of virtual paths (strings)
to file contents (strings)).

Each mod's files have paths of the form `modname:foo/bla.lua`.
Please don't rely on this, use `core.get_modpath()` instead.

The virtual file paths within a mod are meant to mimic the filepaths on the
server, for example `<modpath>/common/foo.lua` gets sent as `modname:common/foo.lua`.

The engine loads `modname:init.lua` for all mods, in server mod dependency order.

There is client and server builtin (modnames are `*client_builtin*` and
`*server_builtin*`). The server builtin is sent from the server, like any other
SSCSM, and the client builtin is located on the client.


### Mod sending API

Currently, you can not add any mods. There's only a small hardcoded preview script
in C++ which is loaded when you set `enable_sscsm` to `singleplayer`.



## API

Unless noted otherwise, these work the same as in the server modding API.

Functions that take or return paths always use virtual paths.

### Global callbacks

* `core.register_globalstep(function(dtime))`


### SSCSM-specific API

* `core.get_node_or_nil(pos)`
* `core.get_content_id(name)`
* `core.get_name_from_content_id(id)`


### Util API

* `core.log([level,] text)`
* `core.get_us_time()`
  * Limited in precision.
* `core.parse_json(str[, nullvalue])`
* `core.write_json(data[, styled])`
* `core.is_yes(arg)`
* `core.compress(data, method, ...)`
* `core.decompress(data, method, ...)`
* `core.encode_base64(string)`
* `core.decode_base64(string)`
* `core.get_version()`
* `core.sha1(string, raw)`
* `core.sha256(string, raw)`
* `core.colorspec_to_colorstring(colorspec)`
* `core.colorspec_to_bytes(colorspec)`
* `core.colorspec_to_table(colorspec)`
* `core.time_to_day_night_ratio(time_of_day)`
* `core.get_last_run_mod()`
* `core.set_last_run_mod(modname)`
* `core.urlencode(value)`


### Other

* `core.get_current_modname()`
* `core.get_modpath(modname)`


### Builtin helpers

* `math.*` additions

* `vector.*`

* `core.global_exists(name)`

* `core.serialize(value)`
* `core.deserialize(str, safe)`

* `dump2(obj, name, dumped)`
* `dump(obj, dumped)`
* `string.*` additions
* `table.*` additions
* `core.formspec_escape(text)`
* `core.hypertext_escape(text)`
* `core.wrap_text(str, limit, as_table)`
* `core.explode_table_event(evt)`
* `core.explode_textlist_event(evt)`
* `core.explode_scrollbar_event(evt)`
* `core.rgba(r, g, b, a)`
* `core.pos_to_string(pos, decimal_places)`
* `core.string_to_pos(value)`
* `core.string_to_area(value, relative_to)`
* `core.get_color_escape_sequence(color)`
* `core.get_background_escape_sequence(color)`
* `core.colorize(color, message)`
* `core.strip_foreground_colors(str)`
* `core.strip_background_colors(str)`
* `core.strip_colors(str)`
* `core.translate(textdomain, str, ...)`
* `core.translate_n(textdomain, str, str_plural, n, ...)`
* `core.get_translator(textdomain)`
* `core.pointed_thing_to_face_pos(placer, pointed_thing)`
* `core.string_to_privs(str, delim)`
* `core.privs_to_string(privs, delim)`
* `core.is_nan(number)`
* `core.parse_relative_number(arg, relative_to)`
* `core.parse_coordinates(x, y, z, relative_to)`

* `core.inventorycube(img1, img2, img3)`
* `core.dir_to_facedir(dir, is6d)`
* `core.facedir_to_dir(facedir)`
* `core.dir_to_fourdir(dir)`
* `core.fourdir_to_dir(fourdir)`
* `core.dir_to_wallmounted(dir)`
* `core.wallmounted_to_dir(wallmounted)`
* `core.dir_to_yaw(dir)`
* `core.yaw_to_dir(yaw)`
* `core.is_colored_paramtype(ptype)`
* `core.strip_param2_color(param2, paramtype2)`

* `core.after(time, func, ...)`


### Lua standard library

* `assert`
* `collectgarbage`
* `error`
* `getfenv`
* `ipairs`
* `next`
* `pairs`
* `pcall`
* `print`
* `rawequal`
* `rawget`
* `rawset`
* `select`
* `setfenv`
* `getmetatable`
* `setmetatable`
* `tonumber`
* `tostring`
* `type`
* `unpack`
* `_VERSION`
* `xpcall`
* `dofile`
  * Overwritten: Loading bytecode is prohibited (like in SSM).
* `load`
  * As above.
* `loadfile`
  * As above.
* `loadstring`
  * As above.
* `coroutine.*`
* `table.*`
* `math.*`
* `string.*`
  * except `string.dump`
* `os.difftime`
* `os.time`
* `os.clock`
  * Reduced precision.
* `debug.traceback`


### LuaJIT `jit` library

* `jit.arch`
* `jit.flush`
* `jit.off`
* `jit.on`
* `jit.opt`
* `jit.os`
* `jit.status`
* `jit.version`
* `jit.version_num`


### Bit library

* `bit.*`


### API only for client builtin

* `core.get_builtin_path()`
  * Returns path, depending on which builtin currently loads, or `nil`.
* `debug.getinfo(...)`
* `INIT`
  * Is `"sscsm"`.
