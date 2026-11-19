// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_util.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_settings.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "network/networkprotocol.h"
#include "serialization.h"
#include <json/json.h>
#include <zstd.h>
#include "cpp_api/s_security.h"
#include "porting.h"
#include "convert_json.h"
#include "log.h"
#include "log_internal.h"
#include "tool.h"
#include "filesys.h"
#include "settings.h"
#include "util/auth.h"
#include "util/base64.h"
#include "config.h"
#include "version.h"
#include "util/hex.h"
#include "util/hashing.h"
#include "util/png.h"
#include "player.h"
#include "daynightratio.h"
#include "constants.h"
#include "porting_emscripten.h"
#include <cstdio>

// only available in zstd 1.3.5+
#ifndef ZSTD_CLEVEL_DEFAULT
#define ZSTD_CLEVEL_DEFAULT 3
#endif

// log([level,] text)
// Writes a line to the logger.
// The one-argument version logs to LL_NONE.
// The two-argument version accepts a log level.
// Either the special case "deprecated" for deprecation notices, or any specified in
// Logger::stringToLevel(name).
int ModApiUtil::l_log(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string_view text;
	LogLevel level = LL_NONE;
	if (lua_isnoneornil(L, 2)) {
		text = readParam<std::string_view>(L, 1);
	} else {
		auto name = readParam<std::string_view>(L, 1);
		text = readParam<std::string_view>(L, 2);
		if (name == "deprecated") {
			// core.log("deprecated", message [, stack_level])
			// Level 1 - immediate caller of core.log (probably engine code);
			// Level 2 - caller of the function that called core.log, and so on
			int stack_level = readParam<int>(L, 3, 2);
			if (stack_level < 1)
				throw LuaError("invalid stack level");
			log_deprecated(L, text, stack_level);
			return 0;
		}
		level = Logger::stringToLevel(name);
		if (level == LL_MAX) {
			warningstream << "Tried to log at unknown level '" << name
				<< "'. Defaulting to \"none\"." << std::endl;
			level = LL_NONE;
		}
	}
	g_logger.log(level, text);
	return 0;
}

// get_us_time()
int ModApiUtil::l_get_us_time(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushnumber(L, porting::getTimeUs());
	return 1;
}

// get_us_time() for SSCSM
int ModApiUtil::l_get_us_time_sscsm(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	auto t = porting::getTimeUs();
	t = t - t % SSCSM_CLOCK_RESOLUTION_US;
	lua_pushnumber(L, t);
	return 1;
}

// Maximum depth of a JSON object:
// Reading and writing should not overflow the Lua, C, or jsoncpp stacks.
constexpr static u16 MAX_JSON_DEPTH = 1024;

// parse_json(str[, nullvalue, return_error])
int ModApiUtil::l_parse_json(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const auto jsonstr = readParam<std::string_view>(L, 1);

	// Use passed nullvalue or default to nil
	int nullindex = 2;
	if (lua_isnone(L, nullindex)) {
		lua_pushnil(L);
		nullindex = lua_gettop(L);
	}

	bool return_error = lua_toboolean(L, 3);
	const auto handle_error = [&](const char *errmsg) {
		if (return_error) {
			lua_pushnil(L);
			lua_pushstring(L, errmsg);
			return 2;
		}
		errorstream << "Failed to parse json data: " << errmsg << std::endl;
		errorstream << "data: \"";
		if (jsonstr.size() <= 100) {
			errorstream << jsonstr << "\"";
		} else {
			errorstream << jsonstr.substr(0, 100) << "\"... (truncated)";
		}
		errorstream << std::endl;
		lua_pushnil(L);
		return 1;
	};

	Json::Value root;
	{
		Json::CharReaderBuilder builder;
		builder.settings_["stackLimit"] = MAX_JSON_DEPTH;
		builder.settings_["collectComments"] = false;
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		std::string errmsg;
		if (!reader->parse(jsonstr.data(), jsonstr.data() + jsonstr.size(), &root, &errmsg))
			return handle_error(errmsg.c_str());
	}

	if (!push_json_value(L, root, nullindex))
		return handle_error("depth exceeds lua stack limit");

	return 1;
}

