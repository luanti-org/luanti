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

namespace {

struct LightCurve {
	float a, b, c; // Lighting curve polynomial coefficients
	float boost, center, sigma; // Lighting curve parametric boost
	float gamma; // Lighting curve gamma correction

	float calculate(float x) const;
};

float LightCurve::calculate(float x) const
{
	if (x >= 1.0f)
		return 1.0f;
	x = std::fmax(x, 0.0f);
	float brightness = ((a * x + b) * x + c) * x;
	brightness += boost *
		std::exp(-0.5f * sqr((x - center) / sigma));
	if (brightness <= 0.0f) // May happen if parameters are extreme
		return 0.0f;
	if (brightness >= 1.0f)
		return 1.0f;
	return powf(brightness, 1.0f / gamma);
}

}

void set_light_curve(float gamma)
{
	LightCurve params;
	// bounding gradients
	const float alpha = rangelim(g_settings->getFloat("lighting_alpha"), 0.0f, 3.0f);
	const float beta  = rangelim(g_settings->getFloat("lighting_beta"), 0.0f, 3.0f);
	// polynomial coefficients
	params.a = alpha + beta - 2.0f;
	params.b = 3.0f - 2.0f * alpha - beta;
	params.c = alpha;
	// parametric boost
	params.boost = rangelim(g_settings->getFloat("lighting_boost"), 0.0f, 0.4f);
	params.center = rangelim(g_settings->getFloat("lighting_boost_center"), 0.0f, 1.0f);
	params.sigma = rangelim(g_settings->getFloat("lighting_boost_spread"), 0.0f, 0.4f);
	// gamma correction
	params.gamma = rangelim(gamma, 0.33f, 3.0f);

	// Boundary values should be fixed
	light_LUT[0] = 0;
	light_LUT[LIGHT_SUN] = 255;

	for (size_t i = 1; i < LIGHT_SUN; i++) {
		float brightness = params.calculate((float)i / LIGHT_SUN);
		light_LUT[i] = std::clamp<s32>(255.0f * brightness, 0, 255);

		// Ensure light brightens with each level
		if (light_LUT[i] <= light_LUT[i - 1]) {
			light_LUT[i] = std::min<u8>(254, light_LUT[i - 1]) + 1;
		}
	}
}

void set_light_table(u8 *data, float gamma)
{
	gamma = rangelim(gamma, 0.33f, 3.0f);

	// No restrictons on values here, API users were warned to set sane values
	for (size_t i = 0; i <= LIGHT_SUN; i++) {
		float brightness = (float)data[i] / 255.0f;
		brightness = std::pow(brightness, 1.0f / gamma);
		light_LUT[i] = std::clamp<s32>(255.0f * brightness, 0, 255);
	}
}

float decode_light_f(float light_f)
{
	light_f = std::fmax(light_f, 0.0f);
	float in;
	float fr = std::modf(light_f * LIGHT_SUN, &in);
	const u32 light_i = in;
	if (light_i >= LIGHT_SUN)
		return light_LUT[LIGHT_SUN] / 255.0f;
	// interpolate two samples
	u8 l0 = light_LUT[light_i], l1 = light_LUT[light_i+1];
	float l0f = l0 / 255.0f, l1f = l1 / 255.0f;
	return l0f * (1.0f - fr) + l1f * fr;
}

#endif
