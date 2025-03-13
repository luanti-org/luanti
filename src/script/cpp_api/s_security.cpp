// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_security.h"
#include "lua_api/l_base.h"
#include "filesys.h"
#include "porting.h"
#include "server.h"
#if CHECK_CLIENT_BUILD()
#include "client/client.h"
#endif
#include "settings.h"

#include <cerrno>
#include <string>
#include <algorithm>
#include <iostream>


#define SECURE_API(lib, name) \
	lua_pushcfunction(L, sl_##lib##_##name); \
	lua_setfield(L, -2, #name);


static inline void copy_safe(lua_State *L, const char *list[], unsigned len, int from=-2, int to=-1)
{
	if (from < 0) from = lua_gettop(L) + from + 1;
	if (to   < 0) to   = lua_gettop(L) + to   + 1;
	for (unsigned i = 0; i < (len / sizeof(list[0])); i++) {
		lua_getfield(L, from, list[i]);
		lua_setfield(L, to,   list[i]);
	}
}

static void shallow_copy_table(lua_State *L, int from=-2, int to=-1)
{
	if (from < 0) from = lua_gettop(L) + from + 1;
	if (to   < 0) to   = lua_gettop(L) + to   + 1;
	lua_pushnil(L);
	while (lua_next(L, from) != 0) {
		assert(lua_type(L, -1) != LUA_TTABLE);
		// duplicate key and value for lua_rawset
		lua_pushvalue(L, -2);
		lua_pushvalue(L, -2);
		lua_rawset(L, to);
		lua_pop(L, 1);
	}
}

// Pushes the original version of a library function on the stack, from the old version
static inline void push_original(lua_State *L, const char *lib, const char *func)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);
	lua_getfield(L, -1, lib);
	lua_remove(L, -2);  // Remove globals_backup
	lua_getfield(L, -1, func);
	lua_remove(L, -2);  // Remove lib
}


void ScriptApiSecurity::initializeSecurity()
{
	static const char *whitelist[] = {
		"assert",
		"core",
		"collectgarbage",
		"DIR_DELIM",
		"PLATFORM",
		"error",
		"getfenv",
		"getmetatable",
		"ipairs",
		"next",
		"pairs",
		"pcall",
		"print",
		"rawequal",
		"rawget",
		"rawset",
		"select",
		"setfenv",
		"setmetatable",
		"tonumber",
		"tostring",
		"type",
		"unpack",
		"_VERSION",
		"xpcall",
	};
	static const char *whitelist_tables[] = {
		// These libraries are completely safe BUT we need to duplicate their table
		// to ensure the sandbox can't affect the insecure env
		"coroutine",
		"string",
		"table",
		"math",
		"bit",
		// Not sure if completely safe. But if someone enables tracy, they'll
		// know what they do.
#if BUILD_WITH_TRACY
		"tracy",
#endif
	};
	static const char *io_whitelist[] = {
		"close",
		"flush",
		"read",
		"type",
		"write",
	};
	static const char *os_whitelist[] = {
		"clock",
		"date",
		"difftime",
		"getenv",
		"time",
	};
	static const char *debug_whitelist[] = {
		"gethook",
		"traceback",
		"getinfo",
		"upvalueid",
		"sethook",
		"debug",
	};
	static const char *package_whitelist[] = {
		"config",
		"cpath",
		"path",
		"searchpath",
	};
#if USE_LUAJIT
	static const char *jit_whitelist[] = {
		"arch",
		"flush",
		"off",
		"on",
		"opt",
		"os",
		"status",
		"version",
		"version_num",
	};
#endif
	m_secure = true;

	lua_State *L = getStack();

	// Backup globals to the registry
	lua_getglobal(L, "_G");
	lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);

	// Replace the global environment with an empty one
	int thread = getThread(L);
	createEmptyEnv(L);
	setLuaEnv(L, thread);

	// Get old globals
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);
	int old_globals = lua_gettop(L);


	// Copy safe base functions
	lua_getglobal(L, "_G");
	copy_safe(L, whitelist, sizeof(whitelist));

	// And replace unsafe ones
	SECURE_API(g, dofile);
	SECURE_API(g, load);
	SECURE_API(g, loadfile);
	SECURE_API(g, loadstring);
	SECURE_API(g, require);
	lua_pop(L, 1);


	// Copy safe libraries
	for (const char *libname : whitelist_tables) {
		lua_getfield(L, old_globals, libname);
		lua_newtable(L);
		shallow_copy_table(L);

		lua_setglobal(L, libname);
		lua_pop(L, 1);
	}


	// Copy safe IO functions
	lua_getfield(L, old_globals, "io");
	lua_newtable(L);
	copy_safe(L, io_whitelist, sizeof(io_whitelist));

	// And replace unsafe ones
	SECURE_API(io, open);
	SECURE_API(io, input);
	SECURE_API(io, output);
	SECURE_API(io, lines);

	lua_setglobal(L, "io");
	lua_pop(L, 1);  // Pop old IO


	// Copy safe OS functions
	lua_getfield(L, old_globals, "os");
	lua_newtable(L);
	copy_safe(L, os_whitelist, sizeof(os_whitelist));

	// And replace unsafe ones
	SECURE_API(os, remove);
	SECURE_API(os, rename);
	SECURE_API(os, setlocale);

	lua_setglobal(L, "os");
	lua_pop(L, 1);  // Pop old OS


	// Copy safe debug functions
	lua_getfield(L, old_globals, "debug");
	lua_newtable(L);
	copy_safe(L, debug_whitelist, sizeof(debug_whitelist));
	lua_setglobal(L, "debug");
	lua_pop(L, 1);  // Pop old debug


	// Copy safe package fields
	lua_getfield(L, old_globals, "package");
	lua_newtable(L);
	copy_safe(L, package_whitelist, sizeof(package_whitelist));
	lua_setglobal(L, "package");
	lua_pop(L, 1);  // Pop old package

