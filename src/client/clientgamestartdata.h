// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 SFENCE, <sfence.software@gmail.com>

#pragma once

#include "gameparams.h"
#include "clientauth.h"

// Information processed by main menu
struct ClientGameStartData : GameParams
{
	ClientGameStartData(const GameParams &params):
		start_data(params)
	{
	}

	bool isSinglePlayer() const { return start_data.address.empty(); }

	GameStartData start_data;
	ClientAuth auth;
};
