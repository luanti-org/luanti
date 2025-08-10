// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier

#include "lua_api/l_mainmenu.h"
#include "lua_api/l_internal.h"
#include "common/c_content.h"
#include "cpp_api/s_async.h"
#include "scripting_mainmenu.h"
#include "gui/guiEngine.h"
#include "gui/guiMainMenu.h"
#include "gui/guiPathSelectMenu.h"
#include "gui/touchscreeneditor.h"
#include "version.h"
#include "porting.h"
#include "filesys.h"
#include "convert_json.h"
#include "content/content.h"
#include "content/subgames.h"
#include "mapgen/mapgen.h"
#include "settings.h"
#include "clientdynamicinfo.h"
#include "client/client.h"
#include "client/renderingengine.h"
#include "client/texturepaths.h"
#include "network/networkprotocol.h"
#include "content/mod_configuration.h"
#include "threading/mutex_auto_lock.h"
#include "common/c_converter.h"
#include "gui/guiOpenURL.h"
#include "gettext.h"
#include "log.h"
#include "util/string.h"

#include <cassert>
#include <iostream>

/******************************************************************************/
std::string ModApiMainMenu::getTextData(lua_State *L, const std::string &name)
{
	lua_getglobal(L, "gamedata");

	lua_getfield(L, -1, name.c_str());

	if(lua_isnil(L, -1))
		return "";

	return luaL_checkstring(L, -1);
}

/******************************************************************************/
int ModApiMainMenu::getIntegerData(lua_State *L, const std::string &name, bool& valid)
{
	lua_getglobal(L, "gamedata");

	lua_getfield(L, -1, name.c_str());

	if(lua_isnil(L, -1)) {
		valid = false;
		return -1;
	}

	valid = true;
	return luaL_checkinteger(L, -1);
}

/******************************************************************************/
int ModApiMainMenu::getBoolData(lua_State *L, const std::string &name, bool& valid)
{
	lua_getglobal(L, "gamedata");

	lua_getfield(L, -1, name.c_str());

	if(lua_isnil(L, -1)) {
		valid = false;
		return false;
	}

	valid = true;
	return readParam<bool>(L, -1);
}