#if USE_LUAJIT
	// Copy safe jit functions, if they exist
	lua_getfield(L, -1, "jit");
	if (!lua_isnil(L, -1)) {
		lua_newtable(L);
		copy_safe(L, jit_whitelist, sizeof(jit_whitelist));
		lua_setglobal(L, "jit");
	}
	lua_pop(L, 1);  // Pop old jit
#endif

	// Get rid of 'core' in the old globals, we don't want anyone thinking it's
	// safe or even usable.
	lua_pushnil(L);
	lua_setfield(L, old_globals, "core");

	lua_pop(L, 1); // Pop globals_backup


	/*
	 * In addition to copying the tables in whitelist_tables, we also need to
	 * replace the string metatable. Otherwise old_globals.string would
	 * be accessible via getmetatable("").__index from inside the sandbox.
	 */
	lua_pushliteral(L, "");
	lua_newtable(L);
	lua_getglobal(L, "string");
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
	lua_pop(L, 1); // Pop empty string
}

#if CHECK_CLIENT_BUILD()

void ScriptApiSecurity::initializeSecurityClient()
{
	static const char *whitelist[] = {
		"assert",
		"core",
		"collectgarbage",
		"DIR_DELIM",
		"error",
		"getfenv",
		"ipairs",
		"next",
		"pairs",
		"pcall",
		"print",
		"rawequal",
		"rawget",
		"rawset",
		"select",
		"setfenv",
		"getmetatable",
		"setmetatable",
		"tonumber",
		"tostring",
		"type",
		"unpack",
		"_VERSION",
		"xpcall",
		// Completely safe libraries
		"coroutine",
		"string",
		"table",
		"math",
		"bit",
		// Not sure if completely safe. But if someone enables tracy, they'll
		// know what they do.
#if BUILD_WITH_TRACY
		"tracy",
#endif
	};
	static const char *os_whitelist[] = {
		"clock",
		"date",
		"difftime",
		"time"
	};
	static const char *debug_whitelist[] = {
		"getinfo", // used by builtin and unset before mods load
		"traceback"
	};

#if USE_LUAJIT
	static const char *jit_whitelist[] = {
		"arch",
		"flush",
		"off",
		"on",
		"opt",
		"os",
		"status",
		"version",
		"version_num",
	};
#endif

	m_secure = true;

	lua_State *L = getStack();
	int thread = getThread(L);

	// create an empty environment
	createEmptyEnv(L);

	// Copy safe base functions
	lua_getglobal(L, "_G");
	lua_getfield(L, -2, "_G");
	copy_safe(L, whitelist, sizeof(whitelist));

	// And replace unsafe ones
	SECURE_API(g, dofile);
	SECURE_API(g, load);
	SECURE_API(g, loadfile);
	SECURE_API(g, loadstring);
	SECURE_API(g, require);
	lua_pop(L, 2);



	// Copy safe OS functions
	lua_getglobal(L, "os");
	lua_newtable(L);
	copy_safe(L, os_whitelist, sizeof(os_whitelist));
	lua_setfield(L, -3, "os");
	lua_pop(L, 1);  // Pop old OS


	// Copy safe debug functions
	lua_getglobal(L, "debug");
	lua_newtable(L);
	copy_safe(L, debug_whitelist, sizeof(debug_whitelist));
	lua_setfield(L, -3, "debug");
	lua_pop(L, 1);  // Pop old debug

#if USE_LUAJIT
	// Copy safe jit functions, if they exist
	lua_getglobal(L, "jit");
	lua_newtable(L);
	copy_safe(L, jit_whitelist, sizeof(jit_whitelist));
	lua_setfield(L, -3, "jit");
	lua_pop(L, 1);  // Pop old jit
#endif

	// Set the environment to the one we created earlier
	setLuaEnv(L, thread);
}

