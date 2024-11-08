// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <iostream>

#include "common/c_types.h"
#include "common/c_internal.h"
#include "itemdef.h"


struct EnumString es_ItemType[] =
	{
		{ITEM_NONE, "none"},
		{ITEM_NODE, "node"},
		{ITEM_CRAFT, "craft"},
		{ITEM_TOOL, "tool"},
		{0, NULL},
	};

struct EnumString es_TouchInteractionMode[] =
	{
		{LONG_DIG_SHORT_PLACE, "long_dig_short_place"},
		{SHORT_DIG_LONG_PLACE, "short_dig_long_place"},
		{TouchInteractionMode_USER, "user"},
		{0, NULL},
	};
