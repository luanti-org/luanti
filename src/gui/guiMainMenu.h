// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "gameparams.h"
#include <string>

/// Data provided to Lua for error reporting by the main menu
struct MainMenuDataForScript {

	MainMenuDataForScript() = default;

	// Whether the server has requested a reconnect
	bool reconnect_requested = false;
	std::string errormessage;
};

struct MainMenuData : GameClientData {
	MainMenuData() = default;

	// Client options
	std::string port; // TODO combine into GameClientData

	// Whether to reconnect
	bool do_reconnect = false;

	// Server options
	int selected_world = 0;

	// Data to be passed to the script
	MainMenuDataForScript script_data;
};