#endif

int ScriptApiSecurity::getThread(lua_State *L)
{
#if LUA_VERSION_NUM <= 501
	int is_main = lua_pushthread(L);  // Push the main thread
	FATAL_ERROR_IF(!is_main, "Security: ScriptApi's Lua state "
		"isn't the main Lua thread!");
	return lua_gettop(L);
#endif
	return 0;
}

void ScriptApiSecurity::createEmptyEnv(lua_State *L)
{
	lua_newtable(L);  // Create new environment
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "_G");  // Create the _G loop
}

void ScriptApiSecurity::setLuaEnv(lua_State *L, int thread)
{
#if LUA_VERSION_NUM >= 502  // Lua >= 5.2
	// Set the global environment
	lua_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#else  // Lua <= 5.1
	// Set the environment of the main thread
	FATAL_ERROR_IF(!lua_setfenv(L, thread), "Security: Unable to set "
		"environment of the main Lua thread!");
	lua_pop(L, 1);  // Pop thread
#endif
}

bool ScriptApiSecurity::isSecure(lua_State *L)
{
	auto *script = ModApiBase::getScriptApiBase(L);
	if (auto *sec = dynamic_cast<ScriptApiSecurity*>(script))
		return sec->m_secure;
	return false;
}

void ScriptApiSecurity::getGlobalsBackup(lua_State *L)
{
	if (!ScriptApiSecurity::isSecure(L)) {
		lua_getglobal(L, "_G");
		return;
	}
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);
	// We cannot fulfill the callers wish securely if they don't exist.
	FATAL_ERROR_IF(lua_isnil(L, -1), "Globals backup requested, but it is not available. Cannot proceed securely.");
}

bool ScriptApiSecurity::safeLoadString(lua_State *L, std::string_view code, const char *chunk_name)
{
	if (code.size() > 0 && code[0] == LUA_SIGNATURE[0]) {
		lua_pushliteral(L, "Bytecode prohibited when mod security is enabled.");
		return false;
	}
	if (luaL_loadbuffer(L, code.data(), code.size(), chunk_name))
		return false;
	return true;
}

