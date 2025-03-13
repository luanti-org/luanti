// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once
#include <cassert>
#include "config.h"
#include "irrlichttypes.h"

/*
	Lower level lighting stuff
*/

// This directly sets the range of light.
// Actually this is not the real maximum, and this is not the brightest, the
// brightest is LIGHT_SUN.
// If changed, this constant as defined in builtin/game/constants.lua must
// also be changed.
#define LIGHT_MAX 14
// Light is stored as 4 bits, thus 15 is the maximum.
// This brightness is reserved for sunlight
#define LIGHT_SUN 15

#if IS_CLIENT_BUILD

/**
 * \internal
 *
 * \warning DO NOT USE this directly; it is here simply so that decode_light()
 * can be inlined.
 *
 * Array size is #LIGHTMAX+1
 *
 * The array is a lookup table to convert the internal representation of light
 * (brightness) to the display brightness.
 *
 */
extern const u8 *light_decode_table;

// 0 <= light <= LIGHT_SUN
// 0 <= return value <= 255
inline u8 decode_light(u8 light)
{
	// assert(light <= LIGHT_SUN);
	if (light > LIGHT_SUN)
		light = LIGHT_SUN;
	return light_decode_table[light];
}

// 0.0 <= light <= 1.0
// 0.0 <= return value <= 1.0
float decode_light_f(float light_f);

void set_light_table(float gamma);

#endif

// 0 <= daylight_factor <= 1000
// 0 <= lightday, lightnight <= LIGHT_SUN
// 0 <= return value <= LIGHT_SUN
inline u8 blend_light(u32 daylight_factor, u8 lightday, u8 lightnight)
{
	u32 c = 1000;
	u32 l = ((daylight_factor * lightday + (c - daylight_factor) * lightnight)) / c;
	if (l > LIGHT_SUN)
		l = LIGHT_SUN;
	return l;
}
