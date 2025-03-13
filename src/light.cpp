// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "light.h"
#include <algorithm>
#include <cmath>
#include "util/numeric.h"
#include "settings.h"

#if CHECK_CLIENT_BUILD()

static u8 light_LUT[LIGHT_SUN + 1];

// The const ref to light_LUT is what is actually used in the code
const u8 *light_decode_table = light_LUT;


struct LightingParams {
	float a, b, c; // Lighting curve polynomial coefficients
	float boost, center, sigma; // Lighting curve parametric boost
	float gamma; // Lighting curve gamma correction
};

static LightingParams params;


float decode_light_f(float x)
{
	if (x >= 1.0f) // x is often 1.0f
		return 1.0f;
	x = std::fmax(x, 0.0f);
	float brightness = ((params.a * x + params.b) * x + params.c) * x;
	brightness += params.boost *
		std::exp(-0.5f * sqr((x - params.center) / params.sigma));
	if (brightness <= 0.0f) // May happen if parameters are extreme
		return 0.0f;
	if (brightness >= 1.0f)
		return 1.0f;
	return powf(brightness, 1.0f / params.gamma);
}


// Initialize or update the light value tables using the specified gamma
void set_light_table(float gamma)
{
// Lighting curve bounding gradients
	const float alpha = rangelim(g_settings->getFloat("lighting_alpha"), 0.0f, 3.0f);
	const float beta  = rangelim(g_settings->getFloat("lighting_beta"), 0.0f, 3.0f);
// Lighting curve polynomial coefficients
	params.a = alpha + beta - 2.0f;
	params.b = 3.0f - 2.0f * alpha - beta;
	params.c = alpha;
// Lighting curve parametric boost
	params.boost = rangelim(g_settings->getFloat("lighting_boost"), 0.0f, 0.4f);
	params.center = rangelim(g_settings->getFloat("lighting_boost_center"), 0.0f, 1.0f);
	params.sigma = rangelim(g_settings->getFloat("lighting_boost_spread"), 0.0f, 0.4f);
// Lighting curve gamma correction
	params.gamma = rangelim(gamma, 0.33f, 3.0f);

// Boundary values should be fixed
	light_LUT[0] = 0;
	light_LUT[LIGHT_SUN] = 255;

	for (size_t i = 1; i < LIGHT_SUN; i++) {
		float brightness = decode_light_f((float)i / LIGHT_SUN);
		// Strictly speaking, rangelim is not necessary here—if the implementation
		// is conforming. But we don’t want problems in any case.
		light_LUT[i] = rangelim((s32)(255.0f * brightness), 0, 255);

		// Ensure light brightens with each level
		if (i > 0 && light_LUT[i] <= light_LUT[i - 1]) {
			light_LUT[i] = std::min((u8)254, light_LUT[i - 1]) + 1;
		}
	}
}

#endif
