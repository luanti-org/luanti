// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 x2048, Dmitry Kostenko <codeforsmile@gmail.com>

#pragma once
#include "SColor.h"


/**
 * Parameters for automatic exposure compensation
 *
 * Automatic exposure compensation uses the following equation:
 *
 * wanted_exposure = 2^exposure_correction / clamp(observed_luminance, 2^luminance_min, 2^luminance_max)
 *
 */
struct AutoExposure
{
	/// @brief Minimum boundary for computed luminance
	float luminance_min;
	/// @brief Maximum boundary for computed luminance
	float luminance_max;
	/// @brief Luminance bias. Higher values make the scene darker, can be negative.
	float exposure_correction;
	/// @brief Speed of transition from dark to bright scenes
	float speed_dark_bright;
	/// @brief Speed of transition from bright to dark scenes
	float speed_bright_dark;
	/// @brief Power value for center-weighted metering. Value of 1.0 measures entire screen uniformly
	float center_weight_power;

	constexpr AutoExposure()
		: luminance_min(-3.f),
		luminance_max(-3.f),
		exposure_correction(0.0f),
		speed_dark_bright(1000.f),
		speed_bright_dark(1000.f),
		center_weight_power(1.f)
	{}
};

/** Describes ambient light settings for a player
 */
struct Lighting
{
	AutoExposure exposure;
	float shadow_intensity {0.0f};
	float saturation {1.0f};
	float volumetric_light_strength {0.0f};
	video::SColor shadow_tint {255, 0, 0, 0};
	float bloom_intensity {0.05f};
	float bloom_strength_factor {1.0f};
	float bloom_radius {1.0f};
};
