// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 sfan5

#include "lighting.h"

#include <cstring>

bool StaticLighting::operator==(const StaticLighting &other) const
{
	if (light_curve_set != other.light_curve_set ||
		ao_gamma != other.ao_gamma)
		return false;
	if (light_curve_set && memcmp(light_curve, other.light_curve, sizeof(light_curve)) != 0)
		return false;
	return true;
}
