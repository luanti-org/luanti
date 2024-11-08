// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_extrabloated.h"
#include "gameparams.h"
#include <string>
#include <list>

struct MainMenuDataForScript {

	MainMenuDataForScript() = default;

	// Whether the server has requested a reconnect
	bool reconnect_requested = false;
	std::string errormessage = "";
};

struct MainMenuData {
	// Client options
	std::string servername;
	std::string serverdescription;
	std::string address;
	std::string port;
	std::string name;
	std::string password;
	// Whether to reconnect
	bool do_reconnect = false;

	// Server options
	int selected_world = 0;
	bool simple_singleplayer_mode = false;

	// Data to be passed to the script
	MainMenuDataForScript script_data;

	ELoginRegister allow_login_or_register = ELoginRegister::Any;

	MainMenuData() = default;
};
