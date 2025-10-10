Luanti Lua Client Modding API Reference 5.14.0
==============================================

**WARNING**: if you're looking for the `minetest` namespace (e.g. `minetest.something`),
it's now called `core` due to the renaming of Luanti (formerly Minetest).
`minetest` will keep existing as an alias, so that old code won't break.

Note that `core` has already existed since version 0.4.10, so you can use it
safely without breaking backwards compatibility.

* More information at <http://www.luanti.org/>
* Additional documentation: <https://docs.luanti.org/>

Introduction
------------

** WARNING: The client API is currently unstable, and may break/change without warning. **

Content and functionality can be added to Luanti by using Lua
scripting in run-time loaded mods.

A mod is a self-contained bunch of scripts, textures and other related
things that is loaded by and interfaces with Luanti.

Transferring client-sided mods from the server to the client is planned, but not implemented yet.

If you see a deficiency in the API, feel free to attempt to add the
functionality in the engine and API. You can send such improvements as
source code patches on GitHub.

Programming in Lua
------------------
If you have any difficulty in understanding this, please read
[Programming in Lua](http://www.lua.org/pil/).

Startup
-------
Mods are loaded during client startup from the mod load paths by running
the `init.lua` scripts in a shared environment.

In order to load client-side mods, the following conditions need to be satisfied:

1) `$path_user/minetest.conf` contains the setting `enable_client_modding = true`

2) The client-side mod located in `$path_user/clientmods/<modname>` is added to
    `$path_user/clientmods/mods.conf` as `load_mod_<modname> = true`.

Note: Depending on the remote server's settings, client-side mods might not
be loaded or have limited functionality. See setting `csm_restriction_flags` for reference.

Paths
-----
* `RUN_IN_PLACE=1` (Windows release, local build)
    * `$path_user`: `<build directory>`
    * `$path_share`: `<build directory>`
* `RUN_IN_PLACE=0`: (Linux release)
    * `$path_share`:
        * Linux: `/usr/share/minetest`
        * Windows: `<install directory>/minetest-0.4.x`
    * `$path_user`:
        * Linux: `$HOME/.minetest`
        * Windows: `C:/users/<user>/AppData/minetest` (maybe)

Mod load path
-------------
Generic:

* `$path_share/clientmods/`
* `$path_user/clientmods/` (User-installed mods)

In a run-in-place version (e.g. the distributed windows version):

* `minetest/clientmods/` (User-installed mods)

On an installed version on Linux:

* `/usr/share/minetest/clientmods/`
* `$HOME/.minetest/clientmods/` (User-installed mods)

Modpack support
----------------

Mods can be put in a subdirectory, if the parent directory, which otherwise
should be a mod, contains a file named `modpack.conf`.
The file is a key-value store of modpack details.

* `name`: The modpack name.
* `description`: Description of mod to be shown in the Mods tab of the main
                 menu.

Mod directory structure
------------------------

    clientmods
    ├── modname
    │   ├── mod.conf
    │   ├── init.lua
    └── another

### modname

The location of this directory.

### mod.conf

An (optional) settings file that provides meta information about the mod.

* `name`: The mod name. Allows Luanti to determine the mod name even if the
          folder is wrongly named.
* `description`: Description of mod to be shown in the Mods tab of the main
                 menu.
* `depends`: A comma separated list of dependencies. These are mods that must be
             loaded before this mod.
* `optional_depends`: A comma separated list of optional dependencies.
                      Like a dependency, but no error if the mod doesn't exist.

### `init.lua`

The main Lua script. Running this script should register everything it
wants to register. Subsequent execution depends on Luanti calling the
registered callbacks.

**NOTE**: Client mods currently can't provide textures, sounds, or models by
themselves. Any media referenced in function calls must already be loaded
(provided by mods that exist on the server).

Naming convention for registered textual names
----------------------------------------------
Registered names should generally be in this format:

    "modname:<whatever>" (<whatever> can have characters a-zA-Z0-9_)

This is to prevent conflicting names from corrupting maps and is
enforced by the mod loader.

### Example
In the mod `experimental`, there is the ideal item/node/entity name `tnt`.
So the name should be `experimental:tnt`.

Enforcement can be overridden by prefixing the name with `:`. This can
be used for overriding the registrations of some other mod.

Example: Any mod can redefine `experimental:tnt` by using the name

    :experimental:tnt

when registering it.
(also that mod is required to have `experimental` as a dependency)

The `:` prefix can also be used for maintaining backwards compatibility.

Sounds
------
**NOTE: Connecting sounds to objects is not implemented.**

Only Ogg Vorbis files are supported.

For positional playing of sounds, only single-channel (mono) files are
supported. Otherwise OpenAL will play them non-positionally.

