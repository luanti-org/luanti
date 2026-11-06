// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "settings.h"
#include "util/numeric.h"

inline float getUIAnimationSpeed()
{
	float speed = 1.0f;
	if (g_settings)
		g_settings->getFloatNoEx("ui_animation_speed", speed);
	return rangelim(speed, 0.25f, 3.0f);
}
