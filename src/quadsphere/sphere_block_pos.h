// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Sphere-aware block position addressing

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"
#include "quadsphere.h"
#include "planet_config.h"
#include <functional>

namespace quadsphere {

/**
 * Block position on a spherical planet.
 *
 * Unlike flat worlds that use (x, y, z) coordinates, sphere blocks are
 * addressed by:
 * - face: Which of the 6 cube faces (0-5)
 * - u, v: Position on the face grid (0 to face_blocks-1)
 * - depth: Altitude layer (-min_altitude to +max_altitude, 0 = surface)
 *
 * This allows efficient storage and lookup while maintaining the
 * spherical topology.
 */
struct SphereBlockPos {
	CubeFace face;   // Which cube face (0-5)
	s16 u;           // U position on face (0 to face_blocks-1)
	s16 v;           // V position on face (0 to face_blocks-1)
	s16 depth;       // Altitude layer (0 = surface, + = up, - = down)

	SphereBlockPos()
		: face(CubeFace::FRONT), u(0), v(0), depth(0) {}

	SphereBlockPos(CubeFace f, s16 u_, s16 v_, s16 d)
		: face(f), u(u_), v(v_), depth(d) {}

	// Equality operators
	bool operator==(const SphereBlockPos &other) const {
		return face == other.face && u == other.u &&
		       v == other.v && depth == other.depth;
	}

	bool operator!=(const SphereBlockPos &other) const {
		return !(*this == other);
	}

	// Comparison for sorting/maps
	bool operator<(const SphereBlockPos &other) const {
		if (face != other.face) return face < other.face;
		if (depth != other.depth) return depth < other.depth;
		if (v != other.v) return v < other.v;
		return u < other.u;
	}

	/**
	 * Convert to legacy v3s16 position for compatibility with existing code.
	 *
	 * Layout in v3s16 space:
	 * - X: u + face_offset_x (faces laid out horizontally)
	 * - Y: depth (altitude)
	 * - Z: v + face_offset_z
	 *
	 * Face layout (looking from above):
	 *           [TOP]
	 *   [LEFT] [FRONT] [RIGHT] [BACK]
	 *          [BOTTOM]
	 */
	v3s16 toLegacy(const PlanetConfig &config) const;

	/**
	 * Convert from legacy v3s16 position.
	 */
	static SphereBlockPos fromLegacy(v3s16 pos, const PlanetConfig &config);

	/**
	 * Get the world-space center position of this block.
	 */
	v3f getWorldCenter(const PlanetConfig &config) const;

	/**
	 * Get the world-space position of a node within this block.
	 * @param nodeOffset Offset within block (0-15 for each axis)
	 */
	v3f getNodeWorldPos(v3s16 nodeOffset, const PlanetConfig &config) const;

	/**
	 * Get the UV coordinates for the center of this block.
	 */
	void getUV(const PlanetConfig &config, f32 &out_u, f32 &out_v) const;

	/**
	 * Get neighboring block position, handling face transitions.
	 *
	 * Direction meanings relative to the face:
	 * - +U: Right on face
	 * - -U: Left on face
	 * - +V: Up on face
	 * - -V: Down on face
	 * - +Depth: Away from planet center
	 * - -Depth: Toward planet center
	 */
	SphereBlockPos getNeighbor(s16 du, s16 dv, s16 ddepth,
	                           const PlanetConfig &config) const;

	/**
	 * Get the 6 face neighbors (not including depth neighbors).
	 */
	void getFaceNeighbors(const PlanetConfig &config,
	                      SphereBlockPos neighbors[4]) const;

	/**
	 * Check if this block position is valid within the planet config.
	 */
	bool isValid(const PlanetConfig &config) const;

	/**
	 * Create from a world position.
	 */
	static SphereBlockPos fromWorldPos(v3f worldPos, const PlanetConfig &config);

	/**
	 * Get a unique hash for this position (for use in hash maps).
	 */
	size_t hash() const;

	/**
	 * Human-readable string representation.
	 */
	std::string toString() const;
};

/**
 * Iterator for blocks within a geodesic range of a center position.
 * Used for block loading, rendering culling, etc.
 */
class SphereBlockIterator {
public:
	using Callback = std::function<void(const SphereBlockPos&)>;

	/**
	 * Iterate over all blocks within a geodesic distance of the center.
	 *
	 * @param center Center position
	 * @param range Maximum geodesic distance (along surface)
	 * @param config Planet configuration
	 * @param callback Called for each block in range
	 */
	static void iterateInRange(const SphereBlockPos &center,
	                          f32 range,
	                          const PlanetConfig &config,
	                          const Callback &callback);

	/**
	 * Iterate over all blocks in a spherical shell (at a specific depth).
	 *
	 * @param face Which face to iterate (or iterate all if face == -1)
	 * @param depth Depth layer to iterate
	 * @param config Planet configuration
	 * @param callback Called for each block
	 */
	static void iterateShell(s8 face, s16 depth,
	                        const PlanetConfig &config,
	                        const Callback &callback);

	/**
	 * Get approximate number of blocks within a range.
	 * Useful for pre-allocating containers.
	 */
	static size_t estimateBlockCount(f32 range, const PlanetConfig &config);
};

/**
 * Utility to calculate distances and directions on the sphere.
 */
class SphereGeometry {
public:
	/**
	 * Calculate geodesic (surface) distance between two block positions.
	 */
	static f32 geodesicDistance(const SphereBlockPos &a,
	                            const SphereBlockPos &b,
	                            const PlanetConfig &config);

	/**
	 * Calculate Euclidean (straight-line) distance between two block positions.
	 */
	static f32 euclideanDistance(const SphereBlockPos &a,
	                             const SphereBlockPos &b,
	                             const PlanetConfig &config);

	/**
	 * Get the direction vector from one block to another (in world space).
	 */
	static v3f directionTo(const SphereBlockPos &from,
	                       const SphereBlockPos &to,
	                       const PlanetConfig &config);

	/**
	 * Check if block b is visible from block a (not occluded by planet).
	 * Simple horizon check.
	 */
	static bool isVisible(const SphereBlockPos &from,
	                      const SphereBlockPos &to,
	                      const PlanetConfig &config);
};

} // namespace quadsphere

// Hash function for use in std::unordered_map
namespace std {
	template<>
	struct hash<quadsphere::SphereBlockPos> {
		size_t operator()(const quadsphere::SphereBlockPos &pos) const {
			return pos.hash();
		}
	};
}