// write_json(data[, styled]) -> string or nil and error message
int ModApiUtil::l_write_json(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	bool styled = false;
	if (!lua_isnone(L, 2)) {
		styled = readParam<bool>(L, 2);
		lua_pop(L, 1);
	}

	Json::Value root;
	try {
		read_json_value(L, root, 1, MAX_JSON_DEPTH);
	} catch (SerializationError &e) {
		lua_pushnil(L);
		lua_pushstring(L, e.what());
		return 2;
	}

	std::string out;
	if (styled) {
		out = root.toStyledString();
	} else {
		out = fastWriteJson(root);
	}
	lua_pushlstring(L, out.c_str(), out.size());
	return 1;
}

// get_tool_wear_after_use(uses[, initial_wear])
int ModApiUtil::l_get_tool_wear_after_use(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	u32 uses = readParam<int>(L, 1);
	u16 initial_wear = readParam<int>(L, 2, 0);
	u32 add_wear = calculateResultWear(uses, initial_wear);
	lua_pushnumber(L, add_wear);
	return 1;
}

// get_dig_params(groups, tool_capabilities[, wear])
int ModApiUtil::l_get_dig_params(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ItemGroupList groups;
	read_groups(L, 1, groups);
	ToolCapabilities tp = read_tool_capabilities(L, 2);
	if (lua_isnoneornil(L, 3)) {
		push_dig_params(L, getDigParams(groups, &tp));
	} else {
		u16 wear = readParam<int>(L, 3);
		push_dig_params(L, getDigParams(groups, &tp, wear));
	}
	return 1;
}

// get_hit_params(groups, tool_capabilities[, time_from_last_punch, [, wear]])
int ModApiUtil::l_get_hit_params(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ItemGroupList groups;
	read_groups(L, 1, groups);
	ToolCapabilities tp = read_tool_capabilities(L, 2);
	float time_from_last_punch = readParam<float>(L, 3, 1000000);
	int wear = readParam<int>(L, 4, 0);
	push_hit_params(L, getHitParams(groups, tp,
		time_from_last_punch, wear));
	return 1;
}

// check_password_entry(name, entry, password)
int ModApiUtil::l_check_password_entry(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string name = luaL_checkstring(L, 1);
	std::string entry = luaL_checkstring(L, 2);
	std::string password = luaL_checkstring(L, 3);

	if (base64_is_valid(entry)) {
		std::string hash = translate_password(name, password);
		lua_pushboolean(L, hash == entry);
		return 1;
	}

	std::string salt;
	std::string verifier;

	if (!decode_srp_verifier_and_salt(entry, &verifier, &salt)) {
		// invalid format
		warningstream << "Invalid password format for " << name << std::endl;
		lua_pushboolean(L, false);
		return 1;
	}
	std::string gen_verifier = generate_srp_verifier(name, password, salt);

	lua_pushboolean(L, gen_verifier == verifier);
	return 1;
}

// get_password_hash(name, raw_password)
int ModApiUtil::l_get_password_hash(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string name = luaL_checkstring(L, 1);
	std::string raw_password = luaL_checkstring(L, 2);
	std::string hash = translate_password(name, raw_password);
	lua_pushstring(L, hash.c_str());
	return 1;
}

// is_yes(arg)
int ModApiUtil::l_is_yes(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	lua_getglobal(L, "tostring"); // function to be called
	lua_pushvalue(L, 1); // 1st argument
	lua_call(L, 1, 1); // execute function
	std::string str = readParam<std::string>(L, -1); // get result
	lua_pop(L, 1);

	bool yes = is_yes(str);
	lua_pushboolean(L, yes);
	return 1;
}

// path_exists(path)
int ModApiUtil::l_path_exists(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string path = luaL_checkstring(L, 1); //path

	CHECK_SECURE_PATH(L, path.c_str(), false);

	bool exists = fs::PathExists(path);
	lua_pushboolean(L, exists);

	return 1;
}

// get_builtin_path()
int ModApiUtil::l_get_builtin_path(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string path = porting::path_share + DIR_DELIM + "builtin" + DIR_DELIM;
	lua_pushstring(L, path.c_str());

	return 1;
}

