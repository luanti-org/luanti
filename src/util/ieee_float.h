// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 SmallJoker <mk939@ymail.com>

#pragma once

#include "irrlichttypes.h"

enum FloatType
{
	FLOATTYPE_UNKNOWN,
	FLOATTYPE_SLOW,
	FLOATTYPE_SYSTEM
};

[[nodiscard]] f32 u32Tof32Slow(u32 i);
[[nodiscard]] u32 f32Tou32Slow(f32 f);

FloatType getFloatSerializationType();
