// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Face boundary seam handling for quad-sphere

#pragma once

#include "quadsphere.h"
#include "sphere_block_pos.h"
#include "planet_config.h"
#include <vector>

namespace quadsphere {

/**
 * Handles seamless transitions at cube face boundaries.
 *
 * When blocks are at the edge of a cube face, their neighbors may be on
 * a different face with a different orientation. This class provides
 * utilities to handle these transitions correctly for:
 * - Block neighbor lookup
 * - Mesh generation (vertices at seams)
 * - Physics (collision at boundaries)
 */
class FaceSeamHandler {
public:
	FaceSeamHandler(const PlanetConfig &config) : m_config(config) {}

	/**
	 * Check if a block is at a face boundary.
	 */
	bool isAtBoundary(const SphereBlockPos &pos) const;

	/**
	 * Check if a block is at a specific edge of its face.
	 */
	bool isAtEdge(const SphereBlockPos &pos, FaceDirection edge) const;

	/**
	 * Check if a block is at a corner (intersection of two edges).
	 */
	bool isAtCorner(const SphereBlockPos &pos) const;

	/**
	 * Get all blocks needed for mesh generation of a given block.
	 * This includes the block itself and its neighbors (which may be
	 * on different faces).
	 *
	 * @param center The block to get neighborhood for
	 * @return Vector of block positions forming the neighborhood
	 */
	std::vector<SphereBlockPos> getMeshNeighborhood(const SphereBlockPos &center) const;

	/**
	 * Get extended neighborhood for LOD stitching.
	 * Includes blocks at distance 2 for proper LOD transitions.
	 */
	std::vector<SphereBlockPos> getLODNeighborhood(const SphereBlockPos &center) const;

	/**
	 * Transform a local position within a block to account for face orientation
	 * when the position crosses into a neighboring face.
	 *
	 * @param localPos Position within block (0-16 for each axis)
	 * @param fromBlock Source block
	 * @param toBlock Destination block (on potentially different face)
	 * @return Transformed local position in destination block's orientation
	 */
	v3s16 transformLocalPosition(v3s16 localPos,
	                             const SphereBlockPos &fromBlock,
	                             const SphereBlockPos &toBlock) const;

	/**
	 * Get the rotation needed when transitioning from one face to another.
	 * Returns the number of 90-degree clockwise rotations needed.
	 *
	 * @param fromFace Source face
	 * @param toFace Destination face
	 * @param dir Direction of transition
	 * @return Rotation count (0-3)
	 */
	u8 getTransitionRotation(CubeFace fromFace, CubeFace toFace,
	                         FaceDirection dir) const;

	/**
	 * Check if a ray crosses a face boundary between two points.
	 *
	 * @param from Start position
	 * @param to End position
	 * @param[out] crossingPoint Point where boundary is crossed
	 * @return true if a boundary is crossed
	 */
	bool checkBoundaryCrossing(const SphereBlockPos &from,
	                           const SphereBlockPos &to,
	                           SphereBlockPos &crossingPoint) const;

	/**
	 * For mesh generation: get vertex positions at the seam that need
	 * to be shared between adjacent faces.
	 *
	 * @param block Block at a face edge
	 * @return Indices of shared vertices (in mesh vertex array)
	 */
	std::vector<u32> getSharedVertexIndices(const SphereBlockPos &block) const;

private:
	const PlanetConfig &m_config;

	// Rotation lookup table for face transitions
	// [fromFace][toFace] = number of 90-degree rotations
	static const u8 TRANSITION_ROTATIONS[6][6];
};

/**
 * Manages vertex welding at face seams for seamless rendering.
 *
 * When mesh vertices at face boundaries don't match exactly, visual seams
 * can appear. This class ensures vertices at seams are properly aligned.
 */
class SeamVertexWelder {
public:
	SeamVertexWelder(const PlanetConfig &config) : m_config(config) {}

	/**
	 * Weld a vertex position to match the expected position at a seam.
	 *
	 * @param vertex World-space vertex position
	 * @param block Block containing the vertex
	 * @param[out] welded Welded vertex position
	 * @return true if vertex was at a seam and was welded
	 */
	bool weldVertex(v3f vertex, const SphereBlockPos &block, v3f &welded) const;

	/**
	 * Check if a vertex position is at a face seam.
	 */
	bool isAtSeam(v3f vertex, const SphereBlockPos &block) const;

	/**
	 * Get the canonical (shared) position for a seam vertex.
	 * This ensures both faces use the exact same position.
	 */
	v3f getCanonicalPosition(v3f vertex, const SphereBlockPos &block) const;

private:
	const PlanetConfig &m_config;

	// Tolerance for vertex welding (in world units)
	static constexpr f32 WELD_TOLERANCE = 0.001f;
};

/**
 * Debug visualization for face seams.
 */
class SeamDebugRenderer {
public:
	/**
	 * Get line segments representing face boundary edges.
	 * Useful for debug overlay.
	 *
	 * @param config Planet configuration
	 * @param depth Which altitude layer to show boundaries for
	 * @return Pairs of points forming line segments
	 */
	static std::vector<std::pair<v3f, v3f>> getBoundaryLines(
		const PlanetConfig &config, s16 depth = 0);

	/**
	 * Get points at face corners.
	 */
	static std::vector<v3f> getCornerPoints(
		const PlanetConfig &config, s16 depth = 0);
};

} // namespace quadsphere