// get_user_path()
int ModApiUtil::l_get_user_path(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string path = porting::path_user;
	lua_pushstring(L, path.c_str());

	return 1;
}

enum LuaCompressMethod
{
	LUA_COMPRESS_METHOD_DEFLATE,
	LUA_COMPRESS_METHOD_RAW_DEFLATE,
	LUA_COMPRESS_METHOD_ZSTD,
};

static const struct EnumString es_LuaCompressMethod[] =
{
	{LUA_COMPRESS_METHOD_DEFLATE, "deflate"},
	{LUA_COMPRESS_METHOD_RAW_DEFLATE, "raw_deflate"},
	{LUA_COMPRESS_METHOD_ZSTD, "zstd"},
	{0, nullptr},
};

static LuaCompressMethod get_compress_method(lua_State *L, int index)
{
	if (lua_isnoneornil(L, index))
		return LUA_COMPRESS_METHOD_DEFLATE;
	const char *name = luaL_checkstring(L, index);
	int value;
	if (!string_to_enum(es_LuaCompressMethod, value, name)) {
		// Pretend it's deflate if we don't know, for compatibility reasons.
		log_deprecated(L, "Unknown compression method \"" + std::string(name)
			+ "\", defaulting to \"deflate\". You should pass a valid value.");
		return LUA_COMPRESS_METHOD_DEFLATE;
	}
	return (LuaCompressMethod) value;
}

