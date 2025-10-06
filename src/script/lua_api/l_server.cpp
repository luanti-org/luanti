// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_server.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "common/c_packer.h"
#include "cpp_api/s_base.h"
#include "cpp_api/s_security.h"
#include "scripting_server.h"
#include "server.h"
#include "environment.h"
#include "remoteplayer.h"
#include "log.h"
#include "filesys.h"
#include <algorithm>

// request_shutdown()
int ModApiServer::l_request_shutdown(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *msg = lua_tolstring(L, 1, NULL);
	bool reconnect = readParam<bool>(L, 2);
	float seconds_before_shutdown = lua_tonumber(L, 3);
	getServer(L)->requestShutdown(msg ? msg : "", reconnect, seconds_before_shutdown);
	return 0;
}

// get_server_status()
int ModApiServer::l_get_server_status(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushstring(L, getServer(L)->getStatusString().c_str());
	return 1;
}

// get_server_uptime()
int ModApiServer::l_get_server_uptime(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushnumber(L, getServer(L)->getUptime());
	return 1;
}

// get_server_max_lag()
int ModApiServer::l_get_server_max_lag(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;
	lua_pushnumber(L, env->getMaxLagEstimate());
	return 1;
}

// print(text)
int ModApiServer::l_print(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string text;
	text = luaL_checkstring(L, 1);
	getServer(L)->printToConsoleOnly(text);
	return 0;
}

// chat_send_all(text)
int ModApiServer::l_chat_send_all(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *text = luaL_checkstring(L, 1);
	// Get server from registry
	Server *server = getServer(L);
	// Send
	try {
		server->notifyPlayers(utf8_to_wide(text));
	} catch (PacketError &e) {
		warningstream << "Exception caught: " << e.what() << std::endl
			<< script_get_backtrace(L) << std::endl;
		server->notifyPlayers(utf8_to_wide(std::string("Internal error: ") + e.what()));
	}

	return 0;
}

// chat_send_player(name, text)
int ModApiServer::l_chat_send_player(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *name = luaL_checkstring(L, 1);
	const char *text = luaL_checkstring(L, 2);

	// Get server from registry
	Server *server = getServer(L);
	// Send
	try {
		server->notifyPlayer(name, utf8_to_wide(text));
	} catch (PacketError &e) {
		warningstream << "Exception caught: " << e.what() << std::endl
			<< script_get_backtrace(L) << std::endl;
		server->notifyPlayer(name, utf8_to_wide(std::string("Internal error: ") + e.what()));
	}
	return 0;
}

// get_player_privs(name, text)
int ModApiServer::l_get_player_privs(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *name = luaL_checkstring(L, 1);
	// Get server from registry
	Server *server = getServer(L);
	// Do it
	lua_newtable(L);
	int table = lua_gettop(L);
	std::set<std::string> privs_s = server->getPlayerEffectivePrivs(name);
	for (const std::string &privs_ : privs_s) {
		lua_pushboolean(L, true);
		lua_setfield(L, table, privs_.c_str());
	}
	lua_pushvalue(L, table);
	return 1;
}

// get_player_ip()
int ModApiServer::l_get_player_ip(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	const char *name = luaL_checkstring(L, 1);
	RemotePlayer *player = env->getPlayer(name);
	if (!player) {
		lua_pushnil(L); // no such player
		return 1;
	}

	lua_pushstring(L, env->getGameDef()->getPeerAddress(
		player->getPeerId()).serializeString().c_str()
	);
	return 1;
}

