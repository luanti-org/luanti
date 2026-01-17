// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 SFENCE, <sfence.software@gmail.com>

#pragma once

#include "gameparams.h"
#include "clientauth.h"

// Information processed by main menu
struct ClientGameStartData : GameParams
{
	ClientGameStartData(const GameStartData &start_data):
		GameParams(start_data),
		name(start_data.name),
		address(start_data.address),
		local_server(start_data.local_server),
		allow_login_or_register(start_data.allow_login_or_register),
		world_spec(start_data.world_spec)
	{
	}

	bool isSinglePlayer() const { return address.empty() && !local_server; }

	std::string name;
	ClientAuth auth;
	std::string address;
	bool local_server;

	ELoginRegister allow_login_or_register = ELoginRegister::Any;

	// "world_path" must be kept in sync!
	WorldSpec world_spec;
};
