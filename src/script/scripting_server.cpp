// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "scripting_server.h"
#include "server.h"
#include "log.h"
#include "settings.h"
#include "filesys.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_areastore.h"
#include "lua_api/l_async.h"
#include "lua_api/l_auth.h"
#include "lua_api/l_base.h"
#include "lua_api/l_craft.h"
#include "lua_api/l_env.h"
#include "lua_api/l_inventory.h"
#include "lua_api/l_item.h"
#include "lua_api/l_itemstackmeta.h"
#include "lua_api/l_mapgen.h"
#include "lua_api/l_modchannels.h"
#include "lua_api/l_nodemeta.h"
#include "lua_api/l_nodetimer.h"
#include "lua_api/l_noise.h"
#include "lua_api/l_object.h"
#include "lua_api/l_playermeta.h"
#include "lua_api/l_particles.h"
#include "lua_api/l_rollback.h"
#include "lua_api/l_server.h"
#include "lua_api/l_util.h"
#include "lua_api/l_vmanip.h"
#include "lua_api/l_settings.h"
#include "lua_api/l_http.h"
#include "lua_api/l_storage.h"
#include "lua_api/l_ipc.h"

extern "C" {
#include <lualib.h>
}

ServerScripting::ServerScripting(Server* server):
		ScriptApiBase(ScriptingType::Server),
		ScriptApiAsync(server)
{
	setGameDef(server);

	// setEnv(env) is called by ScriptApiEnv::initializeEnvironment()
	// once the environment has been created

	SCRIPTAPI_PRECHECKHEADER

	if (g_settings->getBool("secure.enable_security")) {
		initializeSecurity();
	} else {
		warningstream << "\\!/ Mod security should never be disabled, as it allows any mod to "
				<< "access the host machine."
				<< "Mods should use minetest.request_insecure_environment() instead \\!/" << std::endl;
	}

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	lua_newtable(L);
	lua_setfield(L, -2, "object_refs");

	lua_newtable(L);
	lua_setfield(L, -2, "objects_by_guid");

	lua_newtable(L);
	lua_setfield(L, -2, "luaentities");

	// Initialize our lua_api modules
	InitializeModApi(L, top);
	lua_pop(L, 1);

	// Push builtin initialization type
	lua_pushstring(L, "game");
	lua_setglobal(L, "INIT");

	infostream << "SCRIPTAPI: Initialized game modules" << std::endl;
}

void ServerScripting::loadBuiltin()
{
	loadMod(Server::getBuiltinLuaPath() + DIR_DELIM "init.lua", BUILTIN_MOD_NAME);
	checkSetByBuiltin();
}

void ServerScripting::saveGlobals()
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, "get_globals_to_transfer");
	lua_call(L, 0, 1);
	auto *data = script_pack(L, -1);
	assert(!data->contains_userdata);
	getServer()->m_lua_globals_data.reset(data);
	// unset the function
	lua_pushnil(L);
	lua_setfield(L, -3, "get_globals_to_transfer");
	lua_pop(L, 2); // pop 'core', return value
}

void ServerScripting::initAsync()
{
	infostream << "SCRIPTAPI: Initializing async engine" << std::endl;
	asyncEngine.registerStateInitializer(InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiUtil::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiCraft::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiItem::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiServer::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiIPC::Initialize);
	// not added: ModApiMapgen is a minefield for thread safety
	// not added: ModApiHttp async api can't really work together with our jobs
	// not added: ModApiStorage is probably not thread safe(?)

	asyncEngine.initialize(0);
}

void ServerScripting::InitializeModApi(lua_State *L, int top)
{
	// Register reference classes (userdata)
	InvRef::Register(L);
	ItemStackMetaRef::Register(L);
	LuaAreaStore::Register(L);
	LuaItemStack::Register(L);
	LuaValueNoise::Register(L);
	LuaValueNoiseMap::Register(L);
	LuaPseudoRandom::Register(L);
	LuaPcgRandom::Register(L);
	LuaRaycast::Register(L);
	LuaSecureRandom::Register(L);
	LuaVoxelManip::Register(L);
	NodeMetaRef::Register(L);
	NodeTimerRef::Register(L);
	ObjectRef::Register(L);
	PlayerMetaRef::Register(L);
	LuaSettings::Register(L);
	StorageRef::Register(L);
	ModChannelRef::Register(L);

	// Initialize mod api modules
	ModApiAsync::Initialize(L, top);
	ModApiAuth::Initialize(L, top);
	ModApiCraft::Initialize(L, top);
	ModApiEnv::Initialize(L, top);
	ModApiInventory::Initialize(L, top);
	ModApiItem::Initialize(L, top);
	ModApiMapgen::Initialize(L, top);
	ModApiParticles::Initialize(L, top);
	ModApiRollback::Initialize(L, top);
	ModApiServer::Initialize(L, top);
	ModApiUtil::Initialize(L, top);
	ModApiHttp::Initialize(L, top);
	ModApiStorage::Initialize(L, top);
	ModApiChannels::Initialize(L, top);
	ModApiIPC::Initialize(L, top);
}

void ServerScripting::InitializeAsync(lua_State *L, int top)
{
	// classes
	ItemStackMetaRef::Register(L);
	LuaAreaStore::Register(L);
	LuaItemStack::Register(L);
	LuaValueNoise::Register(L);
	LuaValueNoiseMap::Register(L);
	LuaPseudoRandom::Register(L);
	LuaPcgRandom::Register(L);
	LuaSecureRandom::Register(L);
	LuaVoxelManip::Register(L);
	LuaSettings::Register(L);

	// globals data
	auto *data = ModApiBase::getServer(L)->m_lua_globals_data.get();
	assert(data);
	script_unpack(L, data);
	lua_setfield(L, top, "transferred_globals");
}