bool ScriptApiSecurity::safeLoadFile(lua_State *L, const char *path, const char *display_name)
{
	FILE *fp;
	char *chunk_name;
	if (!display_name)
		display_name = path;
	if (!path) {
		fp = stdin;
		chunk_name = const_cast<char *>("=stdin");
	} else {
		fp = std::fopen(path, "rb");
		if (!fp) {
			lua_pushfstring(L, "%s: %s", path, strerror(errno));
			return false;
		}
		size_t len = strlen(display_name) + 2;
		chunk_name = new char[len];
		snprintf(chunk_name, len, "@%s", display_name);
	}

	size_t start = 0;
	int c = std::getc(fp);
	if (c == '#') {
		// Skip the shebang line (but keep line-ending)
		while (c != EOF && c != '\n')
			c = std::getc(fp);
		start = std::ftell(fp) - 1;
	}

	// Read the file
	int ret = std::fseek(fp, 0, SEEK_END);
	if (ret) {
		lua_pushfstring(L, "%s: %s", path, strerror(errno));
		if (path) {
			std::fclose(fp);
			delete [] chunk_name;
		}
		return false;
	}

	size_t size = std::ftell(fp) - start;
	std::string code(size, '\0');
	ret = std::fseek(fp, start, SEEK_SET);
	if (ret) {
		lua_pushfstring(L, "%s: %s", path, strerror(errno));
		if (path) {
			std::fclose(fp);
			delete [] chunk_name;
		}
		return false;
	}

	size_t num_read = std::fread(&code[0], 1, size, fp);
	if (path)
		std::fclose(fp);
	if (num_read != size) {
		lua_pushliteral(L, "Error reading file to load.");
		if (path)
			delete [] chunk_name;
		return false;
	}

	bool result = safeLoadString(L, code, chunk_name);
	if (path)
		delete [] chunk_name;
	return result;
}


std::string ScriptApiSecurity::getCurrentModName(lua_State *L)
{
	auto *script = ModApiBase::getScriptApiBase(L);

	auto *sec = dynamic_cast<ScriptApiSecurity*>(script);
	if (sec && !sec->modNamesAreTrusted())
		return "";

	// We have to make sure that this function is being called directly by
	// a mod, otherwise a malicious mod could override a function and
	// steal its return value. (e.g. request_insecure_environment)
	lua_Debug info;

	// Make sure there's only one item below this function on the stack...
	if (lua_getstack(L, 2, &info))
		return "";
	FATAL_ERROR_IF(!lua_getstack(L, 1, &info), "lua_getstack() failed");
	FATAL_ERROR_IF(!lua_getinfo(L, "S", &info), "lua_getinfo() failed");

	// ...and that that item is the main file scope.
	if (strcmp(info.what, "main") != 0)
		return "";

	// at this point we can trust this value:
	return getCurrentModNameInsecure(L);
}


static bool checkModNameWhitelisted(const std::string &mod_name, const std::string &setting)
{
	assert(str_starts_with(setting, "secure."));

	if (mod_name.empty())
		return false;

	std::string value = g_settings->get(setting);
	value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
	auto mod_list = str_split(value, ',');

	return CONTAINS(mod_list, mod_name);
}


bool ScriptApiSecurity::checkWhitelisted(lua_State *L, const std::string &setting)
{
	std::string mod_name = getCurrentModName(L);
	return checkModNameWhitelisted(mod_name, setting);
}


bool ScriptApiSecurity::checkPath(lua_State *L, const char *path,
		bool write_required, bool *write_allowed)
{
	if (write_allowed)
		*write_allowed = false;

	// We can't use AbsolutePath() here since we want to allow creating paths that
	// do not yet exist. But RemoveRelativePathComponents() would also be incorrect
	// since that wouldn't normalize subpaths that *do* exist.
	// This is required so that comparisons with other normalized paths work correctly.
	std::string abs_path = fs::AbsolutePathPartial(path);
	tracestream << "ScriptApiSecurity: path \"" << path << "\" resolved to \""
		<< abs_path << "\"" << std::endl;

	if (abs_path.empty())
		return false;

	// Note: abs_path can be a valid path while path isn't, e.g.
	// abs_path = "/home/user/.luanti"
	// path = "/home/user/.luanti/noexist/.."
	// Letting this through the sandbox isn't a concern as any actual attempts to
	// use the path would fail.

	// Ask the environment-specific implementation
	auto *sec = ModApiBase::getScriptApi<ScriptApiSecurity>(L);
	return sec->checkPathInternal(abs_path, write_required, write_allowed);
}


