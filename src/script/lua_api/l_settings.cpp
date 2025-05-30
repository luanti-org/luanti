// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 PilzAdam <pilzadam@minetest.net>

#include "lua_api/l_settings.h"
#include "lua_api/l_internal.h"
#include "cpp_api/s_security.h"
#include "threading/mutex_auto_lock.h"
#include "util/string.h" // FlagDesc
#include "settings.h"
#include "noise.h"
#include "log.h"
#include "common/c_converter.h"


/*
 * This protects the following from being set:
 * - 'secure.*' settings
 * - some security-relevant settings
 *   (better solution pending)
 * - some mapgen settings
 *   (not security-criticial, just to avoid messing up user configs)
 */
#define CHECK_SETTING_SECURITY(L, name) \
	if (o->m_settings == g_settings) { \
		if (checkSettingSecurity(L, name) == -1) \
			return 0; \
	}

static inline int checkSettingSecurity(lua_State* L, const std::string &name)
{
#if CHECK_CLIENT_BUILD()
	// Main menu and pause menu are allowed everything
	auto context = ModApiBase::getScriptApiBase(L)->getType();
	if (context == ScriptingType::MainMenu || context == ScriptingType::PauseMenu)
		return 0;
#endif

	if (ScriptApiSecurity::isSecure(L) && name.compare(0, 7, "secure.") == 0)
		throw LuaError("Attempted to set secure setting.");

	if (name == "mg_name" || name == "mg_flags") {
		errorstream << "Tried to set global setting " << name << ", ignoring. "
			"minetest.set_mapgen_setting() should be used instead." << std::endl;
		infostream << script_get_backtrace(L) << std::endl;
		return -1;
	}

	const char *disallowed[] = {
		"main_menu_script", "shader_path", "texture_path", "screenshot_path",
		"serverlist_file", "serverlist_url", "map-dir", "contentdb_url",
	};
	for (const char *name2 : disallowed) {
		if (name == name2)
			throw LuaError("Attempted to set disallowed setting.");
	}

	return 0;
}

LuaSettings::LuaSettings(Settings *settings, const std::string &filename) :
	m_settings(settings),
	m_filename(filename)
{
}

LuaSettings::LuaSettings(const std::string &filename, bool write_allowed) :
	m_filename(filename),
	m_is_own_settings(true),
	m_write_allowed(write_allowed)
{
	m_settings = new Settings();
	m_settings->readConfigFile(filename.c_str());
}

LuaSettings::~LuaSettings()
{
	if (m_is_own_settings)
		delete m_settings;
}


void LuaSettings::create(lua_State *L, Settings *settings,
		const std::string &filename)
{
	LuaSettings *o = new LuaSettings(settings, filename);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}


// garbage collector
int LuaSettings::gc_object(lua_State* L)
{
	LuaSettings* o = *(LuaSettings **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


// get(self, key) -> value
int LuaSettings::l_get(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));
	if (o->m_settings->exists(key)) {
		std::string value = o->m_settings->get(key);
		lua_pushstring(L, value.c_str());
	} else {
		lua_pushnil(L);
	}

	return 1;
}

// get_bool(self, key) -> boolean
int LuaSettings::l_get_bool(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));
	if (o->m_settings->exists(key)) {
		bool value = o->m_settings->getBool(key);
		lua_pushboolean(L, value);
	} else {
		// Push default value
		if (lua_isboolean(L, 3))
			lua_pushboolean(L, readParam<bool>(L, 3));
		else
			lua_pushnil(L);
	}

	return 1;
}

// get_np_group(self, key) -> value
int LuaSettings::l_get_np_group(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings *o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));
	if (o->m_settings->exists(key)) {
		NoiseParams np;
		o->m_settings->getNoiseParams(key, np);
		push_noiseparams(L, &np);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

// get_flags(self, key) -> table or nil
int LuaSettings::l_get_flags(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings *o = checkObject<LuaSettings>(L, 1);
	std::string key = std::string(luaL_checkstring(L, 2));

	u32 flags = 0;
	auto flagdesc = o->m_settings->getFlagDescFallback(key);
	if (o->m_settings->getFlagStrNoEx(key, flags, flagdesc)) {
		lua_newtable(L);
		int table = lua_gettop(L);
		for (size_t i = 0; flagdesc[i].name; ++i) {
			lua_pushboolean(L, flags & flagdesc[i].flag);
			lua_setfield(L, table, flagdesc[i].name);
		}
		lua_pushvalue(L, table);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

// get_pos(self, key) -> vector or nil
int LuaSettings::l_get_pos(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings *o = checkObject<LuaSettings>(L, 1);
	std::string key = luaL_checkstring(L, 2);

	std::optional<v3f> pos;
	if (o->m_settings->getV3FNoEx(key, pos) && pos.has_value())
		push_v3f(L, *pos);
	else
		lua_pushnil(L);
	return 1;
}

// set(self, key, value)
int LuaSettings::l_set(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));
	const char* value = luaL_checkstring(L, 3);

	CHECK_SETTING_SECURITY(L, key);

	if (!o->m_settings->set(key, value))
		throw LuaError("Invalid sequence found in setting parameters");

	return 0;
}

// set_bool(self, key, value)
int LuaSettings::l_set_bool(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));
	bool value = readParam<bool>(L, 3);

	CHECK_SETTING_SECURITY(L, key);

	o->m_settings->setBool(key, value);

	return 0;
}