// compress(data, method, level)
int ModApiUtil::l_compress(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto data = readParam<std::string_view>(L, 1);

	LuaCompressMethod method = get_compress_method(L, 2);

	std::ostringstream os(std::ios_base::binary);

	if (method == LUA_COMPRESS_METHOD_DEFLATE ||
			method == LUA_COMPRESS_METHOD_RAW_DEFLATE) {
		int level = -1;
		if (!lua_isnoneornil(L, 3))
			level = readParam<int>(L, 3);

		compressZlib(data, os, level, method == LUA_COMPRESS_METHOD_RAW_DEFLATE);
	} else if (method == LUA_COMPRESS_METHOD_ZSTD) {
		int level = ZSTD_CLEVEL_DEFAULT;
		if (!lua_isnoneornil(L, 3))
			level = readParam<int>(L, 3);

		compressZstd(data, os, level);
	}

	std::string out = os.str();

	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

// decompress(data, method)
int ModApiUtil::l_decompress(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto data = readParam<std::string_view>(L, 1);

	LuaCompressMethod method = get_compress_method(L, 2);

	// FIXME: zero copy possible in c++26 or with custom rdbuf
	std::istringstream is(std::string(data), std::ios_base::binary);
	std::ostringstream os(std::ios_base::binary);

	if (method == LUA_COMPRESS_METHOD_DEFLATE ||
			method == LUA_COMPRESS_METHOD_RAW_DEFLATE) {
		decompressZlib(is, os, 0, method == LUA_COMPRESS_METHOD_RAW_DEFLATE);
	} else if (method == LUA_COMPRESS_METHOD_ZSTD) {
		decompressZstd(is, os);
	}

	std::string out = os.str();

	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

// encode_base64(string)
int ModApiUtil::l_encode_base64(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto data = readParam<std::string_view>(L, 1);

	std::string out = base64_encode(data);

	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

// decode_base64(string)
int ModApiUtil::l_decode_base64(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto data = readParam<std::string_view>(L, 1);

	if (!base64_is_valid(data)) {
		lua_pushnil(L);
		return 1;
	}

	std::string out = base64_decode(data);

	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

// mkdir(path)
int ModApiUtil::l_mkdir(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *path = luaL_checkstring(L, 1);
	CHECK_SECURE_PATH(L, path, true);
	lua_pushboolean(L, fs::CreateAllDirs(path));
	return 1;
}

// rmdir(path, recursive)
int ModApiUtil::l_rmdir(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *path = luaL_checkstring(L, 1);
	CHECK_SECURE_PATH(L, path, true);

	bool recursive = readParam<bool>(L, 2, false);

	if (recursive)
		lua_pushboolean(L, fs::RecursiveDelete(path));
	else
		lua_pushboolean(L, fs::DeleteSingleFileOrEmptyDirectory(path));

	return 1;
}

// cpdir(source, destination)
int ModApiUtil::l_cpdir(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *source = luaL_checkstring(L, 1);
	const char *destination = luaL_checkstring(L, 2);
	CHECK_SECURE_PATH(L, source, false);
	CHECK_SECURE_PATH(L, destination, true);

	lua_pushboolean(L, fs::CopyDir(source, destination));
	return 1;
}

// mpdir(source, destination)
int ModApiUtil::l_mvdir(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *source = luaL_checkstring(L, 1);
	const char *destination = luaL_checkstring(L, 2);
	CHECK_SECURE_PATH(L, source, true);
	CHECK_SECURE_PATH(L, destination, true);

	lua_pushboolean(L, fs::MoveDir(source, destination));
	return 1;
}

// get_dir_list(path, is_dir)
int ModApiUtil::l_get_dir_list(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *path = luaL_checkstring(L, 1);
	bool list_all = !lua_isboolean(L, 2); // if it's not a boolean list all
	bool list_dirs = readParam<bool>(L, 2); // true: list dirs, false: list files

	CHECK_SECURE_PATH(L, path, false);

	std::vector<fs::DirListNode> list = fs::GetDirListing(path);

	int index = 0;
	lua_newtable(L);

	for (const fs::DirListNode &dln : list) {
		if (list_all || list_dirs == dln.dir) {
			lua_pushstring(L, dln.name.c_str());
			lua_rawseti(L, -2, ++index);
		}
	}

	return 1;
}

// safe_file_write(path, content)
int ModApiUtil::l_safe_file_write(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *path = luaL_checkstring(L, 1);
	auto content = readParam<std::string_view>(L, 2);

	CHECK_SECURE_PATH(L, path, true);

	bool ret = fs::safeWriteToFile(path, content);
	lua_pushboolean(L, ret);

	return 1;
}

// request_insecure_environment()
int ModApiUtil::l_request_insecure_environment(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	if (ScriptApiSecurity::isSecure(L)) {
		if (!ScriptApiSecurity::checkWhitelisted(L, "secure.trusted_mods"))
			return 0;
	}

	// Push insecure environment
	ScriptApiSecurity::getGlobalsBackup(L);
	return 1;
}

// get_version()
int ModApiUtil::l_get_version(lua_State *L)
{
	lua_createtable(L, 0, 3);
	int table = lua_gettop(L);

	lua_pushstring(L, PROJECT_NAME_C);
	lua_setfield(L, table, "project");

	lua_pushstring(L, g_version_string);
	lua_setfield(L, table, "string");

	lua_pushnumber(L, SERVER_PROTOCOL_VERSION_MIN);
	lua_setfield(L, table, "proto_min");

	lua_pushnumber(L, LATEST_PROTOCOL_VERSION);
	lua_setfield(L, table, "proto_max");

	if (strcmp(g_version_string, g_version_hash) != 0) {
		lua_pushstring(L, g_version_hash);
		lua_setfield(L, table, "hash");
	}

	lua_pushboolean(L, DEVELOPMENT_BUILD);
	lua_setfield(L, table, "is_dev");
	return 1;
}

int ModApiUtil::l_sha1(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto data = readParam<std::string_view>(L, 1);
	bool hex = !lua_isboolean(L, 2) || !readParam<bool>(L, 2);

	// Compute actual checksum of data
	std::string data_sha1 = hashing::sha1(data);

	if (hex) {
		std::string sha1_hex = hex_encode(data_sha1);
		lua_pushstring(L, sha1_hex.c_str());
	} else {
		lua_pushlstring(L, data_sha1.data(), data_sha1.size());
	}

	return 1;
}

int ModApiUtil::l_sha256(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto data = readParam<std::string_view>(L, 1);
	bool hex = !lua_isboolean(L, 2) || !readParam<bool>(L, 2);

	std::string data_sha256 = hashing::sha256(data);

	if (hex) {
		lua_pushstring(L, hex_encode(data_sha256).c_str());
	} else {
		lua_pushlstring(L, data_sha256.data(), data_sha256.size());
	}

	return 1;
}

// colorspec_to_colorstring(colorspec)
int ModApiUtil::l_colorspec_to_colorstring(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	video::SColor color;
	if (read_color(L, 1, &color)) {
		char colorstring[10];
		snprintf(colorstring, 10, "#%02X%02X%02X%02X",
			color.getRed(), color.getGreen(), color.getBlue(), color.getAlpha());
		lua_pushstring(L, colorstring);
		return 1;
	}

	return 0;
}

// colorspec_to_bytes(colorspec)
int ModApiUtil::l_colorspec_to_bytes(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	video::SColor color;
	if (read_color(L, 1, &color)) {
		u8 colorbytes[4] = {
			(u8) color.getRed(),
			(u8) color.getGreen(),
			(u8) color.getBlue(),
			(u8) color.getAlpha(),
		};
		lua_pushlstring(L, (const char*) colorbytes, 4);
		return 1;
	}

	return 0;
}

// colorspec_to_table(colorspec)
int ModApiUtil::l_colorspec_to_table(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	video::SColor color;
	if (read_color(L, 1, &color)) {
		push_ARGB8(L, color);
		return 1;
	}

	return 0;
}

// time_to_day_night_ratio(time_of_day)
int ModApiUtil::l_time_to_day_night_ratio(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	float time_of_day = lua_tonumber(L, 1) * 24000;
	u32 dnr = time_to_daynight_ratio(time_of_day, true);
	lua_pushnumber(L, dnr / 1000.0f);
	return 1;
}

// encode_png(w, h, data, level)
int ModApiUtil::l_encode_png(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// The args are already pre-validated on the lua side.
	u32 width = readParam<int>(L, 1);
	u32 height = readParam<int>(L, 2);
	const char *data = luaL_checklstring(L, 3, NULL);
	s32 compression = readParam<int>(L, 4);

	std::string out = encodePNG((const u8*)data, width, height, compression);

	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

// get_last_run_mod()
int ModApiUtil::l_get_last_run_mod(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string current_mod = ScriptApiBase::getCurrentModNameInsecure(L);
	if (current_mod.empty())
		current_mod = getScriptApiBase(L)->getOrigin();
	lua_pushstring(L, current_mod.c_str());
	return 1;
}

// set_last_run_mod(modname)
int ModApiUtil::l_set_last_run_mod(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const char *mod = luaL_checkstring(L, 1);
	getScriptApiBase(L)->setOriginDirect(mod);
	return 0;
}

// urlencode(value)
int ModApiUtil::l_urlencode(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto s = readParam<std::string_view>(L, 1);
	lua_pushstring(L, urlencode(s).c_str());
	return 1;
}

// is_valid_player_name(name)
int ModApiUtil::l_is_valid_player_name(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto s = readParam<std::string_view>(L, 1);
	lua_pushboolean(L, is_valid_player_name(s));
	return 1;
}

// strip_escapes(str)
int ModApiUtil::l_strip_escapes(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto s = readParam<std::string_view>(L, 1);
	auto r = unescape_enriched(s);
	lua_pushlstring(L, r.c_str(), r.size());
	return 1;
}

#ifdef __EMSCRIPTEN__

// set_web_video({clip = "reel.webm", title = "...", loop = true, muted = true})
//
// Hands a clip to the browser page's own <video> element. This exists only in
// the WebAssembly client: a mod feature-detects it with
// `if core.set_web_video then` and falls back to whatever it can render with
// engine-native nodes. `clip` names a file the launcher serves from its media/
// directory — same-origin by construction, and never a URL.
int ModApiUtil::l_set_web_video(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "clip");
	std::string clip = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "title");
	std::string title = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
	lua_pop(L, 1);

	lua_getfield(L, 1, "loop");
	const bool loop = lua_isnil(L, -1) ? true : readParam<bool>(L, -1);
	lua_pop(L, 1);

	// Browsers refuse to autoplay audible media without a user gesture, so
	// muted defaults to true and the page offers its own unmute control.
	lua_getfield(L, 1, "muted");
	const bool muted = lua_isnil(L, -1) ? true : readParam<bool>(L, -1);
	lua_pop(L, 1);

	lua_pushboolean(L, porting::emscripten_show_video_overlay(clip, title,
			loop, muted));
	return 1;
}

// clear_web_video()
int ModApiUtil::l_clear_web_video(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	porting::emscripten_hide_video_overlay();
	lua_pushboolean(L, true);
	return 1;
}

// set_web_video_texture({clip = "reel.webm", texture = "lw_screen_video.png",
//                         width = 320, height = 180, fps = 24})
//
// Theater Tier C spike (GH issue #29): instead of an HTML overlay, the
// browser decodes the clip and republishes frames onto an engine texture, so
// any tile/node already using `texture` picks up the video directly. This is
// a feasibility spike -- one quad, capped resolution/fps (see
// porting_emscripten.h) -- not a production replacement for Tier B.
int ModApiUtil::l_set_web_video_texture(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "clip");
	std::string clip = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "texture");
	std::string texture = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "width");
	const int width = lua_isnil(L, -1) ? porting::VIDEO_TEXTURE_SPIKE_MAX_WIDTH
			: readParam<int>(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "height");
	const int height = lua_isnil(L, -1) ? porting::VIDEO_TEXTURE_SPIKE_MAX_HEIGHT
			: readParam<int>(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "fps");
	const int fps = lua_isnil(L, -1) ? porting::VIDEO_TEXTURE_SPIKE_MAX_FPS
			: readParam<int>(L, -1);
	lua_pop(L, 1);

	lua_pushboolean(L, porting::emscripten_start_video_texture(
			clip, texture, width, height, fps));
	return 1;
}

