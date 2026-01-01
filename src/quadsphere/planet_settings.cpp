// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Planet settings management implementation

#include "planet_settings.h"
#include "sphere_terrain.h"
#include "settings.h"
#include "constants.h"

namespace quadsphere {

bool loadPlanetConfig(const Settings *settings, PlanetConfig &config)
{
	if (!settings)
		return false;

	// Check if planet mode is enabled
	bool enabled = false;
	if (!settings->getBoolNoEx(PlanetSettings::ENABLED, enabled) || !enabled)
		return false;

	// Load radius (in blocks, convert to world units)
	s32 radius_blocks = 640;
	settings->getS32NoEx(PlanetSettings::RADIUS, radius_blocks);
	config.radius = static_cast<f32>(radius_blocks) * MAP_BLOCKSIZE * BS;

	// Load face blocks
	s32 face_blocks = 128;
	settings->getS32NoEx(PlanetSettings::FACE_BLOCKS, face_blocks);
	config.face_blocks = face_blocks;

	// Load altitude limits
	s32 max_alt = 32;
	s32 min_alt = -64;
	settings->getS32NoEx(PlanetSettings::MAX_ALTITUDE, max_alt);
	settings->getS32NoEx(PlanetSettings::MIN_ALTITUDE, min_alt);
	config.max_altitude_blocks = max_alt;
	config.min_altitude_blocks = min_alt;

	// Load sea level
	s16 sea_level = 0;
	settings->getS16NoEx(PlanetSettings::SEA_LEVEL, sea_level);
	config.sea_level = sea_level;

	// Planet center is always at origin
	config.center = v3f(0, 0, 0);

	return true;
}

void savePlanetConfig(Settings *settings, const PlanetConfig &config, bool enabled)
{
	if (!settings)
		return;

	settings->setBool(PlanetSettings::ENABLED, enabled);

	if (enabled) {
		// Convert radius from world units to blocks
		s32 radius_blocks = static_cast<s32>(config.radius / (MAP_BLOCKSIZE * BS));
		settings->setS32(PlanetSettings::RADIUS, radius_blocks);

		settings->setS32(PlanetSettings::FACE_BLOCKS, config.face_blocks);
		settings->setS32(PlanetSettings::MAX_ALTITUDE, config.max_altitude_blocks);
		settings->setS32(PlanetSettings::MIN_ALTITUDE, config.min_altitude_blocks);
		settings->setS16(PlanetSettings::SEA_LEVEL, config.sea_level);
	}
}

bool initializePlanetMode(const Settings *settings)
{
	PlanetConfig config;
	if (loadPlanetConfig(settings, config)) {
		enablePlanetMode(config);
		return true;
	} else {
		disablePlanetMode();
		return false;
	}
}

void enablePlanetMode(const PlanetConfig &config)
{
	g_planet_config = config;
	g_planet_mode_enabled = true;
	initTerrainHelper();
}

void disablePlanetMode()
{
	g_planet_mode_enabled = false;
	cleanupTerrainHelper();
}

void createDefaultPlanetSettings(Settings *settings, const std::string &preset)
{
	PlanetConfig config;

	if (preset == "small") {
		config = PlanetConfig::small();
	} else if (preset == "large") {
		config = PlanetConfig::large();
	} else {
		config = PlanetConfig::medium();
	}

	savePlanetConfig(settings, config, true);
}

bool isPlanetModeEnabled(const Settings *settings)
{
	if (!settings)
		return false;

	bool enabled = false;
	settings->getBoolNoEx(PlanetSettings::ENABLED, enabled);
	return enabled;
}

} // namespace quadsphere
