// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "gameparams.h"
#include <string>

struct MainMenuData : GameClientData {
	MainMenuData(GameErrorData &errordata) :
		script_data(errordata)
	{}

	// Client options
	std::string port; // TODO combine into GameClientData

	// Whether to reconnect
	bool do_reconnect = false;

	// Server options
	int selected_world = 0;

	// Data to be passed to the script
	GameErrorData &script_data;
};