// clear_web_video_texture()
int ModApiUtil::l_clear_web_video_texture(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	porting::emscripten_stop_video_texture();
	lua_pushboolean(L, true);
	return 1;
}

// web_media_request("songs/around_happy_hard")
//
// Asks the page to fetch a set of media files (client/web/runtime_media.js)
// and write them into the virtual filesystem, at the directory the build's
// asset manifest names for that id. Returns a state string and never blocks:
//   "ready"        the files are there; read them from their usual path
//   "loading"      being downloaded; ask again later
//   "busy"         a different set is downloading (one at a time); ask again
//   "missing"      this build does not ship it
//   "failed"       network or integrity failure; asking again retries later
//   "invalid"      not a media id
//   "unavailable"  no page helper
// Defined only in the WebAssembly client, so a mod feature-detects it with
// `if core.web_media_request then` and otherwise reads the files from disk.
int ModApiUtil::l_web_media_request(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const std::string id = luaL_checkstring(L, 1);
	const char *state = "unavailable";
	switch (porting::emscripten_web_media_request(id)) {
	case porting::WebMediaState::Ready: state = "ready"; break;
	case porting::WebMediaState::Loading: state = "loading"; break;
	case porting::WebMediaState::Busy: state = "busy"; break;
	case porting::WebMediaState::Missing: state = "missing"; break;
	case porting::WebMediaState::Failed: state = "failed"; break;
	case porting::WebMediaState::Invalid: state = "invalid"; break;
	case porting::WebMediaState::Unavailable: break;
	}
	lua_pushstring(L, state);
	return 1;
}

