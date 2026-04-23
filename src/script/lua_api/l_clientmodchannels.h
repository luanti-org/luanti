// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

// Server-side bindings for clientmod channels — the asymmetric server-mod ↔
// SSCSM RPC primitive. See doc/sscsm_api.md for the full surface; here we
// only expose `core.clientmodchannel_open(name)` which returns a userdata
// (ServerClientModChannelRef) carrying the channel name plus its publish
// methods (send_all, send_to_player, close).

class ModApiClientModChannelsServer : public ModApiBase
{
private:
	// clientmodchannel_open(name) -> ServerClientModChannelRef
	static int l_clientmodchannel_open(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};

class ServerClientModChannelRef : public ModApiBase
{
public:
	explicit ServerClientModChannelRef(const std::string &channel);
	~ServerClientModChannelRef() = default;

	static void Register(lua_State *L);
	static void create(lua_State *L, const std::string &channel);

	// :send_all(message)
	static int l_send_all(lua_State *L);
	// :send_to_player(player_name, message) -> bool
	static int l_send_to_player(lua_State *L);
	// :close()
	static int l_close(lua_State *L);
	// :get_name() -> string
	static int l_get_name(lua_State *L);

	static const char className[];

private:
	static int gc_object(lua_State *L);

	std::string m_channel;
	bool m_closed = false;

	static const luaL_Reg methods[];
};
