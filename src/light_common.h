// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#pragma once

#include "irrlichttypes.h"

// Wire commands for LightSAO/LightCAO's own private message protocol
enum class LightCommand : u8
{
	LIGHT_CMD_SET_STATE,
	LIGHT_CMD_SET_PROPERTIES,
};
