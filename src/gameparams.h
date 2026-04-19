// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "content/subgames.h"

// Information provided from "main"
// Start information for server or client
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

struct GameClientData
{
	bool isSinglePlayer() const { return mode == GM_SINGLEPLAYER; }
	bool isAnyServer() const
	{ return mode == GM_HOST_AND_JOIN || mode == GM_SINGLEPLAYER; }

	std::string name;
	std::string password;
	std::string address; //< non-empty when joining a server

	enum Mode {
		GM_SINGLEPLAYER,
		GM_HOST_AND_JOIN,
		GM_JOIN,
		GM_Undefined
	} mode = GM_Undefined;

	ELoginRegister allow_login_or_register = ELoginRegister::Any;
};

// Information provided by ClientLauncher
// Client-only data (joining or hosting server)
struct GameStartData : GameParams, GameClientData
{
	GameStartData() = default;

	// "world_path" must be kept in sync!
	WorldSpec world_spec;
};