// set_np_group(self, key, value)
int LuaSettings::l_set_np_group(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings *o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));
	NoiseParams value;
	read_noiseparams(L, 3, &value);

	CHECK_SETTING_SECURITY(L, key);

	o->m_settings->setNoiseParams(key, value);

	return 0;
}

// set_pos(self, key, value)
int LuaSettings::l_set_pos(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings *o = checkObject<LuaSettings>(L, 1);

	std::string key = luaL_checkstring(L, 2);
	v3f value = check_v3f(L, 3);

	CHECK_SETTING_SECURITY(L, key);

	o->m_settings->setV3F(key, value);

	return 0;
}

// remove(self, key) -> success
int LuaSettings::l_remove(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));

	CHECK_SETTING_SECURITY(L, key);

	bool success = o->m_settings->remove(key);
	lua_pushboolean(L, success);

	return 1;
}

// get_names(self) -> {key1, ...}
int LuaSettings::l_get_names(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	std::vector<std::string> keys = o->m_settings->getNames();

	lua_newtable(L);
	for (unsigned int i=0; i < keys.size(); i++)
	{
		lua_pushstring(L, keys[i].c_str());
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}

// has(self, key) -> boolean
int LuaSettings::l_has(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	std::string key = std::string(luaL_checkstring(L, 2));
	lua_pushboolean(L, o->m_settings->existsLocal(key));

	return 1;
}

// write(self) -> success
int LuaSettings::l_write(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	if (!o->m_write_allowed) {
		throw LuaError("Settings: writing " + o->m_filename +
				" not allowed with mod security on.");
	}

	bool success = o->m_settings->updateConfigFile(o->m_filename.c_str());
	lua_pushboolean(L, success);

	return 1;
}

static void push_settings_table(lua_State *L, const Settings *settings)
{
	std::vector<std::string> keys = settings->getNames();
	lua_newtable(L);
	for (const std::string &key : keys) {
		std::string value;
		Settings *group = nullptr;

		if (settings->getNoEx(key, value)) {
			lua_pushstring(L, value.c_str());
		} else if (settings->getGroupNoEx(key, group)) {
			// Recursively push tables
			push_settings_table(L, group);
		} else {
			// Impossible case (multithreading) due to MutexAutoLock
			continue;
		}

		lua_setfield(L, -2, key.c_str());
	}
}

// to_table(self) -> {[key1]=value1,...}
int LuaSettings::l_to_table(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaSettings* o = checkObject<LuaSettings>(L, 1);

	push_settings_table(L, o->m_settings);
	return 1;
}


void LuaSettings::Register(lua_State* L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<LuaSettings>(L, methods, metamethods);

	// Can be created from Lua (Settings(filename))
	lua_register(L, className, create_object);
}

// LuaSettings(filename)
// Creates a LuaSettings and leaves it on top of the stack
int LuaSettings::create_object(lua_State* L)
{
	NO_MAP_LOCK_REQUIRED;
	bool write_allowed = true;
	const char* filename = luaL_checkstring(L, 1);
	CHECK_SECURE_PATH_POSSIBLE_WRITE(L, filename, &write_allowed);
	LuaSettings* o = new LuaSettings(filename, write_allowed);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}

const char LuaSettings::className[] = "Settings";
const luaL_Reg LuaSettings::methods[] = {
	luamethod(LuaSettings, get),
	luamethod(LuaSettings, get_bool),
	luamethod(LuaSettings, get_np_group),
	luamethod(LuaSettings, get_flags),
	luamethod(LuaSettings, get_pos),
	luamethod(LuaSettings, set),
	luamethod(LuaSettings, set_bool),
	luamethod(LuaSettings, set_np_group),
	luamethod(LuaSettings, set_pos),
	luamethod(LuaSettings, remove),
	luamethod(LuaSettings, get_names),
	luamethod(LuaSettings, has),
	luamethod(LuaSettings, write),
	luamethod(LuaSettings, to_table),
	{0,0}
};
