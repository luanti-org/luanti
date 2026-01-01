// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Planet settings management and initialization

#pragma once

#include "planet_config.h"
#include <string>

class Settings;

namespace quadsphere {

/**
 * Planet settings keys for map metadata
 */
namespace PlanetSettings {
	// Whether planet mode is enabled for this world
	constexpr const char* ENABLED = "planet_mode";

	// Planet radius in blocks
	constexpr const char* RADIUS = "planet_radius";

	// Number of blocks per cube face edge
	constexpr const char* FACE_BLOCKS = "planet_face_blocks";

	// Maximum altitude above surface in blocks
	constexpr const char* MAX_ALTITUDE = "planet_max_altitude";

	// Maximum depth below surface in blocks
	constexpr const char* MIN_ALTITUDE = "planet_min_altitude";

	// Sea level relative to surface
	constexpr const char* SEA_LEVEL = "planet_sea_level";
}

/**
 * Load planet configuration from settings.
 *
 * @param settings Settings object to read from (typically map_meta)
 * @param[out] config Configuration to populate
 * @return true if planet mode is enabled and config was loaded
 */
bool loadPlanetConfig(const Settings *settings, PlanetConfig &config);

/**
 * Save planet configuration to settings.
 *
 * @param settings Settings object to write to
 * @param config Configuration to save
 * @param enabled Whether planet mode should be enabled
 */
void savePlanetConfig(Settings *settings, const PlanetConfig &config, bool enabled);

/**
 * Initialize global planet state from settings.
 * Sets g_planet_mode_enabled and g_planet_config.
 *
 * @param settings Settings to read from
 * @return true if planet mode was enabled
 */
bool initializePlanetMode(const Settings *settings);

/**
 * Enable planet mode with the given configuration.
 * Sets g_planet_mode_enabled = true and copies config to g_planet_config.
 *
 * @param config Configuration to use
 */
void enablePlanetMode(const PlanetConfig &config);

/**
 * Disable planet mode.
 * Sets g_planet_mode_enabled = false.
 */
void disablePlanetMode();

/**
 * Create default planet settings and add them to settings.
 * Used when creating a new planet world.
 *
 * @param settings Settings to modify
 * @param preset Which planet size preset to use ("small", "medium", "large")
 */
void createDefaultPlanetSettings(Settings *settings, const std::string &preset = "medium");

/**
 * Check if a settings object has planet mode enabled.
 */
bool isPlanetModeEnabled(const Settings *settings);

} // namespace quadsphere