bool ScriptApiSecurity::checkPathWithGamedef(lua_State *L,
	const std::string &abs_path, bool write_required, bool *write_allowed)
{
	const auto &set_write_allowed = [&] (bool v) {
		if (write_allowed)
			*write_allowed = v;
	};
	std::string str;  // Transient

	auto *gamedef = ModApiBase::getGameDef(L);
	if (!gamedef)
		return false;

	assert(!abs_path.empty());

	if (!g_settings_path.empty()) {
		// Don't allow accessing the settings file
		str = fs::AbsolutePathPartial(g_settings_path);
		if (str == abs_path)
			return false;
	}

	// Get mod name
	std::string mod_name = ScriptApiBase::getCurrentModNameInsecure(L);
	if (!mod_name.empty()) {
		// Builtin can access anything
		if (mod_name == BUILTIN_MOD_NAME) {
			set_write_allowed(true);
			return true;
		}
	}

	// Allow paths in mod path
	// Don't bother if write access isn't important, since it will be handled later
	if (write_required || write_allowed) {
		const ModSpec *mod = gamedef->getModSpec(mod_name);
		if (mod) {
			str = fs::AbsolutePath(mod->path);
			if (!str.empty() && fs::PathStartsWith(abs_path, str)) {
				// `mod_name` cannot be trusted here, so we catch the scenarios where this becomes a problem:
				bool is_trusted = checkModNameWhitelisted(mod_name, "secure.trusted_mods") ||
						checkModNameWhitelisted(mod_name, "secure.http_mods");
				std::string filename = lowercase(fs::GetFilenameFromPath(abs_path.c_str()));
				// By writing to any of these a malicious mod could turn itself into
				// an existing trusted mod by renaming or becoming a modpack.
				bool is_dangerous_file = filename == "mod.conf" ||
						filename == "modpack.conf" ||
						filename == "modpack.txt";
				if (write_required) {
					if (is_trusted) {
						throw LuaError(
								"Unable to write to a trusted or http mod's directory. "
								"For data storage consider minetest.get_mod_data_path() or minetest.get_worldpath() instead.");
					} else if (is_dangerous_file) {
						throw LuaError(
								"Unable to write to special file for security reasons");
					} else {
						const char *message =
								"Writing to mod directories is deprecated, as any changes "
								"will be overwritten when updating content. "
								"For data storage consider minetest.get_mod_data_path() or minetest.get_worldpath() instead.";
						log_deprecated(L, message, 1);
					}
				}
				set_write_allowed(!is_trusted && !is_dangerous_file);
				return true;
			}
		}
	}

	// Allow read-only access to builtin
	if (!write_required) {
		str = fs::AbsolutePath(Server::getBuiltinLuaPath());
		if (!str.empty() && fs::PathStartsWith(abs_path, str))
			return true;
	}

	// Allow read-only access to game directory
	if (!write_required) {
		const SubgameSpec *game_spec = gamedef->getGameSpec();
		if (game_spec && !game_spec->path.empty()) {
			str = fs::AbsolutePath(game_spec->path);
			if (!str.empty() && fs::PathStartsWith(abs_path, str))
				return true;
		}
	}

	// Allow read-only access to all mod directories
	if (!write_required) {
		const std::vector<ModSpec> &mods = gamedef->getMods();
		for (const ModSpec &mod : mods) {
			str = fs::AbsolutePath(mod.path);
			if (!str.empty() && fs::PathStartsWith(abs_path, str))
				return true;
		}
	}

	// Allow read/write access to all mod common dirs
	str = fs::AbsolutePath(gamedef->getModDataPath());
	if (!str.empty() && fs::PathStartsWith(abs_path, str)) {
		set_write_allowed(true);
		return true;
	}

	str = fs::AbsolutePath(gamedef->getWorldPath());
	if (!str.empty()) {
		// Don't allow access to other paths in the world mod/game path.
		// These have to be blocked so you can't override a trusted mod
		// by creating a mod with the same name in a world mod directory.
		// We add to the absolute path of the world instead of getting
		// the absolute paths directly because that won't work if they
		// don't exist.
		if (fs::PathStartsWith(abs_path, str + DIR_DELIM + "worldmods") ||
				fs::PathStartsWith(abs_path, str + DIR_DELIM + "game")) {
			return false;
		}
		// Allow all other paths in world path
		if (fs::PathStartsWith(abs_path, str)) {
			set_write_allowed(true);
			return true;
		}
	}

	return false;
}