// get_player_information(name)
int ModApiServer::l_get_player_information(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	const char *name = luaL_checkstring(L, 1);
	RemotePlayer *player = env->getPlayer(name);
	if (!player) {
		lua_pushnil(L); // no such player
		return 1;
	}

	Server *server = env->getGameDef();
	ClientInfo info;
	if (!server->getClientInfo(player->getPeerId(), info)) {
		warningstream << FUNCTION_NAME << ": no client info?!" << std::endl;
		lua_pushnil(L); // error
		return 1;
	}

	lua_newtable(L);
	int table = lua_gettop(L);

	lua_pushstring(L,"address");
	lua_pushstring(L, info.addr.serializeString().c_str());
	lua_settable(L, table);

	lua_pushstring(L,"ip_version");
	if (info.addr.getFamily() == AF_INET) {
		lua_pushnumber(L, 4);
	} else if (info.addr.getFamily() == AF_INET6) {
		lua_pushnumber(L, 6);
	} else {
		lua_pushnumber(L, 0);
	}
	lua_settable(L, table);

	/*
		Be careful not to introduce a depdendency on the connection to
		the peer here. This function is >>REQUIRED<< to still be able to return
		values even when the peer unexpectedly disappears.
		Hence all the ConInfo values here are optional.
	*/

	auto getConInfo = [&] (con::rtt_stat_type type, float *value) -> bool {
		return server->getClientConInfo(player->getPeerId(), type, value);
	};

	float min_rtt, max_rtt, avg_rtt, min_jitter, max_jitter, avg_jitter;
	bool have_con_info =
		getConInfo(con::MIN_RTT, &min_rtt) &&
		getConInfo(con::MAX_RTT, &max_rtt) &&
		getConInfo(con::AVG_RTT, &avg_rtt) &&
		getConInfo(con::MIN_JITTER, &min_jitter) &&
		getConInfo(con::MAX_JITTER, &max_jitter) &&
		getConInfo(con::AVG_JITTER, &avg_jitter);

	if (have_con_info) { // may be missing
		lua_pushstring(L, "min_rtt");
		lua_pushnumber(L, min_rtt);
		lua_settable(L, table);

		lua_pushstring(L, "max_rtt");
		lua_pushnumber(L, max_rtt);
		lua_settable(L, table);

		lua_pushstring(L, "avg_rtt");
		lua_pushnumber(L, avg_rtt);
		lua_settable(L, table);

		lua_pushstring(L, "min_jitter");
		lua_pushnumber(L, min_jitter);
		lua_settable(L, table);

		lua_pushstring(L, "max_jitter");
		lua_pushnumber(L, max_jitter);
		lua_settable(L, table);

		lua_pushstring(L, "avg_jitter");
		lua_pushnumber(L, avg_jitter);
		lua_settable(L, table);
	}

	lua_pushstring(L,"connection_uptime");
	lua_pushnumber(L, info.uptime);
	lua_settable(L, table);

	lua_pushstring(L,"protocol_version");
	lua_pushnumber(L, info.prot_vers);
	lua_settable(L, table);

	lua_pushstring(L, "formspec_version");
	lua_pushnumber(L, player->formspec_version);
	lua_settable(L, table);

	lua_pushstring(L, "lang_code");
	lua_pushstring(L, info.lang_code.c_str());
	lua_settable(L, table);

	lua_pushstring(L, "version_string");
	lua_pushstring(L, info.vers_string.c_str());
	lua_settable(L, table);

#ifndef NDEBUG
	lua_pushstring(L,"serialization_version");
	lua_pushnumber(L, info.ser_vers);
	lua_settable(L, table);

	lua_pushstring(L,"major");
	lua_pushnumber(L, info.major);
	lua_settable(L, table);

	lua_pushstring(L,"minor");
	lua_pushnumber(L, info.minor);
	lua_settable(L, table);

	lua_pushstring(L,"patch");
	lua_pushnumber(L, info.patch);
	lua_settable(L, table);

	lua_pushstring(L,"state");
	lua_pushstring(L, ClientInterface::state2Name(info.state));
	lua_settable(L, table);
#endif

	return 1;
}

