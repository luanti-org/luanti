// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "lua_api/l_base.h"
#include "config.h"

class ModChannel;

class ModApiChannels : public ModApiBase
{
private:
	// mod_channel_join(name)
	static int l_mod_channel_join(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};

class ModChannelRef : public ModApiBase
{
public:
	ModChannelRef(const std::string &modchannel);
	~ModChannelRef() = default;

	static void Register(lua_State *L);
	static void create(lua_State *L, const std::string &channel);

	// leave()
	static int l_leave(lua_State *L);

	// send(message)
	static int l_send_all(lua_State *L);

	// is_writeable()
	static int l_is_writeable(lua_State *L);

	static const char className[];

private:
	// garbage collector
	static int gc_object(lua_State *L);

	static ModChannel *getobject(lua_State *L, ModChannelRef *ref);

	std::string m_modchannel_name;

	static const luaL_Reg methods[];
};
