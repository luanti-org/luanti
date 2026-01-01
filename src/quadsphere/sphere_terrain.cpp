// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Sphere terrain generation utilities implementation

#include "sphere_terrain.h"
#include "constants.h"
#include <cmath>
#include <memory>

namespace quadsphere {

// Global terrain helper instance
static std::unique_ptr<SphereTerrainHelper> g_terrain_helper;

SphereTerrainHelper::SphereTerrainHelper(const PlanetConfig &config)
	: m_config(config)
{
	// Surface radius is planet radius plus sea level offset
	m_surface_radius = config.radius + config.sea_level * BS;
}

f32 SphereTerrainHelper::getAltitude(v3f pos) const
{
	v3f from_center = pos - m_config.center;
	return from_center.getLength() - m_config.radius;
}

s16 SphereTerrainHelper::getAltitudeBlocks(v3f pos) const
{
	f32 altitude = getAltitude(pos);
	return static_cast<s16>(std::floor(altitude / (MAP_BLOCKSIZE * BS)));
}

s16 SphereTerrainHelper::getGroundLevel(v3f pos) const
{
	// Ground level is at sea_level altitude (in nodes)
	// This is where Y=0 would be in flat world terms
	return m_config.sea_level;
}

bool SphereTerrainHelper::isWithinTerrainShell(v3s16 blockpos) const
{
	// Convert block position to world center
	v3f block_center(
		(blockpos.X + 0.5f) * MAP_BLOCKSIZE * BS,
		(blockpos.Y + 0.5f) * MAP_BLOCKSIZE * BS,
		(blockpos.Z + 0.5f) * MAP_BLOCKSIZE * BS
	);

	f32 altitude = getAltitude(block_center);
	f32 altitude_blocks = altitude / (MAP_BLOCKSIZE * BS);

	// Block is within terrain shell if it's between min and max altitude
	return altitude_blocks >= m_config.min_altitude_blocks - 1 &&
	       altitude_blocks <= m_config.max_altitude_blocks + 1;
}

v3f SphereTerrainHelper::getSurfaceNormal(v3f pos) const
{
	v3f from_center = pos - m_config.center;
	f32 len = from_center.getLength();
	if (len < 0.0001f)
		return v3f(0, 1, 0); // Default up if at center
	return from_center / len;
}

void SphereTerrainHelper::worldToLatLon(v3f pos, f32 &lat, f32 &lon) const
{
	v3f from_center = pos - m_config.center;
	f32 len = from_center.getLength();
	if (len < 0.0001f) {
		lat = 0;
		lon = 0;
		return;
	}

	// Normalize
	v3f dir = from_center / len;

	// Latitude from Y component (asin gives -pi/2 to pi/2)
	lat = std::asin(core::clamp(dir.Y, -1.0f, 1.0f));

	// Longitude from X and Z
	lon = std::atan2(dir.X, dir.Z);
}

v2f SphereTerrainHelper::getNoiseCoords(v3f pos) const
{
	f32 lat, lon;
	worldToLatLon(pos, lat, lon);

	// Scale to reasonable noise coordinates
	// Using radius as scale factor for consistent noise density
	f32 scale = m_config.radius / BS;
	return v2f(lon * scale, lat * scale);
}

bool SphereTerrainHelper::isBelowSurface(v3f pos, f32 noise_value, f32 variation) const
{
	f32 altitude = getAltitude(pos);

	// Surface is at sea_level, with noise variation
	f32 surface_altitude = m_config.sea_level * BS + noise_value * variation * BS;

	return altitude < surface_altitude;
}

PlanetZone getBlockZone(v3s16 blockpos)
{
	if (!g_planet_mode_enabled)
		return PlanetZone::TERRAIN_SHELL;

	// Convert block position to world center
	v3f block_center(
		(blockpos.X + 0.5f) * MAP_BLOCKSIZE * BS,
		(blockpos.Y + 0.5f) * MAP_BLOCKSIZE * BS,
		(blockpos.Z + 0.5f) * MAP_BLOCKSIZE * BS
	);

	// Calculate distance from planet center
	v3f from_center = block_center - g_planet_config.center;
	f32 distance = from_center.getLength();

	// Convert to altitude in blocks
	f32 altitude = distance - g_planet_config.radius;
	f32 altitude_blocks = altitude / (MAP_BLOCKSIZE * BS);

	// Check which zone the block is in
	if (altitude_blocks < g_planet_config.min_altitude_blocks) {
		return PlanetZone::HOLLOW_CORE;
	} else if (altitude_blocks > g_planet_config.max_altitude_blocks) {
		return PlanetZone::OUTER_SPACE;
	}

	return PlanetZone::TERRAIN_SHELL;
}

SphereTerrainHelper *getTerrainHelper()
{
	return g_terrain_helper.get();
}

void initTerrainHelper()
{
	if (g_planet_mode_enabled) {
		g_terrain_helper = std::make_unique<SphereTerrainHelper>(g_planet_config);
	}
}

void cleanupTerrainHelper()
{
	g_terrain_helper.reset();
}

} // namespace quadsphere
