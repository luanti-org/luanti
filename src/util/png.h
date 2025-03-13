// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 hecks

#pragma once

#include <string>
#include "irrlichttypes.h"

/*	Simple PNG encoder. Encodes an RGBA image with no predictors.
	Returns a binary string. */
std::string encodePNG(const u8 *data, u32 width, u32 height, s32 compression);