// web_media_release(id)
//
// Removes a fetched set's files again once the engine has read them (after
// the core.dynamic_add_media callback). Returns true if there was anything.
int ModApiUtil::l_web_media_release(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const std::string id = luaL_checkstring(L, 1);
	lua_pushboolean(L, porting::emscripten_web_media_release(id));
	return 1;
}

#endif // __EMSCRIPTEN__

void ModApiUtil::Initialize(lua_State *L, int top)
{
	API_FCT(log);

	API_FCT(get_us_time);

	API_FCT(parse_json);
	API_FCT(write_json);

	API_FCT(get_tool_wear_after_use);
	API_FCT(get_dig_params);
	API_FCT(get_hit_params);

	API_FCT(check_password_entry);
	API_FCT(get_password_hash);

	API_FCT(is_yes);

	API_FCT(path_exists);

	API_FCT(get_builtin_path);
	API_FCT(get_user_path);

	API_FCT(compress);
	API_FCT(decompress);

	API_FCT(mkdir);
	API_FCT(rmdir);
	API_FCT(cpdir);
	API_FCT(mvdir);
	API_FCT(get_dir_list);
	API_FCT(safe_file_write);

	API_FCT(request_insecure_environment);

	API_FCT(encode_base64);
	API_FCT(decode_base64);

	API_FCT(get_version);
	API_FCT(sha1);
	API_FCT(sha256);
	API_FCT(colorspec_to_colorstring);
	API_FCT(colorspec_to_bytes);
	API_FCT(colorspec_to_table);
	API_FCT(time_to_day_night_ratio);

	API_FCT(encode_png);

	API_FCT(get_last_run_mod);
	API_FCT(set_last_run_mod);

	API_FCT(urlencode);
	API_FCT(is_valid_player_name);
	API_FCT(strip_escapes);

#ifdef __EMSCRIPTEN__
	// Browser-only. Left undefined elsewhere so a mod can feature-detect it.
	API_FCT(set_web_video);
	API_FCT(clear_web_video);
	API_FCT(set_web_video_texture);
	API_FCT(clear_web_video_texture);
	API_FCT(web_media_request);
	API_FCT(web_media_release);
#endif

	LuaSettings::create(L, g_settings, g_settings_path);
	lua_setfield(L, top, "settings");
}

