// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "scripting_client.h"
#include "client/client.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_client.h"
#include "lua_api/l_client_common.h"
#include "lua_api/l_env.h"
#include "lua_api/l_item.h"
#include "lua_api/l_itemstackmeta.h"
#include "lua_api/l_minimap.h"
#include "lua_api/l_modchannels.h"
#include "lua_api/l_particles_local.h"
#include "lua_api/l_storage.h"
#include "lua_api/l_util.h"
#include "lua_api/l_item.h"
#include "lua_api/l_nodemeta.h"
#include "lua_api/l_localplayer.h"
#include "lua_api/l_camera.h"
#include "lua_api/l_settings.h"
#include "lua_api/l_client_sound.h"

ClientScripting::ClientScripting(Client *client):
	ScriptApiBase(ScriptingType::Client)
{
	setGameDef(client);

	SCRIPTAPI_PRECHECKHEADER

	// Security is mandatory client side
	initializeSecurityClient();

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	lua_newtable(L);
	lua_setfield(L, -2, "ui");

	InitializeModApi(L, top);
	lua_pop(L, 1);

	// Push builtin initialization type
	lua_pushstring(L, "client");
	lua_setglobal(L, "INIT");

	infostream << "SCRIPTAPI: Initialized client game modules" << std::endl;
}

void ClientScripting::InitializeModApi(lua_State *L, int top)
{
	LuaItemStack::Register(L);
	ItemStackMetaRef::Register(L);
	LuaRaycast::Register(L);
	StorageRef::Register(L);
	LuaMinimap::Register(L);
	NodeMetaRef::RegisterClient(L);
	LuaLocalPlayer::Register(L);
	LuaCamera::Register(L);
	ModChannelRef::Register(L);
	LuaSettings::Register(L);
	ClientSoundHandle::Register(L);

	ModApiUtil::InitializeClient(L, top);
	ModApiClientCommon::Initialize(L, top);
	ModApiClient::Initialize(L, top);
	ModApiItem::InitializeClient(L, top);
	ModApiStorage::Initialize(L, top);
	ModApiEnv::InitializeClient(L, top);
	ModApiChannels::Initialize(L, top);
	ModApiParticlesLocal::Initialize(L, top);
	ModApiClientSound::Initialize(L, top);
}

void ClientScripting::on_client_ready(LocalPlayer *localplayer)
{
	LuaLocalPlayer::create(getStack(), localplayer);
}

void ClientScripting::on_camera_ready(Camera *camera)
{
	LuaCamera::create(getStack(), camera);
}

void ClientScripting::on_minimap_ready(Minimap *minimap)
{
	LuaMinimap::create(getStack(), minimap);
}
