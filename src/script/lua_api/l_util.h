// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "lua_api/l_base.h"


class ModApiUtil : public ModApiBase
{
private:
	/*
		NOTE:
		The functions in this module are available in the in-game API
		as well as in the mainmenu API.

		All functions that don't require either a Server or
		GUIEngine instance should be in here.
	*/

	// log([level,] text)
	// Writes a line to the logger.
	// The one-argument version logs to LL_NONE.
	// The two-argument version accepts a log level.
	static int l_log(lua_State *L);

	// get us precision time
	static int l_get_us_time(lua_State *L);

	// parse_json(str[, nullvalue])
	static int l_parse_json(lua_State *L);

	// write_json(data[, styled])
	static int l_write_json(lua_State *L);

	// get_tool_wear_after_use(uses[, initial_wear])
	static int l_get_tool_wear_after_use(lua_State *L);

	// get_dig_params(groups, tool_capabilities[, wear])
	static int l_get_dig_params(lua_State *L);

	// get_hit_params(groups, tool_capabilities[, time_from_last_punch[, wear]])
	static int l_get_hit_params(lua_State *L);

	// check_password_entry(name, entry, password)
	static int l_check_password_entry(lua_State *L);

	// get_password_hash(name, raw_password)
	static int l_get_password_hash(lua_State *L);

	// is_yes(arg)
	static int l_is_yes(lua_State *L);

	// get_builtin_path()
	static int l_get_builtin_path(lua_State *L);

	// get_user_path()
	static int l_get_user_path(lua_State *L);

	// compress(data, method, ...)
	static int l_compress(lua_State *L);

	// decompress(data, method, ...)
	static int l_decompress(lua_State *L);

	// mkdir(path)
	static int l_mkdir(lua_State *L);

	// rmdir(path, recursive)
	static int l_rmdir(lua_State *L);

	// cpdir(source, destination, remove_source)
	static int l_cpdir(lua_State *L);

	// mvdir(source, destination)
	static int l_mvdir(lua_State *L);

	// get_dir_list(path, is_dir)
	static int l_get_dir_list(lua_State *L);

	// safe_file_write(path, content)
	static int l_safe_file_write(lua_State *L);

	// request_insecure_environment()
	static int l_request_insecure_environment(lua_State *L);

	// encode_base64(string)
	static int l_encode_base64(lua_State *L);

	// decode_base64(string)
	static int l_decode_base64(lua_State *L);

	// get_version()
	static int l_get_version(lua_State *L);

	// sha1(string, raw)
	static int l_sha1(lua_State *L);

	// sha256(string, raw)
	static int l_sha256(lua_State *L);

	// colorspec_to_colorstring(colorspec)
	static int l_colorspec_to_colorstring(lua_State *L);

	// colorspec_to_bytes(colorspec)
	static int l_colorspec_to_bytes(lua_State *L);

	// colorspec_to_table(colorspec)
	static int l_colorspec_to_table(lua_State *L);

	// time_to_day_night_ratio(time_of_day)
	static int l_time_to_day_night_ratio(lua_State *L);

	// encode_png(w, h, data, level)
	static int l_encode_png(lua_State *L);

	// get_last_run_mod()
	static int l_get_last_run_mod(lua_State *L);

	// set_last_run_mod(modname)
	static int l_set_last_run_mod(lua_State *L);

	// urlencode(value)
	static int l_urlencode(lua_State *L);

	// is_valid_player_name(name)
	static int l_is_valid_player_name(lua_State *L);

	// strip_escapes(str)
	static int l_strip_escapes(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
	static void InitializeAsync(lua_State *L, int top);
	static void InitializeClient(lua_State *L, int top);
};