// get_player_window_information(name)
int ModApiServer::l_get_player_window_information(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	const char *name = luaL_checkstring(L, 1);
	RemotePlayer *player = env->getPlayer(name);
	if (!player)
		return 0;

	Server *server = env->getGameDef();
	auto dynamic = server->getClientDynamicInfo(player->getPeerId());

	if (!dynamic || dynamic->render_target_size == v2u32())
		return 0;

	lua_newtable(L);
	int dyn_table = lua_gettop(L);

	lua_pushstring(L, "size");
	push_v2u32(L, dynamic->render_target_size);
	lua_settable(L, dyn_table);

	lua_pushstring(L, "max_formspec_size");
	push_v2f(L, dynamic->max_fs_size);
	lua_settable(L, dyn_table);

	lua_pushstring(L, "real_gui_scaling");
	lua_pushnumber(L, dynamic->real_gui_scaling);
	lua_settable(L, dyn_table);

	lua_pushstring(L, "real_hud_scaling");
	lua_pushnumber(L, dynamic->real_hud_scaling);
	lua_settable(L, dyn_table);

	lua_pushstring(L, "touch_controls");
	lua_pushboolean(L, dynamic->touch_controls);
	lua_settable(L, dyn_table);

	return 1;
}

// get_ban_list()
int ModApiServer::l_get_ban_list(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushstring(L, getServer(L)->getBanDescription("").c_str());
	return 1;
}

// get_ban_description()
int ModApiServer::l_get_ban_description(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char * ip_or_name = luaL_checkstring(L, 1);
	lua_pushstring(L, getServer(L)->getBanDescription(std::string(ip_or_name)).c_str());
	return 1;
}

// ban_player()
int ModApiServer::l_ban_player(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	const char *name = luaL_checkstring(L, 1);
	RemotePlayer *player = env->getPlayer(name);
	if (!player) {
		lua_pushboolean(L, false); // no such player
		return 1;
	}

	Server *server = env->getGameDef();
	std::string ip_str = server->getPeerAddress(player->getPeerId()).serializeString();
	server->setIpBanned(ip_str, name);
	lua_pushboolean(L, true);
	return 1;
}

// disconnect_player(name[, reason[, reconnect]]) -> success
int ModApiServer::l_disconnect_player(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	const char *name = luaL_checkstring(L, 1);
	std::string message;
	if (lua_isstring(L, 2))
		message.append(readParam<std::string>(L, 2));
	else
		message.append("Disconnected.");

	RemotePlayer *player = env->getPlayer(name);
	if (!player) {
		lua_pushboolean(L, false); // No such player
		return 1;
	}

	bool reconnect = readParam<bool>(L, 3, false);

	Server *server = env->getGameDef();
	server->DenyAccess(player->getPeerId(), SERVER_ACCESSDENIED_CUSTOM_STRING, message, reconnect);
	lua_pushboolean(L, true);
	return 1;
}

int ModApiServer::l_remove_player(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;
	std::string name = luaL_checkstring(L, 1);

	RemotePlayer *player = env->getPlayer(name.c_str());
	if (!player)
		lua_pushinteger(L, env->removePlayerFromDatabase(name) ? 0 : 1);
	else
		lua_pushinteger(L, 2);

	return 1;
}

// unban_player_or_ip()
int ModApiServer::l_unban_player_or_ip(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char * ip_or_name = luaL_checkstring(L, 1);
	getServer(L)->unsetIpBanned(ip_or_name);
	lua_pushboolean(L, true);
	return 1;
}

// show_formspec(playername,formname,formspec)
int ModApiServer::l_show_formspec(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *playername = luaL_checkstring(L, 1);
	const char *formname = luaL_checkstring(L, 2);
	const char *formspec = luaL_checkstring(L, 3);
	lua_pushboolean(L, getServer(L)->showFormspec(playername,formspec,formname));
	return 1;
}

// get_current_modname()
int ModApiServer::l_get_current_modname(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string s = ScriptApiBase::getCurrentModNameInsecure(L);
	if (!s.empty())
		lua_pushstring(L, s.c_str());
	else
		lua_pushnil(L);
	return 1;
}

// get_modpath(modname)
int ModApiServer::l_get_modpath(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string modname = luaL_checkstring(L, 1);
	const ModSpec *mod = getGameDef(L)->getModSpec(modname);
	if (!mod)
		lua_pushnil(L);
	else
		lua_pushstring(L, mod->path.c_str());
	return 1;
}

