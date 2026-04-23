// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "l_clientmodchannels.h"

#include "l_internal.h"
#include "server.h"

int ModApiClientModChannelsServer::l_clientmodchannel_open(lua_State *L)
{
	std::string channel = luaL_checkstring(L, 1);
	if (channel.empty())
		return 0;
	ServerClientModChannelRef::create(L, channel);
	return 1;
}

void ModApiClientModChannelsServer::Initialize(lua_State *L, int top)
{
	API_FCT(clientmodchannel_open);
}

ServerClientModChannelRef::ServerClientModChannelRef(const std::string &channel)
	: m_channel(channel)
{
}

int ServerClientModChannelRef::l_send_all(lua_State *L)
{
	auto *ref = checkObject<ServerClientModChannelRef>(L, 1);
	if (ref->m_closed)
		return 0;
	std::string message = luaL_checkstring(L, 2);
	getServer(L)->broadcastClientModChannel(ref->m_channel, message);
	return 0;
}

int ServerClientModChannelRef::l_send_to_player(lua_State *L)
{
	auto *ref = checkObject<ServerClientModChannelRef>(L, 1);
	if (ref->m_closed) {
		lua_pushboolean(L, false);
		return 1;
	}
	std::string player_name = luaL_checkstring(L, 2);
	std::string message = luaL_checkstring(L, 3);
	bool ok = getServer(L)->sendClientModChannelToPlayer(
			ref->m_channel, player_name, message);
	lua_pushboolean(L, ok);
	return 1;
}

int ServerClientModChannelRef::l_close(lua_State *L)
{
	auto *ref = checkObject<ServerClientModChannelRef>(L, 1);
	ref->m_closed = true;
	return 0;
}

int ServerClientModChannelRef::l_get_name(lua_State *L)
{
	auto *ref = checkObject<ServerClientModChannelRef>(L, 1);
	lua_pushstring(L, ref->m_channel.c_str());
	return 1;
}

void ServerClientModChannelRef::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<ServerClientModChannelRef>(L, methods, metamethods);
}

void ServerClientModChannelRef::create(lua_State *L, const std::string &channel)
{
	ServerClientModChannelRef *o = new ServerClientModChannelRef(channel);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

int ServerClientModChannelRef::gc_object(lua_State *L)
{
	ServerClientModChannelRef *o =
			*(ServerClientModChannelRef **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}

const char ServerClientModChannelRef::className[] = "ServerClientModChannelRef";
const luaL_Reg ServerClientModChannelRef::methods[] = {
	luamethod(ServerClientModChannelRef, send_all),
	luamethod(ServerClientModChannelRef, send_to_player),
	luamethod(ServerClientModChannelRef, close),
	luamethod(ServerClientModChannelRef, get_name),
	{0, 0},
};
