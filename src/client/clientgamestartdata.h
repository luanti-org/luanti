// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 SFENCE, <sfence.software@gmail.com>

#pragma once

#include "gameparams.h"
#include "clientauth.h"

// Information processed by main menu
struct ClientGameStartData : GameParams
{
	ClientGameStartData(const GameStartData &set_start_data):
		start_data(set_start_data)
	{
	}

	bool isSinglePlayer() const { return start_data.address.empty() && !start_data.local_server; }

	GameStartData start_data;
	ClientAuth auth;
};
