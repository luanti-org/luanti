// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "config.h"
#include "content/mods.h"
#include "json-forwards.h"
#include <iostream>

#pragma once

// Note that client serverlist handling is all in Lua, this is only announcements now.

namespace ServerList
{
#if USE_CURL
enum AnnounceAction {AA_START, AA_UPDATE, AA_DELETE};
void sendAnnounce(AnnounceAction, u16 port,
		const std::vector<std::string> &clients_names = std::vector<std::string>(),
		double uptime = 0, u32 game_time = 0, float lag = 0,
		const std::string &gameid = "", const std::string &mg_name = "",
		const std::vector<ModSpec> &mods = std::vector<ModSpec>(),
		bool dedicated = false);
#endif

}