int ScriptApiSecurity::sl_g_dofile(lua_State *L)
{
	int nret = sl_g_loadfile(L);
	if (nret != 1) {
		lua_error(L);
		// code after this function isn't executed
	}
	int top_precall = lua_gettop(L);
	lua_call(L, 0, LUA_MULTRET);
	// Return number of arguments returned by the function,
	// adjusting for the function being poped.
	return lua_gettop(L) - (top_precall - 1);
}


int ScriptApiSecurity::sl_g_load(lua_State *L)
{
	size_t len;
	const char *buf;
	std::string code;
	const char *chunk_name = "=(load)";

	luaL_checktype(L, 1, LUA_TFUNCTION);
	if (!lua_isnone(L, 2)) {
		luaL_checktype(L, 2, LUA_TSTRING);
		chunk_name = lua_tostring(L, 2);
	}

	while (true) {
		lua_pushvalue(L, 1);
		lua_call(L, 0, 1);
		int t = lua_type(L, -1);
		if (t == LUA_TNIL) {
			break;
		}

		if (t != LUA_TSTRING) {
			lua_pushnil(L);
			lua_pushliteral(L, "Loader didn't return a string");
			return 2;
		}
		buf = lua_tolstring(L, -1, &len);
		code += std::string(buf, len);
		lua_pop(L, 1); // Pop return value
	}
	if (!safeLoadString(L, code, chunk_name)) {
		lua_pushnil(L);
		lua_insert(L, -2);
		return 2;
	}
	return 1;
}


int ScriptApiSecurity::sl_g_loadfile(lua_State *L)
{
#if CHECK_CLIENT_BUILD()
	ScriptApiBase *script = ModApiBase::getScriptApiBase(L);

	// Client implementation
	if (script->getType() == ScriptingType::Client) {
		std::string path = readParam<std::string>(L, 1);
		const std::string *contents = script->getClient()->getModFile(path);
		if (!contents) {
			std::string error_msg = "Couldn't find script called: " + path;
			lua_pushnil(L);
			lua_pushstring(L, error_msg.c_str());
			return 2;
		}

		std::string chunk_name = "@" + path;
		if (!safeLoadString(L, *contents, chunk_name.c_str())) {
			lua_pushnil(L);
			lua_insert(L, -2);
			return 2;
		}
		return 1;
	}
#endif

	// Server implementation
	const char *path = NULL;
	if (lua_isstring(L, 1)) {
		path = lua_tostring(L, 1);
		CHECK_SECURE_PATH_INTERNAL(L, path, false, NULL);
	}

	if (!safeLoadFile(L, path)) {
		lua_pushnil(L);
		lua_insert(L, -2);
		return 2;
	}

	return 1;
}


