// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 sfan5 <sfan5@live.de>

#pragma once

#include "cpp_api/s_base.h"

struct BlockMakeData;

/*
 * Note that this is the class defining the functions called inside the emerge
 * Lua state, not the server one.
 */

class ScriptApiMapgen : virtual public ScriptApiBase
{
public:

	void on_mods_loaded();
	void on_shutdown();

	// Called after generating a piece of map, before writing it to the map
	void on_generated(BlockMakeData *bmdata, u32 seed);
};