// get_modnames()
// the returned list is sorted alphabetically for you
int ModApiServer::l_get_modnames(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// Get a list of mods
	std::vector<std::string> modlist;
	for (auto &it : getGameDef(L)->getMods())
		modlist.emplace_back(it.name);

	std::sort(modlist.begin(), modlist.end());

	// Package them up for Lua
	lua_createtable(L, modlist.size(), 0);
	auto iter = modlist.begin();
	for (u16 i = 0; iter != modlist.end(); ++iter) {
		lua_pushstring(L, iter->c_str());
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

// get_game_info()
int ModApiServer::l_get_game_info(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const SubgameSpec *game_spec = getGameDef(L)->getGameSpec();
	assert(game_spec);
	lua_newtable(L);
	setstringfield(L, -1, "id", game_spec->id);
	setstringfield(L, -1, "title", game_spec->title);
	setstringfield(L, -1, "author", game_spec->author);
	setstringfield(L, -1, "path", game_spec->path);
	return 1;
}

// get_worldpath()
int ModApiServer::l_get_worldpath(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const Server *srv = getServer(L);
	lua_pushstring(L, srv->getWorldPath().c_str());
	return 1;
}

// get_mod_data_path()
int ModApiServer::l_get_mod_data_path(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string modname = ScriptApiBase::getCurrentModNameInsecure(L);
	if (modname.empty())
		return 0;

	const Server *srv = getServer(L);
	std::string path = srv->getModDataPath() + DIR_DELIM + modname;
	if (!fs::CreateAllDirs(path))
		throw LuaError("Failed to create dir");

	lua_pushstring(L, path.c_str());
	return 1;
}

// sound_play(spec, parameters, [ephemeral])
int ModApiServer::l_sound_play(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ServerPlayingSound params;
	read_simplesoundspec(L, 1, params.spec);
	read_server_sound_params(L, 2, params);
	bool ephemeral = lua_gettop(L) > 2 && readParam<bool>(L, 3);
	if (ephemeral) {
		getServer(L)->playSound(params, true);
		lua_pushnil(L);
	} else {
		s32 handle = getServer(L)->playSound(params);
		lua_pushinteger(L, handle);
	}
	return 1;
}

// sound_stop(handle)
int ModApiServer::l_sound_stop(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	s32 handle = luaL_checkinteger(L, 1);
	getServer(L)->stopSound(handle);
	return 0;
}

int ModApiServer::l_sound_fade(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	s32 handle = luaL_checkinteger(L, 1);
	float step = readParam<float>(L, 2);
	float gain = readParam<float>(L, 3);
	getServer(L)->fadeSound(handle, step, gain);
	return 0;
}

// dynamic_add_media(filepath)
int ModApiServer::l_dynamic_add_media(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	Server *server = getServer(L);
	const bool at_startup = !getEnv(L);

	std::string tmp;
	Server::DynamicMediaArgs args;

	if (lua_istable(L, 1)) {
		getstringfield(L, 1, "filename", args.filename);
		if (getstringfield(L, 1, "filepath", tmp))
			args.filepath = tmp;
		args.data.emplace();
		if (!getstringfield(L, 1, "filedata", *args.data))
			args.data.reset();
		getstringfield(L, 1, "to_player", args.to_player);
		getboolfield(L, 1, "ephemeral", args.ephemeral);
		args.client_cache = getboolfield_default(L, 1, "client_cache", !args.ephemeral);
	} else {
		tmp = readParam<std::string>(L, 1);
		args.filepath = tmp;
		log_deprecated(L, "Deprecated call to core.dynamic_add_media() with string argument", 1, true);
	}
	if (at_startup) {
		if (!lua_isnoneornil(L, 2))
			throw LuaError("must be called without callback at load-time");
		// In order to keep edge cases to a minimum actually use an empty function.
		int err = luaL_loadstring(L, "");
		SANITY_CHECK(err == 0);
		lua_replace(L, 2);
	} else {
		luaL_checktype(L, 2, LUA_TFUNCTION);
	}

	// validate
	if (args.filepath) {
		if (args.filepath->empty())
			throw LuaError("filepath must be non-empty");
		if (args.data)
			throw LuaError("cannot provide both filepath and filedata");
	} else if (args.data) {
		if (args.filename.empty())
			throw LuaError("filename required");
	} else {
		throw LuaError("either filepath or filedata must be provided");
	}

	if (args.filepath)
		CHECK_SECURE_PATH(L, args.filepath->c_str(), false);

	args.token = server->getScriptIface()->allocateDynamicMediaCallback(L, 2);

	bool ok = server->dynamicAddMedia(args);
	if (!ok)
		server->getScriptIface()->freeDynamicMediaCallback(args.token);
	lua_pushboolean(L, ok);

	return 1;
}

// is_singleplayer()
int ModApiServer::l_is_singleplayer(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const Server *srv = getServer(L);
	lua_pushboolean(L, srv->isSingleplayer());
	return 1;
}

// notify_authentication_modified(name)
int ModApiServer::l_notify_authentication_modified(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string name;
	if(lua_isstring(L, 1))
		name = readParam<std::string>(L, 1);
	getServer(L)->reportPrivsModified(name);
	return 0;
}

// register_async_dofile(path)
int ModApiServer::l_register_async_dofile(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string path = readParam<std::string>(L, 1);
	CHECK_SECURE_PATH(L, path.c_str(), false);

	std::string modname = ScriptApiBase::getCurrentModNameInsecure(L);
	if (modname.empty())
		throw ModError("cannot determine mod name");

	getServer(L)->m_async_init_files.emplace_back(modname, path);
	lua_pushboolean(L, true);
	return 1;
}

// register_mapgen_script(path)
int ModApiServer::l_register_mapgen_script(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string path = readParam<std::string>(L, 1);
	CHECK_SECURE_PATH(L, path.c_str(), false);

	std::string modname = ScriptApiBase::getCurrentModNameInsecure(L);
	if (modname.empty())
		throw ModError("cannot determine mod name");

	getServer(L)->m_mapgen_init_files.emplace_back(modname, path);
	lua_pushboolean(L, true);
	return 1;
}

// serialize_roundtrip(value)
// Meant for unit testing the packer from Lua
int ModApiServer::l_serialize_roundtrip(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	int top = lua_gettop(L);
	auto *pv = script_pack(L, 1);
	if (top != lua_gettop(L))
		throw LuaError("stack values leaked");

#ifndef NDEBUG
	script_dump_packed(pv);
#endif

	top = lua_gettop(L);
	script_unpack(L, pv);
	delete pv;
	if (top + 1 != lua_gettop(L))
		throw LuaError("stack values leaked");

	return 1;
}

void ModApiServer::Initialize(lua_State *L, int top)
{
	API_FCT(request_shutdown);
	API_FCT(get_server_status);
	API_FCT(get_server_uptime);
	API_FCT(get_server_max_lag);
	API_FCT(get_mod_data_path);
	API_FCT(get_worldpath);
	API_FCT(is_singleplayer);

	API_FCT(get_current_modname);
	API_FCT(get_modpath);
	API_FCT(get_modnames);
	API_FCT(get_game_info);

	API_FCT(print);

	API_FCT(chat_send_all);
	API_FCT(chat_send_player);
	API_FCT(show_formspec);
	API_FCT(sound_play);
	API_FCT(sound_stop);
	API_FCT(sound_fade);
	API_FCT(dynamic_add_media);

	API_FCT(get_player_information);
	API_FCT(get_player_window_information);
	API_FCT(get_player_privs);
	API_FCT(get_player_ip);
	API_FCT(get_ban_list);
	API_FCT(get_ban_description);
	API_FCT(ban_player);
	API_FCT(disconnect_player);
	API_FCT(remove_player);
	API_FCT(unban_player_or_ip);
	API_FCT(notify_authentication_modified);

	API_FCT(register_async_dofile);
	API_FCT(serialize_roundtrip);

	API_FCT(register_mapgen_script);
}

void ModApiServer::InitializeAsync(lua_State *L, int top)
{
	API_FCT(get_worldpath);
	API_FCT(is_singleplayer);

	API_FCT(get_current_modname);
	API_FCT(get_modpath);
	API_FCT(get_modnames);
	API_FCT(get_game_info);
}