int ScriptApiSecurity::sl_g_loadstring(lua_State *L)
{
	const char *chunk_name = "=(load)";

	luaL_checktype(L, 1, LUA_TSTRING);
	if (!lua_isnone(L, 2)) {
		luaL_checktype(L, 2, LUA_TSTRING);
		chunk_name = lua_tostring(L, 2);
	}

	size_t size;
	const char *code = lua_tolstring(L, 1, &size);
	std::string code_s(code, size);

	if (!safeLoadString(L, code_s, chunk_name)) {
		lua_pushnil(L);
		lua_insert(L, -2);
		return 2;
	}
	return 1;
}


int ScriptApiSecurity::sl_g_require(lua_State *L)
{
	lua_pushliteral(L, "require() is disabled when mod security is on.");
	return lua_error(L);
}


int ScriptApiSecurity::sl_io_open(lua_State *L)
{
	bool with_mode = lua_gettop(L) > 1;

	luaL_checktype(L, 1, LUA_TSTRING);
	const char *path = lua_tostring(L, 1);

	bool write_requested = false;
	if (with_mode) {
		luaL_checktype(L, 2, LUA_TSTRING);
		const char *mode = lua_tostring(L, 2);
		write_requested = strchr(mode, 'w') != NULL ||
			strchr(mode, '+') != NULL ||
			strchr(mode, 'a') != NULL;
	}
	CHECK_SECURE_PATH_INTERNAL(L, path, write_requested, NULL);

	push_original(L, "io", "open");
	lua_pushvalue(L, 1);
	if (with_mode) {
		lua_pushvalue(L, 2);
	}

	lua_call(L, with_mode ? 2 : 1, 2);
	return 2;
}


int ScriptApiSecurity::sl_io_input(lua_State *L)
{
	if (lua_isstring(L, 1)) {
		const char *path = lua_tostring(L, 1);
		CHECK_SECURE_PATH_INTERNAL(L, path, false, NULL);
	}

	push_original(L, "io", "input");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	return 1;
}


int ScriptApiSecurity::sl_io_output(lua_State *L)
{
	if (lua_isstring(L, 1)) {
		const char *path = lua_tostring(L, 1);
		CHECK_SECURE_PATH_INTERNAL(L, path, true, NULL);
	}

	push_original(L, "io", "output");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	return 1;
}


int ScriptApiSecurity::sl_io_lines(lua_State *L)
{
	if (lua_isstring(L, 1)) {
		const char *path = lua_tostring(L, 1);
		CHECK_SECURE_PATH_INTERNAL(L, path, false, NULL);
	}

	int top_precall = lua_gettop(L);
	push_original(L, "io", "lines");
	lua_pushvalue(L, 1);
	lua_call(L, 1, LUA_MULTRET);
	// Return number of arguments returned by the function,
	// adjusting for the function being poped.
	return lua_gettop(L) - top_precall;
}


int ScriptApiSecurity::sl_os_rename(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TSTRING);
	const char *path1 = lua_tostring(L, 1);
	CHECK_SECURE_PATH_INTERNAL(L, path1, true, NULL);

	luaL_checktype(L, 2, LUA_TSTRING);
	const char *path2 = lua_tostring(L, 2);
	CHECK_SECURE_PATH_INTERNAL(L, path2, true, NULL);

	push_original(L, "os", "rename");
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_call(L, 2, 2);
	return 2;
}


int ScriptApiSecurity::sl_os_remove(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TSTRING);
	const char *path = lua_tostring(L, 1);
	CHECK_SECURE_PATH_INTERNAL(L, path, true, NULL);

	push_original(L, "os", "remove");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 2);
	return 2;
}


int ScriptApiSecurity::sl_os_setlocale(lua_State *L)
{
	const bool cat = lua_gettop(L) > 1;
	// Don't allow changes
	if (!lua_isnoneornil(L, 1)) {
		lua_pushnil(L);
		return 1;
	}

	push_original(L, "os", "setlocale");
	lua_pushnil(L);
	if (cat)
		lua_pushvalue(L, 2);
	lua_call(L, cat ? 2 : 1, 1);
	return 1;
}