Mods should generally prefix their sounds with `modname_`, e.g. given
the mod name "`foomod`", a sound could be called:

    foomod_foosound.ogg

Sounds are referred to by their name with a dot, a single digit and the
file extension stripped out. When a sound is played, the actual sound file
is chosen randomly from the matching sounds.

When playing the sound `foomod_foosound`, the sound is chosen randomly
from the available ones of the following files:

* `foomod_foosound.ogg`
* `foomod_foosound.0.ogg`
* `foomod_foosound.1.ogg`
* (...)
* `foomod_foosound.9.ogg`

Examples of sound parameter tables:

```lua
-- Play locationless
{
    gain = 1.0, -- default
}
-- Play locationless, looped
{
    gain = 1.0, -- default
    loop = true,
}
-- Play in a location
{
    pos = {x = 1, y = 2, z = 3},
    gain = 1.0, -- default
}
```

Looped sounds must be played locationless.

### SimpleSoundSpec
* e.g. `""`
* e.g. `"default_place_node"`
* e.g. `{}`
* e.g. `{name = "default_place_node"}`
* e.g. `{name = "default_place_node", gain = 1.0}`

Representations of simple things
--------------------------------

### Position/vector

```lua
{x=num, y=num, z=num}
```

For helper functions see "Vector helpers".

### pointed_thing
* `{type="nothing"}`
* `{type="node", under=pos, above=pos}`
* `{type="object", id=ObjectID}`

Flag Specifier Format
---------------------

Refer to `lua_api.md`.

Formspec
--------

Formspec defines a menu. It is a string, with a somewhat strange format.

For details, refer to `lua_api.md`.

Spatial Vectors
---------------

Refer to `lua_api.md`.

Helper functions
----------------
* `dump2(obj, name="_", dumped={})`
     * Return object serialized as a string, handles reference loops
* `dump(obj, dumped={})`
    * Return object serialized as a string
* `math.hypot(x, y)`
    * Get the hypotenuse of a triangle with legs x and y.
      Useful for distance calculation.
* `math.sign(x, tolerance)`
    * Get the sign of a number.
      Optional: Also returns `0` when the absolute value is within the tolerance (default: `0`)
* `string.split(str, separator=",", include_empty=false, max_splits=-1, sep_is_pattern=false)`
    * If `max_splits` is negative, do not limit splits.
    * `sep_is_pattern` specifies if separator is a plain string or a pattern (regex).
    * e.g. `string:split("a,b", ",") == {"a","b"}`
* `string:trim()`
    * e.g. `string.trim("\n \t\tfoo bar\t ") == "foo bar"`
* `core.wrap_text(str, limit)`: returns a string
    * Adds new lines to the string to keep it within the specified character limit
    * limit: Maximal amount of characters in one line
* `core.pos_to_string({x=X,y=Y,z=Z}, decimal_places))`: returns string `"(X,Y,Z)"`
    * Convert position to a printable string
      Optional: 'decimal_places' will round the x, y and z of the pos to the given decimal place.
* `core.string_to_pos(string)`: returns a position
    * Same but in reverse. Returns `nil` if the string can't be parsed to a position.
* `core.string_to_area("(X1, Y1, Z1) (X2, Y2, Z2)")`: returns two positions
    * Converts a string representing an area box into two positions
* `core.is_yes(arg)`
    * returns whether `arg` can be interpreted as yes
* `core.is_nan(arg)`
    * returns true when the passed number represents NaN.
* `table.copy(table)`: returns a table
    * returns a deep copy of `table`

'core' namespace reference
--------------------------

### Utilities

* `core.get_current_modname()`: returns the currently loading mod's name, when we are loading a mod
* `core.get_modpath(modname)`: returns virtual path of given mod including
   the trailing separator. This is useful to load additional Lua files
   contained in your mod:
   e.g. `dofile(core.get_modpath(core.get_current_modname()) .. "stuff.lua")`
* `core.get_language()`: returns two strings
   * the current gettext locale
   * the current language code (the same as used for client-side translations)
* `core.get_version()`: returns a table containing components of the
   engine version.  Components:
    * `project`: Name of the project, eg, "Luanti"
    * `string`: Simple version, eg, "1.2.3-dev"
    * `hash`: Full git version (only set if available), eg, "1.2.3-dev-01234567-dirty"
  Use this for informational purposes only. The information in the returned
  table does not represent the capabilities of the engine, nor is it
  reliable or verifiable. Compatible forks will have a different name and
  version entirely. To check for the presence of engine features, test
  whether the functions exported by the wanted features exist. For example:
  `if core.check_for_falling then ... end`.
* `core.sha1(data, [raw])`: returns the sha1 hash of data
    * `data`: string of data to hash
    * `raw`: return raw bytes instead of hex digits, default: false
