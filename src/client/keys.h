// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 est31, <MTest31@outlook.com>

#pragma once

#include <list>

class KeyType
{
public:
	enum T
	{
		// Player movement
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		JUMP,
		AUX1,
		SNEAK,
		AUTOFORWARD,
		DIG,
		PLACE,

		ESC,

		// Other
		DROP,
		INVENTORY,
		CHAT,
		CMD,
		CMD_LOCAL,
		CONSOLE,
		MINIMAP,
		FREEMOVE,
		PITCHMOVE,
		FASTMOVE,
		NOCLIP,
		HOTBAR_PREV,
		HOTBAR_NEXT,
		MUTE,
		INC_VOLUME,
		DEC_VOLUME,
		CINEMATIC,
		SCREENSHOT,
		TOGGLE_BLOCK_BOUNDS,
		TOGGLE_HUD,
		TOGGLE_CHAT,
		TOGGLE_FOG,
		TOGGLE_UPDATE_CAMERA,
		TOGGLE_DEBUG,
		TOGGLE_PROFILER,
		CAMERA_MODE,
		INCREASE_VIEWING_RANGE,
		DECREASE_VIEWING_RANGE,
		RANGESELECT,
		ZOOM,

		QUICKTUNE_NEXT,
		QUICKTUNE_PREV,
		QUICKTUNE_INC,
		QUICKTUNE_DEC,

		// hotbar
		SLOT_1,
		SLOT_2,
		SLOT_3,
		SLOT_4,
		SLOT_5,
		SLOT_6,
		SLOT_7,
		SLOT_8,
		SLOT_9,
		SLOT_10,
		SLOT_11,
		SLOT_12,
		SLOT_13,
		SLOT_14,
		SLOT_15,
		SLOT_16,
		SLOT_17,
		SLOT_18,
		SLOT_19,
		SLOT_20,
		SLOT_21,
		SLOT_22,
		SLOT_23,
		SLOT_24,
		SLOT_25,
		SLOT_26,
		SLOT_27,
		SLOT_28,
		SLOT_29,
		SLOT_30,
		SLOT_31,
		SLOT_32,

		// Fake keycode for array size and internal checks
		INTERNAL_ENUM_COUNT

	};
};

typedef KeyType::T GameKeyType;