/******************************************************************************/
int ModApiMainMenu::l_update_formspec(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	if (engine->m_startgame)
		return 0;

	//read formspec
	std::string formspec(luaL_checkstring(L, 1));

	if (engine->m_formspecgui != 0) {
		engine->m_formspecgui->setForm(formspec);
	}

	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_set_formspec_prepend(lua_State *L)
{
	GUIEngine *engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	if (engine->m_startgame)
		return 0;

	std::string formspec(luaL_checkstring(L, 1));
	engine->setFormspecPrepend(formspec);

	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_start(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	//update c++ gamedata from lua table

	bool valid = false;

	MainMenuData *data = engine->m_data;

	data->selected_world = getIntegerData(L, "selected_world",valid) -1;
	data->simple_singleplayer_mode = getBoolData(L,"singleplayer",valid);
	data->do_reconnect = getBoolData(L, "do_reconnect", valid);
	if (!data->do_reconnect) {
		// Get rid of trailing whitespace in name (may be added by autocompletion
		// on Android, which would then cause SERVER_ACCESSDENIED_WRONG_CHARS_IN_NAME).
		data->name     = trim(getTextData(L, "playername"));
		data->password = getTextData(L, "password");
		// There's no reason for these to have leading/trailing whitespace either.
		data->address  = trim(getTextData(L, "address"));
		data->port     = trim(getTextData(L, "port"));

		const auto val = getTextData(L, "allow_login_or_register");
		if (val == "login")
			data->allow_login_or_register = ELoginRegister::Login;
		else if (val == "register")
			data->allow_login_or_register = ELoginRegister::Register;
		else
			data->allow_login_or_register = ELoginRegister::Any;
	}
	data->serverdescription = getTextData(L,"serverdescription");
	data->servername        = getTextData(L,"servername");

	//close menu next time
	engine->m_startgame = true;
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_close(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	engine->m_kill = true;
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_set_background(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string backgroundlevel(luaL_checkstring(L, 1));
	std::string texturename(luaL_checkstring(L, 2));

	bool tile_image = false;
	bool retval     = false;
	unsigned int minsize = 16;

	if (!lua_isnone(L, 3)) {
		tile_image = readParam<bool>(L, 3);
	}

	if (!lua_isnone(L, 4)) {
		minsize = lua_tonumber(L, 4);
	}

	if (backgroundlevel == "background") {
		retval |= engine->setTexture(TEX_LAYER_BACKGROUND, texturename,
				tile_image, minsize);
	}

	if (backgroundlevel == "overlay") {
		retval |= engine->setTexture(TEX_LAYER_OVERLAY, texturename,
				tile_image, minsize);
	}

	if (backgroundlevel == "header") {
		retval |= engine->setTexture(TEX_LAYER_HEADER,  texturename,
				tile_image, minsize);
	}

	if (backgroundlevel == "footer") {
		retval |= engine->setTexture(TEX_LAYER_FOOTER, texturename,
				tile_image, minsize);
	}

	lua_pushboolean(L,retval);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_set_clouds(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	bool value = readParam<bool>(L,1);

	engine->m_clouds_enabled = value;

	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_get_textlist_index(lua_State *L)
{
	// get_table_index accepts both tables and textlists
	return l_get_table_index(L);
}

/******************************************************************************/
int ModApiMainMenu::l_get_table_index(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string tablename(luaL_checkstring(L, 1));
	GUITable *table = engine->m_menu->getTable(tablename);
	s32 selection = table ? table->getSelected() : 0;

	if (selection >= 1)
		lua_pushinteger(L, selection);
	else
		lua_pushnil(L);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_worlds(lua_State *L)
{
	std::vector<WorldSpec> worlds = getAvailableWorlds();

	lua_newtable(L);
	int top = lua_gettop(L);
	unsigned int index = 1;

	for (const WorldSpec &world : worlds) {
		lua_pushnumber(L,index);

		lua_newtable(L);
		int top_lvl2 = lua_gettop(L);

		lua_pushstring(L,"path");
		lua_pushstring(L, world.path.c_str());
		lua_settable(L, top_lvl2);

		lua_pushstring(L,"name");
		lua_pushstring(L, world.name.c_str());
		lua_settable(L, top_lvl2);

		lua_pushstring(L,"gameid");
		lua_pushstring(L, world.gameid.c_str());
		lua_settable(L, top_lvl2);

		lua_settable(L, top);
		index++;
	}
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_games(lua_State *L)
{
	std::vector<SubgameSpec> games = getAvailableGames();

	lua_newtable(L);
	int top = lua_gettop(L);
	unsigned int index = 1;

	for (const SubgameSpec &game : games) {
		lua_pushnumber(L, index);
		lua_newtable(L);
		int top_lvl2 = lua_gettop(L);

		lua_pushstring(L,  "id");
		lua_pushstring(L,  game.id.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "path");
		lua_pushstring(L,  game.path.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "type");
		lua_pushstring(L,  "game");
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "gamemods_path");
		lua_pushstring(L,  game.gamemods_path.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "name");
		lua_pushstring(L,  game.title.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "title");
		lua_pushstring(L,  game.title.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "author");
		lua_pushstring(L,  game.author.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "release");
		lua_pushinteger(L, game.release);
		lua_settable(L,    top_lvl2);

		auto menuicon = getImagePath(game.path + DIR_DELIM "menu" DIR_DELIM "icon.png");
		lua_pushstring(L,  "menuicon_path");
		lua_pushstring(L,  menuicon.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L, "addon_mods_paths");
		lua_newtable(L);
		int table2 = lua_gettop(L);
		int internal_index = 1;
		for (const auto &addon_mods_path : game.addon_mods_paths) {
			lua_pushnumber(L, internal_index);
			lua_pushstring(L, addon_mods_path.second.c_str());
			lua_settable(L,   table2);
			internal_index++;
		}
		lua_settable(L, top_lvl2);
		lua_settable(L, top);
		index++;
	}
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_content_info(lua_State *L)
{
	std::string path = luaL_checkstring(L, 1);

	CHECK_SECURE_PATH(L, path.c_str(), false)

	ContentSpec spec;
	spec.path = path;
	parseContentInfo(spec);

	if (spec.type == "unknown") {
		// In <=5.11.0 the API call was erroneously not documented as
		// being able to return type "unknown".
		// TODO inspect call sites and make sure this is handled, then we can
		// likely remove the warning.
		warningstream << "Requested content info has type \"unknown\" "
				<< "(at " << path << ")" << std::endl;
	}

	lua_newtable(L);

	lua_pushstring(L, spec.name.c_str());
	lua_setfield(L, -2, "name");

	lua_pushstring(L, spec.title.c_str());
	lua_setfield(L, -2, "title");

	lua_pushstring(L, spec.type.c_str());
	lua_setfield(L, -2, "type");

	lua_pushstring(L, spec.author.c_str());
	lua_setfield(L, -2, "author");

	lua_pushinteger(L, spec.release);
	lua_setfield(L, -2, "release");

	lua_pushstring(L, spec.desc.c_str());
	lua_setfield(L, -2, "description");

	lua_pushstring(L, spec.path.c_str());
	lua_setfield(L, -2, "path");

	lua_pushstring(L, spec.textdomain.c_str());
	lua_setfield(L, -2, "textdomain");

	if (spec.type == "mod") {
		ModSpec spec;
		spec.path = path;
		// Since the content was already determined to be a mod,
		// the parsing is guaranteed to succeed unless the init.lua
		// file happens to be deleted between the content parse and
		// the mod parse.
		[[maybe_unused]] bool success = parseModContents(spec);
		assert(success);

		// Dependencies
		lua_newtable(L);
		int i = 1;
		for (const auto &dep : spec.depends) {
			lua_pushstring(L, dep.c_str());
			lua_rawseti(L, -2, i++);
		}
		lua_setfield(L, -2, "depends");

		// Optional Dependencies
		lua_newtable(L);
		i = 1;
		for (const auto &dep : spec.optdepends) {
			lua_pushstring(L, dep.c_str());
			lua_rawseti(L, -2, i++);
		}
		lua_setfield(L, -2, "optional_depends");
	}

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_check_mod_configuration(lua_State *L)
{
	std::string worldpath = luaL_checkstring(L, 1);

	CHECK_SECURE_PATH(L, worldpath.c_str(), false)

	ModConfiguration modmgr;

	// Add all game mods
	SubgameSpec gamespec = findWorldSubgame(worldpath);
	modmgr.addGameMods(gamespec);
	modmgr.addModsInPath(worldpath + DIR_DELIM + "worldmods", "worldmods");

	// Add user-configured mods
	std::vector<ModSpec> modSpecs;

	luaL_checktype(L, 2, LUA_TTABLE);

	lua_pushnil(L);
	while (lua_next(L, 2)) {
		// Ignore non-string keys
		if (lua_type(L, -2) != LUA_TSTRING) {
			throw LuaError(
				"Unexpected non-string key in table passed to core.check_mod_configuration");
		}

		std::string modpath = luaL_checkstring(L, -1);
		lua_pop(L, 1);
		std::string virtual_path = lua_tostring(L, -1);

		modSpecs.emplace_back();
		ModSpec &spec = modSpecs.back();
		spec.name = fs::GetFilenameFromPath(modpath.c_str());
		spec.path = modpath;
		spec.virtual_path = virtual_path;
		if (!parseModContents(spec)) {
			throw LuaError("Not a mod!");
		}
	}

	modmgr.addMods(modSpecs);
	try {
		modmgr.checkConflictsAndDeps();
	} catch (const ModError &err) {
		errorstream << err.what() << std::endl;

		lua_newtable(L);

		lua_pushboolean(L, false);
		lua_setfield(L, -2, "is_consistent");

		lua_newtable(L);
		lua_setfield(L, -2, "unsatisfied_mods");

		lua_newtable(L);
		lua_setfield(L, -2, "satisfied_mods");

		lua_pushstring(L, err.what());
		lua_setfield(L, -2, "error_message");
		return 1;
	}

	lua_newtable(L);

	lua_pushboolean(L, modmgr.isConsistent());
	lua_setfield(L, -2, "is_consistent");

	lua_newtable(L);
	int top = lua_gettop(L);
	unsigned int index = 1;
	for (const auto &spec : modmgr.getUnsatisfiedMods()) {
		lua_pushnumber(L, index);
		push_mod_spec(L, spec, true);
		lua_settable(L, top);
		index++;
	}

	lua_setfield(L, -2, "unsatisfied_mods");

	lua_newtable(L);
	top = lua_gettop(L);
	index = 1;
	for (const auto &spec : modmgr.getMods()) {
		lua_pushnumber(L, index);
		push_mod_spec(L, spec, false);
		lua_settable(L, top);
		index++;
	}
	lua_setfield(L, -2, "satisfied_mods");
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_content_translation(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string path = luaL_checkstring(L, 1);
	std::string domain = luaL_checkstring(L, 2);
	std::string string = luaL_checkstring(L, 3);
	std::string lang = gettext("LANG_CODE");
	if (lang == "LANG_CODE")
		lang = "";

	auto *translations = engine->getContentTranslations(path, domain, lang);
	string = wide_to_utf8(translate_string(utf8_to_wide(string), translations));
	lua_pushstring(L, string.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_show_touchscreen_layout(lua_State *L)
{
	GUIEngine *engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	GUITouchscreenLayout *gui = new GUITouchscreenLayout(
			engine->m_rendering_engine->get_gui_env(),
			engine->m_parent,
			-1,
			engine->m_menumanager,
			engine->m_texture_source.get());
	gui->drop();
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_create_world(lua_State *L)
{
	const char *name   = luaL_checkstring(L, 1);
	const char *gameid = luaL_checkstring(L, 2);

	StringMap use_settings;
	luaL_checktype(L, 3, LUA_TTABLE);
	lua_pushnil(L);
	while (lua_next(L, 3) != 0) {
		// key at index -2 and value at index -1
		use_settings[luaL_checkstring(L, -2)] = luaL_checkstring(L, -1);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	std::string path = porting::path_user + DIR_DELIM
			"worlds" + DIR_DELIM
			+ sanitizeDirName(name, "world_");

	std::vector<SubgameSpec> games = getAvailableGames();
	auto game_it = std::find_if(games.begin(), games.end(), [gameid] (const SubgameSpec &spec) {
		return spec.id == gameid;
	});
	if (game_it == games.end()) {
		lua_pushstring(L, "Game ID not found");
		return 1;
	}

	// Set the settings for world creation
	// this is a bad hack but the best we have right now..
	StringMap backup;
	for (auto &it : use_settings) {
		if (g_settings->existsLocal(it.first))
			backup[it.first] = g_settings->get(it.first);
		g_settings->set(it.first, it.second);
	}

	// Create world if it doesn't exist
	try {
		loadGameConfAndInitWorld(path, name, *game_it, true);
		lua_pushnil(L);
	} catch (const BaseException &e) {
		auto err = std::string("Failed to initialize world: ") + e.what();
		lua_pushstring(L, err.c_str());
	}

	// Restore previous settings
	for (auto &it : use_settings) {
		auto it2 = backup.find(it.first);
		if (it2 == backup.end())
			g_settings->remove(it.first); // wasn't set before
		else
			g_settings->set(it.first, it2->second); // was set before
	}

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_delete_world(lua_State *L)
{
	int world_id = luaL_checkinteger(L, 1) - 1;
	std::vector<WorldSpec> worlds = getAvailableWorlds();
	if (world_id < 0 || world_id >= (int) worlds.size()) {
		lua_pushstring(L, "Invalid world index");
		return 1;
	}
	const WorldSpec &spec = worlds[world_id];
	if (!fs::RecursiveDelete(spec.path)) {
		lua_pushstring(L, "Failed to delete world");
		return 1;
	}
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_set_topleft_text(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string text;

	if (!lua_isnone(L,1) &&	!lua_isnil(L,1))
		text = luaL_checkstring(L, 1);

	engine->setTopleftText(text);
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_get_mapgen_names(lua_State *L)
{
	std::vector<const char *> names;
	bool include_hidden = lua_isboolean(L, 1) && readParam<bool>(L, 1);
	Mapgen::getMapgenNames(&names, include_hidden);

	lua_newtable(L);
	for (size_t i = 0; i != names.size(); i++) {
		lua_pushstring(L, names[i]);
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}


/******************************************************************************/
int ModApiMainMenu::l_get_user_path(lua_State *L)
{
	std::string path = fs::RemoveRelativePathComponents(porting::path_user);
	lua_pushstring(L, path.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_modpath(lua_State *L)
{
	std::string modpath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "mods" + DIR_DELIM);
	lua_pushstring(L, modpath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_modpaths(lua_State *L)
{
	lua_newtable(L);

	ModApiMainMenu::l_get_modpath(L);
	lua_setfield(L, -2, "mods");

	if (porting::path_share != porting::path_user) {
		std::string modpath = fs::RemoveRelativePathComponents(
			porting::path_share + DIR_DELIM + "mods" + DIR_DELIM);
		lua_pushstring(L, modpath.c_str());
		lua_setfield(L, -2, "share");
	}

	for (const std::string &component : getEnvModPaths()) {
		lua_pushstring(L, component.c_str());
		lua_setfield(L, -2, fs::AbsolutePath(component).c_str());
	}
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_clientmodpath(lua_State *L)
{
	std::string modpath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "clientmods" + DIR_DELIM);
	lua_pushstring(L, modpath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_gamepath(lua_State *L)
{
	std::string gamepath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "games" + DIR_DELIM);
	lua_pushstring(L, gamepath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_texturepath(lua_State *L)
{
	std::string gamepath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "textures");
	lua_pushstring(L, gamepath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_texturepath_share(lua_State *L)
{
	std::string gamepath = fs::RemoveRelativePathComponents(
		porting::path_share + DIR_DELIM + "textures");
	lua_pushstring(L, gamepath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_cache_path(lua_State *L)
{
	lua_pushstring(L, fs::RemoveRelativePathComponents(porting::path_cache).c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_temp_path(lua_State *L)
{
	if (lua_isnoneornil(L, 1) || !lua_toboolean(L, 1))
		lua_pushstring(L, fs::CreateTempDir().c_str());
	else
		lua_pushstring(L, fs::CreateTempFile().c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_create_dir(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	CHECK_SECURE_PATH(L, path, true)

	lua_pushboolean(L, fs::CreateAllDirs(path));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_delete_dir(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	CHECK_SECURE_PATH(L, path, true)

	lua_pushboolean(L, fs::RecursiveDelete(path));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_copy_dir(lua_State *L)
{
	const char *source	= luaL_checkstring(L, 1);
	const char *destination	= luaL_checkstring(L, 2);

	bool keep_source = readParam<bool>(L, 3, true);

	CHECK_SECURE_PATH(L, source, !keep_source)
	CHECK_SECURE_PATH(L, destination, true)

	bool retval;
	if (keep_source)
		retval = fs::CopyDir(source, destination);
	else
		retval = fs::MoveDir(source, destination);
	lua_pushboolean(L, retval);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_is_dir(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	CHECK_SECURE_PATH(L, path, false)

	lua_pushboolean(L, fs::IsDir(path));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_extract_zip(lua_State *L)
{
	const char *zipfile	= luaL_checkstring(L, 1);
	const char *destination	= luaL_checkstring(L, 2);

	CHECK_SECURE_PATH(L, zipfile, false)
	CHECK_SECURE_PATH(L, destination, true)

	auto fs = RenderingEngine::get_raw_device()->getFileSystem();
	bool ok = fs::extractZipFile(fs, zipfile, destination);
	lua_pushboolean(L, ok);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_mainmenu_path(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	lua_pushstring(L,engine->getScriptDir().c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_may_modify_path(lua_State *L)
{
	const char *target = luaL_checkstring(L, 1);
	bool write_allowed = false;
	bool ok = ScriptApiSecurity::checkPath(L, target, false, &write_allowed);
	lua_pushboolean(L, ok && write_allowed);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_show_path_select_dialog(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	const char *formname= luaL_checkstring(L, 1);
	const char *title	= luaL_checkstring(L, 2);
	bool is_file_select = readParam<bool>(L, 3);

	GUIFileSelectMenu* fileOpenMenu =
		new GUIFileSelectMenu(engine->m_rendering_engine->get_gui_env(),
				engine->m_parent,
				-1,
				engine->m_menumanager,
				title,
				formname,
				is_file_select);
	fileOpenMenu->setTextDest(engine->m_buttonhandler);
	fileOpenMenu->drop();
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_download_file(lua_State *L)
{
	const char *url    = luaL_checkstring(L, 1);
	const char *target = luaL_checkstring(L, 2);

	CHECK_SECURE_PATH(L, target, true)

	lua_pushboolean(L, GUIEngine::downloadFile(url, target));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_language(lua_State *L)
{
	std::string lang = gettext("LANG_CODE");
	if (lang == "LANG_CODE")
		lang = "";

	lua_pushstring(L, lang.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_window_info(lua_State *L)
{
	lua_newtable(L);
	int top = lua_gettop(L);

	auto info = ClientDynamicInfo::getCurrent();

	lua_pushstring(L, "size");
	push_v2u32(L, info.render_target_size);
	lua_settable(L, top);

	lua_pushstring(L, "max_formspec_size");
	push_v2f(L, info.max_fs_size);
	lua_settable(L, top);

	lua_pushstring(L, "real_gui_scaling");
	lua_pushnumber(L, info.real_gui_scaling);
	lua_settable(L, top);

	lua_pushstring(L, "real_hud_scaling");
	lua_pushnumber(L, info.real_hud_scaling);
	lua_settable(L, top);

	lua_pushstring(L, "touch_controls");
	lua_pushboolean(L, info.touch_controls);
	lua_settable(L, top);

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_active_renderer(lua_State *L)
{
	lua_pushstring(L, RenderingEngine::get_video_driver()->getName());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_active_irrlicht_device(lua_State *L)
{
	auto device = RenderingEngine::get_raw_device();
	std::string device_name = [device] {
		switch (device->getType()) {
		case EIDT_WIN32: return "WIN32";
		case EIDT_X11: return "X11";
		case EIDT_OSX: return "OSX";
		case EIDT_SDL: return "SDL";
		case EIDT_ANDROID: return "ANDROID";
		default: return "Unknown";
		}
	}();
	if (auto version = device->getVersionString(); !version.empty())
		device_name.append(" " + version);
	lua_pushstring(L, device_name.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_min_supp_proto(lua_State *L)
{
	lua_pushinteger(L, CLIENT_PROTOCOL_VERSION_MIN);
	return 1;
}

int ModApiMainMenu::l_get_max_supp_proto(lua_State *L)
{
	lua_pushinteger(L, LATEST_PROTOCOL_VERSION);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_formspec_version(lua_State  *L)
{
	lua_pushinteger(L, FORMSPEC_API_VERSION);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_open_url(lua_State *L)
{
	std::string url = luaL_checkstring(L, 1);
	lua_pushboolean(L, porting::open_url(url));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_open_url_dialog(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string url = luaL_checkstring(L, 1);

	GUIOpenURLMenu* openURLMenu =
			new GUIOpenURLMenu(engine->m_rendering_engine->get_gui_env(),
				engine->m_parent, -1, engine->m_menumanager,
				engine->m_texture_source.get(), url);
	openURLMenu->drop();
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_open_dir(lua_State *L)
{
	const char *target = luaL_checkstring(L, 1);

	CHECK_SECURE_PATH(L, target, false)

	lua_pushboolean(L, porting::open_directory(target));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_share_file(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	CHECK_SECURE_PATH(L, path, false)

#ifdef __ANDROID__
	porting::shareFileAndroid(path);
	lua_pushboolean(L, true);
#else
	lua_pushboolean(L, false);
#endif
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_do_async_callback(lua_State *L)
{
	MainMenuScripting *script = getScriptApi<MainMenuScripting>(L);

	luaL_checktype(L, 1, LUA_TFUNCTION);
	call_string_dump(L, 1);
	size_t func_length;
	const char *serialized_func_raw = lua_tolstring(L, -1, &func_length);

	size_t param_length;
	const char* serialized_param_raw = luaL_checklstring(L, 2, &param_length);

	u32 jobId = script->queueAsync(
		std::string(serialized_func_raw, func_length),
		std::string(serialized_param_raw, param_length));

	lua_settop(L, 0);
	lua_pushinteger(L, jobId);
	return 1;
}

/******************************************************************************/
void ModApiMainMenu::Initialize(lua_State *L, int top)
{
	API_FCT(update_formspec);
	API_FCT(set_formspec_prepend);
	API_FCT(set_clouds);
	API_FCT(get_textlist_index);
	API_FCT(get_table_index);
	API_FCT(get_worlds);
	API_FCT(get_games);
	API_FCT(get_content_info);
	API_FCT(check_mod_configuration);
	API_FCT(get_content_translation);
	API_FCT(start);
	API_FCT(close);
	API_FCT(show_touchscreen_layout);
	API_FCT(create_world);
	API_FCT(delete_world);
	API_FCT(set_background);
	API_FCT(set_topleft_text);
	API_FCT(get_mapgen_names);
	API_FCT(get_user_path);
	API_FCT(get_modpath);
	API_FCT(get_modpaths);
	API_FCT(get_clientmodpath);
	API_FCT(get_gamepath);
	API_FCT(get_texturepath);
	API_FCT(get_texturepath_share);
	API_FCT(get_cache_path);
	API_FCT(get_temp_path);
	API_FCT(create_dir);
	API_FCT(delete_dir);
	API_FCT(copy_dir);
	API_FCT(is_dir);
	API_FCT(extract_zip);
	API_FCT(may_modify_path);
	API_FCT(get_mainmenu_path);
	API_FCT(show_path_select_dialog);
	API_FCT(download_file);
	API_FCT(get_language);
	API_FCT(get_window_info);
	API_FCT(get_active_renderer);
	API_FCT(get_active_irrlicht_device);
	API_FCT(get_min_supp_proto);
	API_FCT(get_max_supp_proto);
	API_FCT(get_formspec_version);
	API_FCT(open_url);
	API_FCT(open_url_dialog);
	API_FCT(open_dir);
	API_FCT(share_file);
	API_FCT(do_async_callback);

	lua_pushboolean(L, g_first_run);
	lua_setfield(L, top, "is_first_run");
}

/******************************************************************************/
void ModApiMainMenu::InitializeAsync(lua_State *L, int top)
{
	API_FCT(get_worlds);
	API_FCT(get_games);
	API_FCT(get_mapgen_names);
	API_FCT(get_user_path);
	API_FCT(get_modpath);
	API_FCT(get_modpaths);
	API_FCT(get_clientmodpath);
	API_FCT(get_gamepath);
	API_FCT(get_texturepath);
	API_FCT(get_texturepath_share);
	API_FCT(get_cache_path);
	API_FCT(get_temp_path);
	API_FCT(create_dir);
	API_FCT(delete_dir);
	API_FCT(copy_dir);
	API_FCT(is_dir);
	API_FCT(extract_zip);
	API_FCT(may_modify_path);
	API_FCT(download_file);
	API_FCT(get_min_supp_proto);
	API_FCT(get_max_supp_proto);
	API_FCT(get_formspec_version);
	API_FCT(get_language);
}
