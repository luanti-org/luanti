// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Planet configuration for quad-sphere world

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"
#include "constants.h"

namespace quadsphere {

/**
 * Configuration parameters for a spherical planet.
 * These define the size, structure, and behavior of the planet.
 */
struct PlanetConfig {
	// Planet center in world coordinates
	// Typically (0, 0, 0) but can be offset
	v3f center = v3f(0, 0, 0);

	// Planet radius in world units (BS = 10 units per node)
	// Default: 6400 blocks * 16 nodes/block * 10 units/node = ~1,024,000 units
	// This gives a planet roughly 6.4km in radius (about 40km circumference)
	f32 radius = 6400.0f * MAP_BLOCKSIZE * BS;

	// Number of blocks along each edge of a cube face at the surface
	// Higher = more resolution, more blocks
	// 256 means 256x256 blocks per face = 65536 blocks per face
	// Total surface blocks: 6 * 256 * 256 = 393,216 blocks
	s32 face_blocks = 256;

	// Maximum altitude above surface in blocks
	// Players/blocks can exist up to this height above surface
	s32 max_altitude_blocks = 64;

	// Maximum depth below surface in blocks
	// For caves, underground structures, planet core
	s32 min_altitude_blocks = -128;

	// Sea level relative to surface (in nodes, positive = above)
	s16 sea_level = 0;

	// Gravity strength (m/s^2 equivalent, scaled by BS)
	f32 gravity = 9.81f * BS;

	// Whether to enable atmospheric effects based on altitude
	bool enable_atmosphere = true;

	// Atmosphere height in nodes (above surface where sky starts changing)
	f32 atmosphere_height = 1000.0f * BS;

	// === Derived/Computed Values ===

	/**
	 * Get the size of each block in UV space on a face.
	 * UV ranges from 0 to 1, so each block covers 1/face_blocks of the face.
	 */
	f32 getBlockUVSize() const {
		return 1.0f / static_cast<f32>(face_blocks);
	}

	/**
	 * Get the approximate size of a surface block in world units.
	 * Due to sphere projection, this varies slightly across the face.
	 */
	f32 getApproxBlockWorldSize() const {
		// Circumference of great circle / number of blocks around equator
		// Equator spans 4 faces, each with face_blocks
		f32 circumference = 2.0f * 3.14159265f * radius;
		return circumference / (4.0f * face_blocks);
	}

	/**
	 * Get the total number of altitude layers (blocks in radial direction).
	 */
	s32 getAltitudeRange() const {
		return max_altitude_blocks - min_altitude_blocks;
	}

	/**
	 * Check if an altitude (in blocks relative to surface) is valid.
	 */
	bool isValidAltitude(s32 altitude_blocks) const {
		return altitude_blocks >= min_altitude_blocks &&
		       altitude_blocks <= max_altitude_blocks;
	}

	/**
	 * Convert block altitude to world distance from center.
	 */
	f32 altitudeToRadius(s32 altitude_blocks) const {
		return radius + static_cast<f32>(altitude_blocks) * MAP_BLOCKSIZE * BS;
	}

	/**
	 * Convert world distance from center to block altitude.
	 */
	s32 radiusToAltitude(f32 world_radius) const {
		return static_cast<s32>((world_radius - radius) / (MAP_BLOCKSIZE * BS));
	}

	/**
	 * Get world position of planet surface at given UV on a face.
	 */
	v3f getSurfacePosition(CubeFace face, f32 u, f32 v) const;

	/**
	 * Check if a world position is within the valid planet volume.
	 */
	bool isWithinPlanet(v3f worldPos) const {
		f32 dist = (worldPos - center).getLength();
		f32 minR = altitudeToRadius(min_altitude_blocks);
		f32 maxR = altitudeToRadius(max_altitude_blocks);
		return dist >= minR && dist <= maxR;
	}

	// Preset configurations for different planet sizes

	/**
	 * Small planet - good for testing, quick to generate
	 * ~640m radius, ~4km circumference
	 */
	static PlanetConfig small() {
		PlanetConfig cfg;
		cfg.radius = 64.0f * MAP_BLOCKSIZE * BS;
		cfg.face_blocks = 32;
		cfg.max_altitude_blocks = 16;
		cfg.min_altitude_blocks = -32;
		return cfg;
	}

	/**
	 * Medium planet - balanced gameplay
	 * ~6.4km radius, ~40km circumference
	 */
	static PlanetConfig medium() {
		PlanetConfig cfg;
		cfg.radius = 640.0f * MAP_BLOCKSIZE * BS;
		cfg.face_blocks = 128;
		cfg.max_altitude_blocks = 32;
		cfg.min_altitude_blocks = -64;
		return cfg;
	}

	/**
	 * Large planet - expansive world
	 * ~64km radius, ~400km circumference
	 */
	static PlanetConfig large() {
		PlanetConfig cfg;
		cfg.radius = 6400.0f * MAP_BLOCKSIZE * BS;
		cfg.face_blocks = 512;
		cfg.max_altitude_blocks = 64;
		cfg.min_altitude_blocks = -128;
		return cfg;
	}
};

// Global planet configuration instance
// This will be initialized during world load
extern PlanetConfig g_planet_config;

// Check if planet mode is enabled for this world
extern bool g_planet_mode_enabled;

} // namespace quadsphere
