// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "content/subgames.h"

// Information provided from "main"
struct GameParams
{
	GameParams() = default;

	u16 socket_port;
	std::string world_path;
	SubgameSpec game_spec;
	bool is_dedicated_server;
};

enum class ELoginRegister {
	Any = 0,
	Login,
	Register
};

// Information processed by main menu
// TODO: unify with MainMenuData
struct GameStartData : GameParams
{
	GameStartData() = default;

	bool isSinglePlayer() const { return address.empty() && !local_server; }

	std::string name;
	std::string password;
	// If empty, we're hosting a server.
	// This may or may not be in "simple singleplayer mode".
	std::string address;
	// If true, we're hosting a server and are *not* in "simple singleplayer
	// mode".
	bool local_server;

	ELoginRegister allow_login_or_register = ELoginRegister::Any;

	// "world_path" must be kept in sync!
	WorldSpec world_spec;
};