void ModApiUtil::InitializeClient(lua_State *L, int top)
{
	API_FCT(log);

	API_FCT(get_us_time);

	API_FCT(parse_json);
	API_FCT(write_json);

	API_FCT(is_yes);

	API_FCT(compress);
	API_FCT(decompress);

	API_FCT(encode_base64);
	API_FCT(decode_base64);

	API_FCT(get_version);
	API_FCT(sha1);
	API_FCT(sha256);
	API_FCT(colorspec_to_colorstring);
	API_FCT(colorspec_to_bytes);
	API_FCT(colorspec_to_table);
	API_FCT(time_to_day_night_ratio);

	API_FCT(get_last_run_mod);
	API_FCT(set_last_run_mod);

	API_FCT(urlencode);
	API_FCT(strip_escapes);

	LuaSettings::create(L, g_settings, g_settings_path);
	lua_setfield(L, top, "settings");
}

void ModApiUtil::InitializeSSCSM(lua_State *L, int top)
{
	API_FCT(log);

	registerFunction(L, "get_us_time", l_get_us_time_sscsm, top);

	API_FCT(parse_json);
	API_FCT(write_json);

	API_FCT(is_yes);

	API_FCT(compress);
	API_FCT(decompress);

	API_FCT(encode_base64);
	API_FCT(decode_base64);

	API_FCT(get_version);
	API_FCT(sha1);
	API_FCT(sha256);
	API_FCT(colorspec_to_colorstring);
	API_FCT(colorspec_to_bytes);
	API_FCT(colorspec_to_table);
	API_FCT(time_to_day_night_ratio);

	API_FCT(get_last_run_mod);
	API_FCT(set_last_run_mod);

	API_FCT(urlencode);
}

void ModApiUtil::InitializeAsync(lua_State *L, int top)
{
	API_FCT(log);

	API_FCT(get_us_time);

	API_FCT(parse_json);
	API_FCT(write_json);

	API_FCT(is_yes);

	API_FCT(path_exists);

	API_FCT(get_builtin_path);
	API_FCT(get_user_path);

	API_FCT(compress);
	API_FCT(decompress);

	API_FCT(mkdir);
	API_FCT(rmdir);
	API_FCT(cpdir);
	API_FCT(mvdir);
	API_FCT(get_dir_list);
	API_FCT(safe_file_write);

	// no request_insecure_environment here! mod origins are not tracked securely here.

	API_FCT(encode_base64);
	API_FCT(decode_base64);

	API_FCT(get_version);
	API_FCT(sha1);
	API_FCT(sha256);
	API_FCT(colorspec_to_colorstring);
	API_FCT(colorspec_to_bytes);
	API_FCT(colorspec_to_table);
	API_FCT(time_to_day_night_ratio);

	API_FCT(encode_png);

	API_FCT(get_last_run_mod);
	API_FCT(set_last_run_mod);

	API_FCT(urlencode);
	API_FCT(strip_escapes);

	LuaSettings::create(L, g_settings, g_settings_path);
	lua_setfield(L, top, "settings");
}
