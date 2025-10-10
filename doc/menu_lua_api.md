Luanti Lua Mainmenu API Reference 5.14.0
========================================

Introduction
-------------

The main menu is defined as a formspec by Lua in `builtin/mainmenu/`
Description of formspec language to show your menu is in `lua_api.md`


Images and 3D models
------

Directory delimiters change according to the OS (e.g. on Unix-like systems
is `/`, on Windows is `\`). When putting an image or a 3D model inside a formspec,
be sure to sanitize it first with `core.formspec_escape(img)`; otherwise,
any resource located in a subpath won't be displayed on OSs using `\` as delimiter.


Callbacks
---------

* `core.button_handler(fields)`: called when a button is pressed.
  * `fields` = `{name1 = value1, name2 = value2, ...}`
* `core.event_handler(event)`
  * `event`: `"MenuQuit"` (derived from `quit`) or `"FullscreenChange"`
    The main menu may issue custom events, such as `"Refresh"` (server list).
* `core.on_before_close()`: called before the menu is closed, either to exit or
  to join a game


Gamedata
--------

The "gamedata" table is read when calling `core.start()`. It should contain:

```lua
{
    playername     = <name>,
    password       = <password>,
    address        = <IP/address>,
    port           = <port>,
    selected_world = <index>, -- 0 for client mode
    singleplayer   = <true/false>,
}
```


Functions
---------

* `core.start()`
  * start game session
* `core.close()`
  * exit engine
* `core.get_min_supp_proto()`
  * returns the minimum supported network protocol version
* `core.get_max_supp_proto()`
  * returns the maximum supported network protocol version
* `core.open_url(url)`
  * opens the URL in a web browser, returns false on failure.
  * `url` must begin with http:// or https://
* `core.open_url_dialog(url)`
  * shows a dialog to allow the user to choose whether to open a URL.
  * `url` must begin with http:// or https://
* `core.open_dir(path)`
  * opens the path in the system file browser/explorer, returns false on failure.
  * Must be an existing directory.
* `core.share_file(path)`
  * Android only. Shares file using the share popup
* `core.get_version()` (possible in async calls)
  * returns current core version
* `core.get_formspec_version()`
  * returns maximum supported formspec version



Filesystem
----------

To access specific subpaths, use `DIR_DELIM` as a directory delimiter instead
of manually putting one, as different OSs use different delimiters. E.g.
```lua
"my" .. DIR_DELIM .. "custom" .. DIR_DELIM .. "path" -- and not my/custom/path
```

* `core.get_builtin_path()`
  * returns path to builtin root
* `core.create_dir(absolute_path)` (possible in async calls)
  * `absolute_path` to directory to create (needs to be absolute)
  * returns true/false
* `core.delete_dir(absolute_path)` (possible in async calls)
  * `absolute_path` to directory to delete (needs to be absolute)
  * returns true/false
* `core.copy_dir(source,destination,keep_source)` (possible in async calls)
  * `source` folder
  * `destination` folder
  * `keep_source` DEFAULT true --> if set to false `source` is deleted after copying
  * returns true/false
* `core.is_dir(path)` (possible in async calls)
  * returns true if `path` is a valid dir
* `core.extract_zip(zipfile,destination)` [unzip within path required]
  * `zipfile` to extract
  * `destination` folder to extract to
  * returns true/false
* `core.sound_play(spec, looped)` -> handle
  * `spec` = `SimpleSoundSpec` (see `lua_api.md`)
  * `looped` = bool
* `handle:stop()` or `core.sound_stop(handle)`
* `core.get_mapgen_names([include_hidden=false])` -> table of map generator algorithms
    registered in the core (possible in async calls)
* `core.get_cache_path()` -> path of cache
* `core.get_temp_path([param])` (possible in async calls)
  * `param`=true: returns path to a newly created temporary file
  * otherwise: returns path to a newly created temporary folder


HTTP Requests
-------------

* `core.download_file(url, target)` (possible in async calls)
    * `url` to download, and `target` to store to
    * returns true/false
* `core.get_http_api()` (possible in async calls)
    * returns `HTTPApiTable` containing http functions.
    * The returned table contains the functions `fetch_sync`, `fetch_async` and
      `fetch_async_get` described below.
    * Function only exists if Luanti server was built with cURL support.
* `HTTPApiTable.fetch_sync(HTTPRequest req)`: returns HTTPRequestResult
    * Performs given request synchronously
* `HTTPApiTable.fetch_async(HTTPRequest req)`: returns handle
    * Performs given request asynchronously and returns handle for
      `HTTPApiTable.fetch_async_get`
* `HTTPApiTable.fetch_async_get(handle)`: returns `HTTPRequestResult`
    * Return response data for given asynchronous HTTP request

### `HTTPRequest` definition

Used by `HTTPApiTable.fetch` and `HTTPApiTable.fetch_async`.

```lua
{
    url = "http://example.org",

    timeout = 10,
    -- Timeout for connection in seconds. Default is 3 seconds.

    post_data = "Raw POST request data string" OR {field1 = "data1", field2 = "data2"},
    -- Optional, if specified a POST request with post_data is performed.
    -- Accepts both a string and a table. If a table is specified, encodes
    -- table as x-www-form-urlencoded key-value pairs.
    -- If post_data is not specified, a GET request is performed instead.

    user_agent = "ExampleUserAgent",
    -- Optional, if specified replaces the default Luanti user agent with
    -- given string

    extra_headers = { "Accept-Language: en-us", "Accept-Charset: utf-8" },
    -- Optional, if specified adds additional headers to the HTTP request.
    -- You must make sure that the header strings follow HTTP specification
    -- ("Key: Value").

    multipart = boolean
    -- Optional, if true performs a multipart HTTP request.
    -- Default is false.
}
```

### `HTTPRequestResult` definition

Passed to `HTTPApiTable.fetch` callback. Returned by
`HTTPApiTable.fetch_async_get`.

```lua
{
    completed = true,
    -- If true, the request has finished (either succeeded, failed or timed
    -- out)

    succeeded = true,
    -- If true, the request was successful

    timeout = false,
    -- If true, the request timed out

    code = 200,
    -- HTTP status code

    data = "response"
}
```


Formspec
--------

* `core.update_formspec(formspec)`
* `core.get_table_index(tablename)` -> index
  * can also handle textlists
* `core.formspec_escape(string)` -> string
  * escapes characters [ ] \ , ; that cannot be used in formspecs
* `core.explode_table_event(string)` -> table
  * returns e.g. `{type="CHG", row=1, column=2}`
  * `type`: "INV" (no row selected), "CHG" (selected) or "DCL" (double-click)
* `core.explode_textlist_event(string)` -> table
  * returns e.g. `{type="CHG", index=1}`
  * `type`: "INV" (no row selected), "CHG" (selected) or "DCL" (double-click)
* `core.set_formspec_prepend(formspec)`
  * `formspec`: string to be added to every mainmenu formspec, to be used for theming.


GUI
---

* `core.set_background(type,texturepath,[tile],[minsize])`
  * `type`: "background", "overlay", "header" or "footer"
  * `tile`: tile the image instead of scaling (background only)
  * `minsize`: minimum tile size, images are scaled to at least this size prior
   doing tiling (background only)
* `core.set_clouds(<true/false>)`
* `core.set_topleft_text(text)`
* `core.show_touchscreen_layout()`
* `core.show_path_select_dialog(formname, caption, is_file_select)`
  * shows a path select dialog
  * `formname` is base name of dialog response returned in fields
     - if dialog was accepted `"_accepted"`
        will be added to fieldname containing the path
     - if dialog was canceled `"_cancelled"`
        will be added to fieldname value is set to formname itself
  * if `is_file_select` is `true`, a file and not a folder will be selected
  * returns nil or selected file/folder
* `core.get_active_driver()`:
  * technical name of active video driver, e.g. "opengl"
* `core.get_active_renderer()`:
  * name of current renderer, e.g. "OpenGL 4.6"
* `core.get_active_irrlicht_device()`:
  * name of current irrlicht device, e.g. "SDL"
* `core.get_window_info()`: Same as server-side `get_player_window_information` API.

  ```lua
  -- Note that none of these things are constant, they are likely to change
  -- as the player resizes the window and moves it between monitors
  --
  -- real_gui_scaling and real_hud_scaling can be used instead of DPI.
  -- OSes don't necessarily give the physical DPI, as they may allow user configuration.
  -- real_*_scaling is just OS DPI / 96 but with another level of user configuration.
  {
      -- Current size of the in-game render target.
      --
      -- This is usually the window size, but may be smaller in certain situations,
      -- such as side-by-side mode.
      size = {
          x = 1308,
          y = 577,
      },

      -- Estimated maximum formspec size before Luanti will start shrinking the
      -- formspec to fit. For a fullscreen formspec, use this formspec size and
      -- `padding[0,0]`. `bgcolor[;true]` is also recommended.
      max_formspec_size = {
          x = 20,
          y = 11.25
      },

      -- GUI Scaling multiplier
      -- Equal to the setting `gui_scaling` multiplied by `dpi / 96`
      real_gui_scaling = 1,

      -- HUD Scaling multiplier
      -- Equal to the setting `hud_scaling` multiplied by `dpi / 96`
      real_hud_scaling = 1,

      -- Whether the touchscreen controls are enabled.
      -- Usually (but not always) `true` on Android.
      touch_controls = false,
  }
  ```



Content and Packages
--------------------

Content - an installed mod, modpack, game, or texture pack (txt)
Package - content which is downloadable from the content db, may or may not be installed.

* `core.get_user_path()` (possible in async calls)
    * returns path to global user data,
      the directory that contains user-provided mods, worlds, games, and texture packs.
* `core.get_modpath()` (possible in async calls)
    * returns path to global modpath in the user path, where mods can be installed
* `core.get_modpaths()` (possible in async calls)
    * returns table of virtual path to global modpaths, where mods have been installed
      The difference with `core.get_modpath` is that no mods should be installed in these
      directories by Luanti -- they might be read-only.

      Ex:

      ```lua
      {
          mods = "/home/user/.minetest/mods",
          share = "/usr/share/minetest/mods", -- only provided when RUN_IN_PLACE=0

          -- Custom dirs can be specified by the MINETEST_MOD_DIR env variable
          ["/path/to/custom/dir"] = "/path/to/custom/dir",
      }
      ```

* `core.get_clientmodpath()` (possible in async calls)
    * returns path to global client-side modpath
* `core.get_gamepath()` (possible in async calls)
    * returns path to global gamepath
* `core.get_texturepath()` (possible in async calls)
    * returns path to default textures
* `core.get_games()` -> table of all games (possible in async calls)
    * `name` in return value is deprecated, use `title` instead.
    * returns a table (ipairs) with values:
      ```lua
      {
          id               = <id>,
          path             = <full path to game>,
          gamemods_path    = <path>,
          title            = <title of game>,
          menuicon_path    = <full path to menuicon>,
          author           = "author",
          --DEPRECATED:
          addon_mods_paths = {[1] = <path>,},
      }
      ```
* `core.get_content_info(path)`
    * returns
      ```lua
      {
          name             = "technical_id",
          type             = "mod" or "modpack" or "game" or "txp" or "unknown",
          title            = "Human readable title",
          description      = "description",
          author           = "author",
          path             = "path/to/content",
          textdomain = "textdomain", -- textdomain to translate title / description with
          depends          = {"mod", "names"}, -- mods only
          optional_depends = {"mod", "names"}, -- mods only
      }
      ```
* `core.check_mod_configuration(world_path, mod_paths)`
    * Checks whether configuration is valid.
    * `world_path`: path to the world
    * `mod_paths`: list of enabled mod paths
    * returns:
      ```lua
      {
          is_consistent = true,  -- true is consistent, false otherwise
          unsatisfied_mods = {},  -- list of mod specs
          satisfied_mods = {}, -- list of mod specs
          error_message = "",  -- message or nil
      }
      ```
* `core.get_content_translation(path, domain, string)`
  * Translates `string` using `domain` in content directory at `path`.
  * Textdomains will be found by looking through all locale folders.
  * String should contain translation markup from `core.translate(textdomain, ...)`.
  * Ex: `core.get_content_translation("mods/mymod", "mymod", core.translate("mymod", "Hello World"))`
    will translate "Hello World" into the current user's language
    using `mods/mymod/locale/mymod.fr.tr`.

Logging
-------

* `core.debug(line)` (possible in async calls)
  * Always printed to `stderr` and logfile (`print()` is redirected here)
* `core.log(line)` (possible in async calls)
* `core.log(loglevel, line)` (possible in async calls)
  * `loglevel` one of "error", "action", "info", "verbose"


Settings
--------

* `core.settings:set(name, value)`
* `core.settings:get(name)` -> string or nil (possible in async calls)
* `core.settings:set_bool(name, value)`
* `core.settings:get_bool(name)` -> bool or nil (possible in async calls)
* `core.settings:save()` -> nil, save all settings to config file

For a complete list of methods of the `Settings` object see
[lua_api.md](./lua_api.md)


Worlds
------

* `core.get_worlds()` -> list of worlds (possible in async calls)
  * returns
    ```lua
    {
        [1] = {
            path   = <full path to world>,
            name   = <name of world>,
            gameid = <gameid of world>,
        },
    }
    ```
* `core.create_world(worldname, gameid, init_settings)`
* `core.delete_world(index)`


Helpers
-------

* `core.get_us_time()`
  * returns time with microsecond precision
* `core.gettext(string)` -> string
  * look up the translation of a string in the gettext message catalog
* `fgettext_ne(string, ...)`
  * call `core.gettext(string)`, replace "$1"..."$9" with the given
  extra arguments and return the result
* `fgettext(string, ...)` -> string
  * same as `fgettext_ne()`, but calls `core.formspec_escape` before returning result
* `core.parse_json(string[, nullvalue])` -> something (possible in async calls)
  * see `core.parse_json` (`lua_api.md`)
* `dump(obj, dumped={})`
  * Return object serialized as a string
* `string:split(separator)`
  * eg. `string:split("a,b", ",")` == `{"a","b"}`
* `string:trim()`
  * eg. `string.trim("\n \t\tfoo bar\t ")` == `"foo bar"`
* `core.is_yes(arg)` (possible in async calls)
  * returns whether `arg` can be interpreted as yes
* `core.encode_base64(string)` (possible in async calls)
  * Encodes a string in base64.
* `core.decode_base64(string)` (possible in async calls)
  * Decodes a string encoded in base64.
* `core.urlencode(str)`: Encodes non-unreserved URI characters by a
  percent sign followed by two hex digits. See
  [RFC 3986, section 2.3](https://datatracker.ietf.org/doc/html/rfc3986#section-2.3).


Async
-----

* `core.handle_async(async_job,parameters,finished)`
  * execute a function asynchronously
  * `async_job` is a function receiving one parameter and returning one parameter
  * `parameters` parameter table passed to `async_job`
  * `finished` function to be called once `async_job` has finished
    the result of `async_job` is passed to this function

### Limitations of Async operations
 * No access to global lua variables, don't even try
 * Limited set of available functions
    e.g. No access to functions modifying menu like `core.start`, `core.close`,
    `core.show_path_select_dialog`


Background music
----------------

The main menu supports background music.
It looks for a `main_menu` sound in `$USER_PATH/sounds`. The same naming
conventions as for normal sounds apply.
This means the player can add a custom sound.
It will be played in the main menu (gain = 1.0), looped.
