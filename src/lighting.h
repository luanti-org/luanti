// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 x2048, Dmitry Kostenko <codeforsmile@gmail.com>

#pragma once
#include "SColor.h"
#include "irr_v3d.h"

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

/**
 * Parameters for vignette in post-processing
 *
 */
struct Vignette {
	/// @brief The darkest part of the vignette will be darkened/brightened by this factor.
	float dark = 1.0f;
	/// @brief The brightest part of the vignette will be darkened/brightened by this factor.
	float bright = 1.0f;
	/// @brief Describes the blending between dark and bright. Higher values mean darkening is more intense at the screen edges.
	float power = 1.0f;
};

/**
 * ASL CDL parameters
 *
 * Colors in ASL CDL follow the following equation:
 *
 * out = pow(in * slope + offset, power)
 *
 */
struct ColorDecisionList {
	v3f slope{1.0, 1.0, 1.0};
	v3f offset{0.0, 0.0, 0.0};
	v3f power{1.0, 1.0, 1.0};
};

/** Describes ambient light settings for a player
 */
struct Lighting
{
	AutoExposure exposure;
	Vignette vignette;
	ColorDecisionList cdl;
	float shadow_intensity {0.0f};
	float saturation {1.0f};
	float volumetric_light_strength {0.0f};
	float foliage_translucency{1.5f};
	float specular_intensity{0.0f};
	// These factors are calculated based on expected value of scattering factor of 1e-5
	// for Nitrogen at 532nm (green), 2e25 molecules/m3 in atmosphere
	v3f volumetric_beta_r0{ 3.3362176e-01f, 8.753783e-01f, 1.9534237f };
	video::SColor artificial_light_color{ 255, 133, 133, 133 };
	video::SColor shadow_tint {255, 0, 0, 0};
	float bloom_intensity {0.05f};
	float bloom_strength_factor {1.0f};
	float bloom_radius {1.0f};
};
