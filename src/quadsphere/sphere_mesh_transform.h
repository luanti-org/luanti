// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Sphere mesh transformation for curved planet rendering

#pragma once

#include "quadsphere.h"
#include "planet_config.h"
#include "sphere_block_pos.h"
#include "irr_v3d.h"
#include "S3DVertex.h"
#include <vector>

namespace quadsphere {

/**
 * Transforms mesh vertices from flat local coordinates to curved sphere
 * coordinates. This is the key component that makes the world look spherical.
 *
 * The transformation maps flat vertex positions to positions on the sphere
 * surface, curving them according to the planet geometry.
 */
class SphereMeshTransformer {
public:
	SphereMeshTransformer(const PlanetConfig &config, const SphereBlockPos &blockPos);

	/**
	 * Transform a single vertex from flat local coordinates to sphere world
	 * coordinates.
	 *
	 * @param localPos Position relative to block origin (in BS units)
	 * @return Transformed world position on sphere
	 */
	v3f transformPosition(v3f localPos) const;

	/**
	 * Transform a normal vector for a vertex at the given local position.
	 * Normals need to be rotated to align with the sphere's surface.
	 *
	 * @param localPos Position relative to block origin
	 * @param flatNormal Normal in flat space
	 * @return Transformed normal for sphere surface
	 */
	v3f transformNormal(v3f localPos, v3f flatNormal) const;

	/**
	 * Transform an array of vertices in-place.
	 * Modifies both position and normal of each vertex.
	 *
	 * @param vertices Array of vertices to transform
	 * @param count Number of vertices
	 */
	void transformVertices(video::S3DVertex *vertices, u32 count) const;

	/**
	 * Transform a vector of vertices in-place.
	 */
	void transformVertices(std::vector<video::S3DVertex> &vertices) const;

	/**
	 * Get the world-space position of the block center.
	 */
	v3f getBlockWorldCenter() const { return m_block_world_center; }

	/**
	 * Get the local "up" direction (away from planet center) at the block.
	 */
	v3f getLocalUp() const { return m_local_up; }

	/**
	 * Get the local coordinate frame for the block.
	 * This defines the tangent plane on the sphere surface.
	 *
	 * @param[out] right Local X axis (tangent)
	 * @param[out] up Local Y axis (normal, away from center)
	 * @param[out] forward Local Z axis (tangent)
	 */
	void getLocalFrame(v3f &right, v3f &up, v3f &forward) const;

	/**
	 * Check if sphere transformation is needed.
	 * Returns false if planet mode is disabled.
	 */
	static bool isEnabled();

private:
	const PlanetConfig &m_config;
	SphereBlockPos m_block_pos;

	// Cached values for efficient transformation
	v3f m_block_world_center;  // World position of block center
	v3f m_local_up;            // Local up direction (surface normal)
	v3f m_local_right;         // Local right direction
	v3f m_local_forward;       // Local forward direction

	// Block UV coordinates on cube face
	f32 m_block_u_min, m_block_u_max;
	f32 m_block_v_min, m_block_v_max;

	// Calculate the rotation matrix to go from flat to sphere orientation
	void calculateLocalFrame();
};

/**
 * Factory for creating mesh transformers.
 * Caches commonly used transformers for performance.
 */
class SphereMeshTransformFactory {
public:
	SphereMeshTransformFactory(const PlanetConfig &config);

	/**
	 * Get a transformer for the given block position.
	 */
	SphereMeshTransformer getTransformer(const SphereBlockPos &blockPos) const;

	/**
	 * Get a transformer for a legacy v3s16 block position.
	 */
	SphereMeshTransformer getTransformer(v3s16 legacyBlockPos) const;

private:
	const PlanetConfig &m_config;
};

/**
 * Utility functions for mesh transformation.
 */
namespace MeshTransformUtils {

/**
 * Calculate the bounding sphere for a set of transformed vertices.
 * The bounding sphere is larger on a curved surface due to the curvature.
 *
 * @param vertices Transformed vertices
 * @param count Number of vertices
 * @param center[out] Center of bounding sphere
 * @return Radius of bounding sphere
 */
f32 calculateBoundingSphere(const video::S3DVertex *vertices, u32 count,
                            v3f &center);

/**
 * Estimate the additional scale factor for bounding radius due to curvature.
 * Blocks on the sphere surface are slightly larger than their flat equivalents.
 *
 * @param blockPos Block position
 * @param config Planet configuration
 * @return Scale factor (>= 1.0)
 */
f32 estimateCurvatureScaleFactor(const SphereBlockPos &blockPos,
                                  const PlanetConfig &config);

/**
 * Check if a block is visible from a camera position, accounting for
 * the planet's horizon.
 *
 * @param blockPos Block to check
 * @param cameraWorldPos Camera world position
 * @param config Planet configuration
 * @return true if block might be visible
 */
bool isBlockAboveHorizon(const SphereBlockPos &blockPos,
                         v3f cameraWorldPos,
                         const PlanetConfig &config);

} // namespace MeshTransformUtils

} // namespace quadsphere
