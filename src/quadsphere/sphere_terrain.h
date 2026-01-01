// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Sphere terrain generation utilities

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"
#include "planet_config.h"
#include "quadsphere.h"

namespace quadsphere {

/**
 * Utilities for sphere-aware terrain generation.
 * Provides coordinate transformations that allow existing mapgen
 * algorithms to work on a spherical planet.
 */
class SphereTerrainHelper {
public:
	explicit SphereTerrainHelper(const PlanetConfig &config);

	/**
	 * Get the altitude (distance from planet center) for a world position.
	 * @param pos World position
	 * @return Altitude in world units (positive = above center)
	 */
	f32 getAltitude(v3f pos) const;

	/**
	 * Get the altitude in blocks for a world position.
	 * @param pos World position
	 * @return Altitude in blocks
	 */
	s16 getAltitudeBlocks(v3f pos) const;

	/**
	 * Get terrain generation ground level at a position.
	 * This is the altitude where terrain surface should be.
	 * @param pos World position
	 * @return Ground level altitude in nodes
	 */
	s16 getGroundLevel(v3f pos) const;

	/**
	 * Check if a block position is within the planet's terrain shell.
	 * Blocks outside this shell should be skipped or filled with air/void.
	 * @param blockpos Block position
	 * @return true if block should be generated
	 */
	bool isWithinTerrainShell(v3s16 blockpos) const;

	/**
	 * Get the surface normal at a world position.
	 * This is the "up" direction at that point.
	 * @param pos World position
	 * @return Normalized surface normal
	 */
	v3f getSurfaceNormal(v3f pos) const;

	/**
	 * Transform world XZ coordinates to spherical latitude/longitude.
	 * This provides continuous coordinates for noise sampling.
	 * @param pos World position
	 * @param lat Output latitude (-pi/2 to pi/2)
	 * @param lon Output longitude (-pi to pi)
	 */
	void worldToLatLon(v3f pos, f32 &lat, f32 &lon) const;

	/**
	 * Get noise sampling coordinates for a world position.
	 * These coordinates are continuous across the sphere for seamless noise.
	 * @param pos World position
	 * @return 2D coordinates suitable for noise sampling
	 */
	v2f getNoiseCoords(v3f pos) const;

	/**
	 * Check if a node position is below the terrain surface.
	 * Uses spherical coordinates and altitude.
	 * @param pos Node position in world coords
	 * @param noise_value Noise value at this position (-1 to 1)
	 * @param variation Maximum terrain variation height
	 * @return true if position should be solid (below surface)
	 */
	bool isBelowSurface(v3f pos, f32 noise_value, f32 variation = 32.0f) const;

private:
	PlanetConfig m_config;
	f32 m_surface_radius; // Radius at which terrain surface is centered
};

/**
 * Get the terrain helper if planet mode is enabled.
 * @return Pointer to terrain helper, or nullptr if planet mode is disabled
 */
SphereTerrainHelper *getTerrainHelper();

/**
 * Initialize the global terrain helper with current planet config.
 * Should be called when planet mode is enabled.
 */
void initTerrainHelper();

/**
 * Clean up the global terrain helper.
 */
void cleanupTerrainHelper();

} // namespace quadsphere
