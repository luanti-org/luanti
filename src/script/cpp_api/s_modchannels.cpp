// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "s_modchannels.h"
#include "s_internal.h"
#include "modchannels.h"

void ScriptApiModChannels::on_modchannel_message(const std::string &channel,
		const std::string &sender, const std::string &message)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_generateds
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_modchannel_message");
	// Call callbacks
	lua_pushstring(L, channel.c_str());
	lua_pushstring(L, sender.c_str());
	lua_pushstring(L, message.c_str());
	runCallbacks(3, RUN_CALLBACKS_MODE_AND);
}

void ScriptApiModChannels::on_modchannel_signal(
		const std::string &channel, ModChannelSignal signal)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_generateds
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_modchannel_signal");
	// Call callbacks
	lua_pushstring(L, channel.c_str());
	lua_pushinteger(L, (int)signal);
	runCallbacks(2, RUN_CALLBACKS_MODE_AND);
}
