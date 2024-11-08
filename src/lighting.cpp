// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 x2048, Dmitry Kostenko <codeforsmile@gmail.com>

#include "lighting.h"

AutoExposure::AutoExposure()
	: luminance_min(-3.f),
	luminance_max(-3.f),
	exposure_correction(0.0f),
	speed_dark_bright(1000.f),
	speed_bright_dark(1000.f),
	center_weight_power(1.f)
{}