* `core.colorspec_to_colorstring(colorspec)`: Converts a ColorSpec to a
  ColorString. If the ColorSpec is invalid, returns `nil`.
    * `colorspec`: The ColorSpec to convert
* `core.get_csm_restrictions()`: returns a table of `Flags` indicating the
   restrictions applied to the current mod.
   * If a flag in this table is set to true, the feature is RESTRICTED.
   * Possible flags: `load_client_mods`, `chat_messages`, `read_itemdefs`,
                   `read_nodedefs`, `lookup_nodes`, `read_playerinfo`
* `core.urlencode(str)`: Encodes non-unreserved URI characters by a
  percent sign followed by two hex digits. See
  [RFC 3986, section 2.3](https://datatracker.ietf.org/doc/html/rfc3986#section-2.3).

### Logging
* `core.debug(...)`
    * Equivalent to `core.log(table.concat({...}, "\t"))`
* `core.log([level,] text)`
    * `level` is one of `"none"`, `"error"`, `"warning"`, `"action"`,
      `"info"`, or `"verbose"`.  Default is `"none"`.

### Global callback registration functions
Call these functions only at load time!

* `core.register_globalstep(function(dtime))`
    * Called every client environment step
    * `dtime` is the time since last execution in seconds.
* `core.register_on_mods_loaded(function())`
    * Called just after mods have finished loading.
* `core.register_on_shutdown(function())`
    * Called before client shutdown
    * **Warning**: If the client terminates abnormally (i.e. crashes), the registered
      callbacks **will likely not be run**. Data should be saved at
      semi-frequent intervals as well as on server shutdown.
* `core.register_on_receiving_chat_message(function(message))`
    * Called always when a client receive a message
    * Return `true` to mark the message as handled, which means that it will not be shown to chat
* `core.register_on_sending_chat_message(function(message))`
    * Called always when a client sends a message from chat
    * Return `true` to mark the message as handled, which means that it will not be sent to server
* `core.register_chatcommand(cmd, chatcommand definition)`
    * Adds definition to core.registered_chatcommands
* `core.unregister_chatcommand(name)`
    * Unregisters a chatcommands registered with register_chatcommand.
* `core.register_on_chatcommand(function(command, params))`
    * Called always when a chatcommand is triggered, before `core.registered_chatcommands`
      is checked to see if the command exists, but after the input is parsed.
    * Return `true` to mark the command as handled, which means that the default
      handlers will be prevented.
* `core.register_on_hp_modification(function(hp))`
    * Called when server modified player's HP
* `core.register_on_damage_taken(function(hp))`
    * Called when the local player take damages
* `core.register_on_formspec_input(function(formname, fields))`
    * Called when a button is pressed in the local player's inventory form
    * Newest functions are called first
    * If function returns `true`, remaining functions are not called
* `core.register_on_dignode(function(pos, node))`
    * Called when the local player digs a node
    * Newest functions are called first
    * If any function returns true, the node isn't dug
* `core.register_on_punchnode(function(pos, node))`
    * Called when the local player punches a node
    * Newest functions are called first
    * If any function returns true, the punch is ignored
* `core.register_on_placenode(function(pointed_thing, node))`
    * Called when a node has been placed
* `core.register_on_item_use(function(item, pointed_thing))`
    * Called when the local player uses an item.
    * Newest functions are called first.
    * If any function returns true, the item use is not sent to server.
* `core.register_on_modchannel_message(function(channel_name, sender, message))`
    * Called when an incoming mod channel message is received
    * You must have joined some channels before, and server must acknowledge the
      join request.
    * If message comes from a server mod, `sender` field is an empty string.
* `core.register_on_modchannel_signal(function(channel_name, signal))`
    * Called when a valid incoming mod channel signal is received
    * Signal id permit to react to server mod channel events
    * Possible values are:
      0: join_ok
      1: join_failed
      2: leave_ok
      3: leave_failed
      4: event_on_not_joined_channel
      5: state_changed
* `core.register_on_inventory_open(function(inventory))`
    * Called when the local player open inventory
    * Newest functions are called first
    * If any function returns true, inventory doesn't open
### Sounds
* `core.sound_play(spec, parameters)`: returns a handle
    * `spec` is a `SimpleSoundSpec`
    * `parameters` is a sound parameter table
* `handle:stop()` or `core.sound_stop(handle)`
    * `handle` is a handle returned by `core.sound_play`
* `handle:fade(step, gain)` or `core.sound_fade(handle, step, gain)`
    * `handle` is a handle returned by `core.sound_play`
    * `step` determines how fast a sound will fade.
      Negative step will lower the sound volume, positive step will increase
      the sound volume.
    * `gain` the target gain for the fade.

### Timing
* `core.after(time, func, ...)`
    * Call the function `func` after `time` seconds, may be fractional
    * Optional: Variable number of arguments that are passed to `func`
    * Jobs set for earlier times are executed earlier. If multiple jobs expire
      at exactly the same time, then they expire in the order in which they were
      registered. This basically just applies to jobs registered on the same
      step with the exact same delay.
* `core.get_us_time()`
    * Returns time with microsecond precision. May not return wall time.
* `core.get_timeofday()`
    * Returns the time of day: `0` for midnight, `0.5` for midday

### Map
* `core.get_node_or_nil(pos)`
    * Returns the node at the given position as table in the format
      `{name="node_name", param1=0, param2=0}`, returns `nil`
      for unloaded areas or flavor limited areas.
* `core.get_node_light(pos, timeofday)`
    * Gets the light value at the given position. Note that the light value
      "inside" the node at the given position is returned, so you usually want
      to get the light value of a neighbor.
    * `pos`: The position where to measure the light.
    * `timeofday`: `nil` for current time, `0` for night, `0.5` for day
    * Returns a number between `0` and `15` or `nil`
* `core.find_node_near(pos, radius, nodenames, [search_center])`: returns pos or `nil`
    * `radius`: using a maximum metric
    * `nodenames`: e.g. `{"ignore", "group:tree"}` or `"default:dirt"`
    * `search_center` is an optional boolean (default: `false`)
      If true `pos` is also checked for the nodes
* `core.find_nodes_in_area(pos1, pos2, nodenames, [grouped])`
    * `pos1` and `pos2` are the min and max positions of the area to search.
    * `nodenames`: e.g. `{"ignore", "group:tree"}` or `"default:dirt"`
    * If `grouped` is true the return value is a table indexed by node name
      which contains lists of positions.
    * If `grouped` is false or absent the return values are as follows:
      first value: Table with all node positions
      second value: Table with the count of each node with the node name
      as index
    * Area volume is limited to 4,096,000 nodes
* `core.find_nodes_in_area_under_air(pos1, pos2, nodenames)`: returns a
  list of positions.
    * `nodenames`: e.g. `{"ignore", "group:tree"}` or `"default:dirt"`
    * Return value: Table with all node positions with a node air above
    * Area volume is limited to 4,096,000 nodes
* `core.line_of_sight(pos1, pos2)`: returns `boolean, pos`
    * Checks if there is anything other than air between pos1 and pos2.
    * Returns false if something is blocking the sight.
    * Returns the position of the blocking node when `false`
    * `pos1`: First position
    * `pos2`: Second position
* `core.raycast(pos1, pos2, objects, liquids)`: returns `Raycast`
    * Creates a `Raycast` object.
    * `pos1`: start of the ray
    * `pos2`: end of the ray
    * `objects`: if false, only nodes will be returned. Default is `true`.
    * `liquids`: if false, liquid nodes won't be returned. Default is `false`.

* `core.find_nodes_with_meta(pos1, pos2)`
    * Get a table of positions of nodes that have metadata within a region
      {pos1, pos2}.
* `core.get_meta(pos)`
    * Get a `NodeMetaRef` at that position
* `core.get_node_level(pos)`
    * get level of leveled node (water, snow)
* `core.get_node_max_level(pos)`
    * get max available level for leveled node

### Player
* `core.send_chat_message(message)`
    * Act as if `message` was typed by the player into the terminal.
* `core.run_server_chatcommand(cmd, param)`
    * Alias for `core.send_chat_message("/" .. cmd .. " " .. param)`
* `core.clear_out_chat_queue()`
    * Clears the out chat queue
* `core.localplayer`
    * Reference to the LocalPlayer object. See [`LocalPlayer`](#localplayer) class reference for methods.

### Privileges
* `core.get_privilege_list()`
    * Returns a list of privileges the current player has in the format `{priv1=true,...}`
* `core.string_to_privs(str)`: returns `{priv1=true,...}`
* `core.privs_to_string(privs)`: returns `"priv1,priv2,..."`
    * Convert between two privilege representations

### Client Environment
* `core.get_player_names()`
    * Returns list of player names on server (nil if CSM_RF_READ_PLAYERINFO is enabled by server)
* `core.disconnect()`
    * Disconnect from the server and exit to main menu.
    * Returns `false` if the client is already disconnecting otherwise returns `true`.
* `core.get_server_info()`
    * Returns [server info](#server-info).

### Storage API
* `core.get_mod_storage()`:
    * returns reference to mod private `StorageRef`
    * must be called during mod load time

### Mod channels
![Mod channels communication scheme](docs/mod channels.png)

* `core.mod_channel_join(channel_name)`
    * Client joins channel `channel_name`, and creates it, if necessary. You
      should listen from incoming messages with `core.register_on_modchannel_message`
      call to receive incoming messages. Warning, this function is asynchronous.

### Particles
* `core.add_particle(particle definition)`

* `core.add_particlespawner(particlespawner definition)`
    * Add a `ParticleSpawner`, an object that spawns an amount of particles over `time` seconds
    * Returns an `id`, and -1 if adding didn't succeed

* `core.delete_particlespawner(id)`
    * Delete `ParticleSpawner` with `id` (return value from `core.add_particlespawner`)

### Misc.
* `core.parse_json(string[, nullvalue])`: returns something
    * Convert a string containing JSON data into the Lua equivalent
    * `nullvalue`: returned in place of the JSON null; defaults to `nil`
    * On success returns a table, a string, a number, a boolean or `nullvalue`
    * On failure outputs an error message and returns `nil`
    * Example: `parse_json("[10, {\"a\":false}]")`, returns `{10, {a = false}}`
* `core.write_json(data[, styled])`: returns a string or `nil` and an error message
    * Convert a Lua table into a JSON string
    * styled: Outputs in a human-readable format if this is set, defaults to false
    * Unserializable things like functions and userdata are saved as null.
    * **Warning**: JSON is more strict than the Lua table format.
        1. You can only use strings and positive integers of at least one as keys.
        2. You cannot mix string and integer keys.
           This is due to the fact that JSON has two distinct array and object values.
    * Example: `write_json({10, {a = false}})`, returns `"[10, {\"a\": false}]"`
* `core.serialize(table)`: returns a string
    * Convert a table containing tables, strings, numbers, booleans and `nil`s
      into string form readable by `core.deserialize`
    * Example: `serialize({foo='bar'})`, returns `'return { ["foo"] = "bar" }'`
* `core.deserialize(string)`: returns a table
    * Convert a string returned by `core.deserialize` into a table
    * `string` is loaded in an empty sandbox environment.
    * Will load functions, but they cannot access the global environment.
    * Example: `deserialize('return { ["foo"] = "bar" }')`, returns `{foo='bar'}`
    * Example: `deserialize('print("foo")')`, returns `nil` (function call fails)
        * `error:[string "print("foo")"]:1: attempt to call global 'print' (a nil value)`
* `core.compress(data, method, ...)`: returns `compressed_data`
    * Compress a string of data.
    * `method` is a string identifying the compression method to be used.
    * Supported compression methods:
        * Deflate (zlib): `"deflate"`
        * Zstandard: `"zstd"`
    * `...` indicates method-specific arguments. Currently defined arguments
      are:
        * Deflate: `level` - Compression level, `0`-`9` or `nil`.
        * Zstandard: `level` - Compression level. Integer or `nil`. Default `3`.
        Note any supported Zstandard compression level could be used here,
        but these are subject to change between Zstandard versions.
* `core.decompress(compressed_data, method, ...)`: returns data
    * Decompress a string of data using the algorithm specified by `method`.
    * See documentation on `core.compress()` for supported compression
      methods.
    * `...` indicates method-specific arguments. Currently, no methods use this
* `core.rgba(red, green, blue[, alpha])`: returns a string
    * Each argument is an 8 Bit unsigned integer
    * Returns the ColorString from rgb or rgba values
    * Example: `core.rgba(10, 20, 30, 40)`, returns `"#0A141E28"`
* `core.encode_base64(string)`: returns string encoded in base64
    * Encodes a string in base64.
* `core.decode_base64(string)`: returns string or nil on failure
    * Padding characters are only supported starting at version 5.4.0, where
      5.5.0 and newer perform proper checks.
    * Decodes a string encoded in base64.
* `core.gettext(string)` : returns string
    * look up the translation of a string in the gettext message catalog
* `fgettext_ne(string, ...)`
    * call core.gettext(string), replace "$1"..."$9" with the given
      extra arguments and return the result
* `fgettext(string, ...)` : returns string
    * same as fgettext_ne(), but calls core.formspec_escape before returning result
* `core.pointed_thing_to_face_pos(placer, pointed_thing)`: returns a position
    * returns the exact position on the surface of a pointed node
* `core.global_exists(name)`
    * Checks if a global variable has been set, without triggering a warning.

### UI
* `core.ui.minimap`
    * Reference to the minimap object. See [`Minimap`](#minimap) class reference for methods.
    * If client disabled minimap (using enable_minimap setting) this reference will be nil.
* `core.camera`
    * Reference to the camera object. See [`Camera`](#camera) class reference for methods.
* `core.show_formspec(formname, formspec)` : returns true on success
    * Shows a formspec to the player
* `core.display_chat_message(message)` returns true on success
    * Shows a chat message to the current player.

Setting-related
---------------

* `core.settings`: Settings object containing all of the settings from the
  main config file (`minetest.conf`). Check lua_api.md for class reference.
* `core.setting_get_pos(name)`: Loads a setting from the main settings and
  parses it as a position (in the format `(1,2,3)`). Returns a position or nil.

Class reference
---------------

### ModChannel

An interface to use mod channels on client and server

#### Methods
* `leave()`: leave the mod channel.
    * Client leaves channel `channel_name`.
    * No more incoming or outgoing messages can be sent to this channel from client mods.
    * This invalidate all future object usage
    * Ensure your set mod_channel to nil after that to free Lua resources
* `is_writeable()`: returns true if channel is writable and mod can send over it.
* `send_all(message)`: Send `message` though the mod channel.
    * If mod channel is not writable or invalid, message will be dropped.
    * Message size is limited to 65535 characters by protocol.

### Minimap
An interface to manipulate minimap on client UI

#### Methods
* `show()`: shows the minimap (if not disabled by server)
* `hide()`: hides the minimap
* `set_pos(pos)`: sets the minimap position on screen
* `get_pos()`: returns the minimap current position
* `set_angle(deg)`: sets the minimap angle in degrees
* `get_angle()`: returns the current minimap angle in degrees
* `set_mode(mode)`: sets the minimap mode (0 to 6)
* `get_mode()`: returns the current minimap mode
* `set_shape(shape)`: Sets the minimap shape. (0 = square, 1 = round)
* `get_shape()`: Gets the minimap shape. (0 = square, 1 = round)

### Camera
An interface to get or set information about the camera and camera-node.
Please do not try to access the reference until the camera is initialized, otherwise the reference will be nil.

#### Methods
* `set_camera_mode(mode)`
    * Pass `0` for first-person, `1` for third person, and `2` for third person front
* `get_camera_mode()`
    * Returns 0, 1, or 2 as described above
* `get_fov()`
    * Returns a table with X, Y, maximum and actual FOV in degrees:

        ```lua
        {
            x = number,
            y = number,
            max = number,
            actual = number
        }
        ```

* `get_pos()`
    * Returns position of camera with view bobbing
* `get_offset()`
    * Returns eye offset vector
* `get_look_dir()`
    * Returns eye direction unit vector
* `get_look_vertical()`
    * Returns pitch in radians
* `get_look_horizontal()`
    * Returns yaw in radians
* `get_aspect_ratio()`
    * Returns aspect ratio of screen

### LocalPlayer
An interface to retrieve information about the player.
This object will only be available after the client is initialized. Earlier accesses will yield a `nil` value.

Methods:

* `get_pos()`
    * returns current player current position
* `get_velocity()`
    * returns player speed vector
* `get_hp()`
    * returns player HP
* `get_name()`
    * returns player name
* `get_wield_index()`
    * returns the index of the wielded item
* `get_wielded_item()`
    * returns the itemstack the player is holding
* `is_attached()`
    * returns true if player is attached
* `is_touching_ground()`
    * returns true if player touching ground
* `is_in_liquid()`
    * returns true if player is in a liquid (This oscillates so that the player jumps a bit above the surface)
* `is_in_liquid_stable()`
    * returns true if player is in a stable liquid (This is more stable and defines the maximum speed of the player)
* `get_move_resistance()`
    * returns move resistance of current node, the higher the slower the player moves
* `is_climbing()`
    * returns true if player is climbing
* `swimming_vertical()`
    * returns true if player is swimming in vertical
* `get_physics_override()`
    * returns:

        ```lua
        {
            speed = float,
            speed_climb = float,
            speed_crouch = float,
            speed_fast = float,
            speed_walk = float,
            acceleration_default = float,
            acceleration_air = float,
            acceleration_fast = float,
            jump = float,
            gravity = float,
            liquid_fluidity = float,
            liquid_fluidity_smooth = float,
            liquid_sink = float,
            sneak = boolean,
            sneak_glitch = boolean,
            new_move = boolean,
        }
        ```

* `get_override_pos()`
    * returns override position
* `get_last_pos()`
    * returns last player position before the current client step
* `get_last_velocity()`
    * returns last player speed
* `get_breath()`
    * returns the player's breath
* `get_movement_acceleration()`
    * returns acceleration of the player in different environments
      (note: does not take physics overrides into account):

        ```lua
        {
            fast = float,
            air = float,
            default = float,
        }
        ```

* `get_movement_speed()`
    * returns player's speed in different environments
      (note: does not take physics overrides into account):

        ```lua
        {
            walk = float,
            jump = float,
            crouch = float,
            fast = float,
            climb = float,
        }
        ```

* `get_movement()`
    * returns player's movement in different environments
      (note: does not take physics overrides into account):

        ```lua
        {
            liquid_fluidity = float,
            liquid_sink = float,
            liquid_fluidity_smooth = float,
            gravity = float,
        }
        ```

* `get_last_look_horizontal()`:
    * returns last look horizontal angle
* `get_last_look_vertical()`:
    * returns last look vertical angle
* `get_control()`:
    * returns pressed player controls

        ```lua
        {
            up = boolean,
            down = boolean,
            left = boolean,
            right = boolean,
            jump = boolean,
            aux1 = boolean,
            sneak = boolean,
            zoom = boolean,
            dig = boolean,
            place = boolean,
        }
        ```

* `get_armor_groups()`
    * returns a table with the armor group ratings
* `hud_add(definition)`
    * add a HUD element described by HUD def, returns ID number on success and `nil` on failure.
    * See [`HUD definition`](#hud-definition-hud_add-hud_get)
* `hud_get(id)`
    * returns the [`definition`](#hud-definition-hud_add-hud_get) of the HUD with that ID number or `nil`, if non-existent.
* `hud_get_all()`:
    * Returns a table in the form `{ [id] = HUD definition, [id] = ... }`.
    * A mod should keep track of its introduced IDs and only use this to access foreign elements.
    * It is discouraged to change foreign HUD elements.
* `hud_remove(id)`
    * remove the HUD element of the specified id, returns `true` on success
* `hud_change(id, stat, value)`
    * change a value of a previously added HUD element
    * element `stat` values: `position`, `name`, `scale`, `text`, `number`, `item`, `dir`
    * Returns `true` on success, otherwise returns `nil`

### Settings
An interface to read config files in the format of `minetest.conf`.

It can be created via `Settings(filename)`.

#### Methods
* `get(key)`: returns a value
* `get_bool(key)`: returns a boolean
* `set(key, value)`
* `remove(key)`: returns a boolean (`true` for success)
* `get_names()`: returns `{key1,...}`
* `write()`: returns a boolean (`true` for success)
    * write changes to file
* `to_table()`: returns `{[key1]=value1,...}`

### NodeMetaRef
Node metadata: reference extra data and functionality stored in a node.
Can be obtained via `core.get_meta(pos)`.

#### Methods
* `get_string(name)`
* `get_int(name)`
* `get_float(name)`
* `to_table()`: returns `nil` or a table with keys:
    * `fields`: key-value storage
    * `inventory`: `{list1 = {}, ...}}`

### `Raycast`

A raycast on the map. It works with selection boxes.
Can be used as an iterator in a for loop as:

```lua
local ray = Raycast(...)
for pointed_thing in ray do
    ...
end
```

The map is loaded as the ray advances. If the map is modified after the
`Raycast` is created, the changes may or may not have an effect on the object.

It can be created via `Raycast(pos1, pos2, objects, liquids)` or
`core.raycast(pos1, pos2, objects, liquids)` where:

* `pos1`: start of the ray
* `pos2`: end of the ray
* `objects`: if false, only nodes will be returned. Default is true.
* `liquids`: if false, liquid nodes won't be returned. Default is false.

#### Methods

* `next()`: returns a `pointed_thing` with exact pointing location
    * Returns the next thing pointed by the ray or nil.

-----------------
### Definitions
* `core.get_node_def(nodename)`
    * Returns [node definition](#node-definition) table of `nodename`
* `core.get_item_def(itemstring)`
    * Returns item definition table of `itemstring`

#### Node Definition

```lua
{
    has_on_construct = bool,        -- Whether the node has the on_construct callback defined
    has_on_destruct = bool,         -- Whether the node has the on_destruct callback defined
    has_after_destruct = bool,      -- Whether the node has the after_destruct callback defined
    name = string,                  -- The name of the node e.g. "air", "default:dirt"
    groups = table,                 -- The groups of the node
    paramtype = string,             -- Paramtype of the node
    paramtype2 = string,            -- ParamType2 of the node
    drawtype = string,              -- Drawtype of the node
    mesh = <string>,                -- Mesh name if existent
    minimap_color = <Color>,        -- Color of node on minimap *May not exist*
    visual_scale = number,          -- Visual scale of node
    alpha = number,                 -- Alpha of the node. Only used for liquids
    color = <Color>,                -- Color of node *May not exist*
    palette_name = <string>,        -- Filename of palette *May not exist*
    palette = <{                    -- List of colors
        Color,
        Color
    }>,
    waving = number,                -- 0 of not waving, 1 if waving
    connect_sides = number,         -- Used for connected nodes
    connects_to = {                 -- List of nodes to connect to
        "node1",
        "node2"
    },
    post_effect_color = Color,      -- Color overlaid on the screen when the player is in the node
    leveled = number,               -- Max level for node
    sunlight_propogates = bool,     -- Whether light passes through the block
    light_source = number,          -- Light emitted by the block
    is_ground_content = bool,       -- Whether caves should cut through the node
    walkable = bool,                -- Whether the player collides with the node
    pointable = bool,               -- Whether the player can select the node
    diggable = bool,                -- Whether the player can dig the node
    climbable = bool,               -- Whether the player can climb up the node
    buildable_to = bool,            -- Whether the player can replace the node by placing a node on it
    rightclickable = bool,          -- Whether the player can place nodes pointing at this node
    damage_per_second = number,     -- HP of damage per second when the player is in the node
    liquid_type = <string>,         -- A string containing "none", "flowing", or "source" *May not exist*
    liquid_alternative_flowing = <string>, -- Alternative node for liquid *May not exist*
    liquid_alternative_source = <string>, -- Alternative node for liquid *May not exist*
    liquid_viscosity = <number>,    -- How slow the liquid flows *May not exist*
    liquid_renewable = <boolean>,   -- Whether the liquid makes an infinite source *May not exist*
    liquid_range = <number>,        -- How far the liquid flows *May not exist*
    drowning = bool,                -- Whether the player will drown in the node
    floodable = bool,               -- Whether nodes will be replaced by liquids (flooded)
    node_box = table,               -- Nodebox to draw the node with
    collision_box = table,          -- Nodebox to set the collision area
    selection_box = table,          -- Nodebox to set the area selected by the player
    sounds = {                      -- Table of sounds that the block makes
        sound_footstep = SimpleSoundSpec,
        sound_dig = SimpleSoundSpec,
        sound_dug = SimpleSoundSpec
    },
    legacy_facedir_simple = bool,   -- Whether to use old facedir
    legacy_wallmounted = bool       -- Whether to use old wallmounted
    move_resistance = <number>,     -- How slow players can move through the node *May not exist*
}
```

#### Item Definition

```lua
{
    name = string,                  -- Name of the item e.g. "default:stone"
    description = string,           -- Description of the item e.g. "Stone"
    type = string,                  -- Item type: "none", "node", "craftitem", "tool"
    inventory_image = string,       -- Image in the inventory
    wield_image = string,           -- Image in wieldmesh
    palette_image = string,         -- Image for palette
    color = Color,                  -- Color for item
    wield_scale = Vector,           -- Wieldmesh scale
    stack_max = number,             -- Number of items stackable together
    usable = bool,                  -- Has on_use callback defined
    liquids_pointable = bool,       -- Whether you can point at liquids with the item
    tool_capabilities = <table>,    -- If the item is a tool, tool capabilities of the item
    groups = table,                 -- Groups of the item
    sound_place = SimpleSoundSpec,  -- Sound played when placed
    sound_place_failed = SimpleSoundSpec, -- Sound played when placement failed
    node_placement_prediction = string -- Node placed in client until server catches up
}
```
-----------------

### Chat command definition (`register_chatcommand`)

```lua
{
    params = "<name> <privilege>", -- Short parameter description
    description = "Remove privilege from player", -- Full description
    func = function(param),        -- Called when command is run.
                                   -- Returns boolean success and text output.
}
```

### Server info

```lua
{
    address = "luanti.example.org",   -- The domain name/IP address of a remote server or "" for a local server.
    ip = "203.0.113.156",             -- The IP address of the server.
    port = 30000,                     -- The port the client is connected to.
    protocol_version = 30             -- Will not be accurate at start up as the client might not be connected to the server yet, in that case it will be 0.
}
```

### HUD Definition (`hud_add`, `hud_get`)

Refer to `lua_api.md`.

Escape sequences
----------------
Most text can contain escape sequences that can for example color the text.
There are a few exceptions: tab headers, dropdowns and vertical labels can't.
The following functions provide escape sequences:
* `core.get_color_escape_sequence(color)`:
    * `color` is a [ColorString](#colorstring)
    * The escape sequence sets the text color to `color`
* `core.colorize(color, message)`:
    * Equivalent to:
      `core.get_color_escape_sequence(color) ..
       message ..
       core.get_color_escape_sequence("#ffffff")`
* `core.get_background_escape_sequence(color)`
    * `color` is a [ColorString](#colorstring)
    * The escape sequence sets the background of the whole text element to
      `color`. Only defined for item descriptions and tooltips.
* `core.strip_foreground_colors(str)`
    * Removes foreground colors added by `get_color_escape_sequence`.
* `core.strip_background_colors(str)`
    * Removes background colors added by `get_background_escape_sequence`.
* `core.strip_colors(str)`
    * Removes all color escape sequences.

`ColorString`
-------------

Refer to `lua_api.md`.

`Color`
-------------
`{a = alpha, r = red, g = green, b = blue}` defines an ARGB8 color.

### Particle definition (`add_particle`)

As documented in `lua_api.md`, except for obvious reasons, the `playername` field is not supported.

### `ParticleSpawner` definition (`add_particlespawner`)

As documented in `lua_api.md`, except for obvious reasons, the `playername` field is not supported.
