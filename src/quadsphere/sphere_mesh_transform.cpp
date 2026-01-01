// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Sphere mesh transformation implementation

#include "sphere_mesh_transform.h"
#include "constants.h"
#include <cmath>

namespace quadsphere {

SphereMeshTransformer::SphereMeshTransformer(const PlanetConfig &config,
                                             const SphereBlockPos &blockPos)
	: m_config(config), m_block_pos(blockPos)
{
	// Calculate block UV bounds
	f32 blockSize = config.getBlockUVSize();
	m_block_u_min = static_cast<f32>(blockPos.u) * blockSize;
	m_block_u_max = m_block_u_min + blockSize;
	m_block_v_min = static_cast<f32>(blockPos.v) * blockSize;
	m_block_v_max = m_block_v_min + blockSize;

	// Calculate block world center
	m_block_world_center = blockPos.getWorldCenter(config);

	// Calculate local coordinate frame
	calculateLocalFrame();
}

void SphereMeshTransformer::calculateLocalFrame()
{
	// Local up is the surface normal (pointing away from planet center)
	m_local_up = getSurfaceNormal(m_block_world_center, m_config.center);

	// Get face tangent vectors as basis for right/forward
	v3f tangentU, tangentV;
	getFaceTangents(m_block_pos.face, tangentU, tangentV);

	// Project tangent vectors onto the tangent plane of the sphere
	// at this block's location
	v3f projectedU = tangentU - m_local_up * tangentU.dotProduct(m_local_up);
	v3f projectedV = tangentV - m_local_up * tangentV.dotProduct(m_local_up);

	// Normalize
	f32 lenU = projectedU.getLength();
	f32 lenV = projectedV.getLength();

	if (lenU > 0.0001f) {
		m_local_right = projectedU / lenU;
	} else {
		// Fallback: use cross product
		m_local_right = m_local_up.crossProduct(v3f(0, 0, 1));
		if (m_local_right.getLengthSQ() < 0.0001f)
			m_local_right = m_local_up.crossProduct(v3f(1, 0, 0));
		m_local_right.normalize();
	}

	if (lenV > 0.0001f) {
		m_local_forward = projectedV / lenV;
	} else {
		m_local_forward = m_local_up.crossProduct(m_local_right);
	}

	// Ensure orthogonality
	m_local_forward = m_local_up.crossProduct(m_local_right);
	m_local_forward.normalize();
}

v3f SphereMeshTransformer::transformPosition(v3f localPos) const
{
	// Convert local position to UV coordinates within this block
	// localPos is in BS units, relative to block origin

	// Block size in BS units
	f32 blockSizeBS = MAP_BLOCKSIZE * BS;

	// Normalize to [0, 1] within block
	f32 localU = (localPos.X + blockSizeBS * 0.5f) / blockSizeBS;
	f32 localV = (localPos.Z + blockSizeBS * 0.5f) / blockSizeBS;

	// Clamp to valid range
	localU = std::max(0.0f, std::min(1.0f, localU));
	localV = std::max(0.0f, std::min(1.0f, localV));

	// Convert to face UV coordinates
	f32 faceU = m_block_u_min + localU * (m_block_u_max - m_block_u_min);
	f32 faceV = m_block_v_min + localV * (m_block_v_max - m_block_v_min);

	// Calculate altitude (Y is up in local space, becomes radial on sphere)
	// depth is in blocks, localPos.Y is in BS units relative to block center
	f32 baseAltitude = static_cast<f32>(m_block_pos.depth) * MAP_BLOCKSIZE * BS;
	f32 altitude = baseAltitude + localPos.Y;

	// Convert to world position on sphere
	return cubeToWorld(m_block_pos.face, faceU, faceV, m_config.radius, altitude)
	       + m_config.center;
}

v3f SphereMeshTransformer::transformNormal(v3f localPos, v3f flatNormal) const
{
	// Transform normal from flat space to sphere tangent space
	// The flat normal (x, y, z) maps to:
	// - x component -> local_right direction
	// - y component -> local_up direction
	// - z component -> local_forward direction

	// For a more accurate transformation at each vertex, we could
	// calculate the local frame at that specific point. For now,
	// use the block's average local frame.

	v3f sphereNormal = m_local_right * flatNormal.X +
	                   m_local_up * flatNormal.Y +
	                   m_local_forward * flatNormal.Z;

	// Normalize the result
	f32 len = sphereNormal.getLength();
	if (len > 0.0001f) {
		sphereNormal /= len;
	} else {
		sphereNormal = m_local_up;
	}

	return sphereNormal;
}

void SphereMeshTransformer::transformVertices(video::S3DVertex *vertices,
                                              u32 count) const
{
	for (u32 i = 0; i < count; ++i) {
		video::S3DVertex &v = vertices[i];

		// Store original normal before position change
		v3f originalNormal(v.Normal.X, v.Normal.Y, v.Normal.Z);

		// Transform position
		v3f localPos(v.Pos.X, v.Pos.Y, v.Pos.Z);
		v3f worldPos = transformPosition(localPos);

		// Make position relative to block world center for rendering
		// (Irrlicht uses node-relative positions)
		v3f relativePos = worldPos - m_block_world_center;

		v.Pos.X = relativePos.X;
		v.Pos.Y = relativePos.Y;
		v.Pos.Z = relativePos.Z;

		// Transform normal
		v3f sphereNormal = transformNormal(localPos, originalNormal);
		v.Normal.X = sphereNormal.X;
		v.Normal.Y = sphereNormal.Y;
		v.Normal.Z = sphereNormal.Z;
	}
}

void SphereMeshTransformer::transformVertices(
	std::vector<video::S3DVertex> &vertices) const
{
	if (!vertices.empty()) {
		transformVertices(vertices.data(), vertices.size());
	}
}

void SphereMeshTransformer::getLocalFrame(v3f &right, v3f &up, v3f &forward) const
{
	right = m_local_right;
	up = m_local_up;
	forward = m_local_forward;
}

bool SphereMeshTransformer::isEnabled()
{
	return g_planet_mode_enabled;
}

// SphereMeshTransformFactory implementation

SphereMeshTransformFactory::SphereMeshTransformFactory(const PlanetConfig &config)
	: m_config(config)
{
}

SphereMeshTransformer SphereMeshTransformFactory::getTransformer(
	const SphereBlockPos &blockPos) const
{
	return SphereMeshTransformer(m_config, blockPos);
}

SphereMeshTransformer SphereMeshTransformFactory::getTransformer(
	v3s16 legacyBlockPos) const
{
	SphereBlockPos spherePos = SphereBlockPos::fromLegacy(legacyBlockPos, m_config);
	return getTransformer(spherePos);
}

// MeshTransformUtils implementation

namespace MeshTransformUtils {

f32 calculateBoundingSphere(const video::S3DVertex *vertices, u32 count,
                            v3f &center)
{
	if (count == 0) {
		center = v3f(0, 0, 0);
		return 0.0f;
	}

	// Calculate centroid
	v3f sum(0, 0, 0);
	for (u32 i = 0; i < count; ++i) {
		sum.X += vertices[i].Pos.X;
		sum.Y += vertices[i].Pos.Y;
		sum.Z += vertices[i].Pos.Z;
	}
	center = sum / static_cast<f32>(count);

	// Find maximum distance from centroid
	f32 maxDistSq = 0.0f;
	for (u32 i = 0; i < count; ++i) {
		v3f diff(vertices[i].Pos.X - center.X,
		         vertices[i].Pos.Y - center.Y,
		         vertices[i].Pos.Z - center.Z);
		f32 distSq = diff.getLengthSQ();
		if (distSq > maxDistSq) {
			maxDistSq = distSq;
		}
	}

	return std::sqrt(maxDistSq);
}

f32 estimateCurvatureScaleFactor(const SphereBlockPos &blockPos,
                                  const PlanetConfig &config)
{
	// Blocks near face edges/corners have slightly more curvature
	// This is a simplified estimate

	f32 blockSize = config.getBlockUVSize();
	f32 centerU = (static_cast<f32>(blockPos.u) + 0.5f) * blockSize;
	f32 centerV = (static_cast<f32>(blockPos.v) + 0.5f) * blockSize;

	// Distance from face center (0.5, 0.5)
	f32 du = centerU - 0.5f;
	f32 dv = centerV - 0.5f;
	f32 distFromCenter = std::sqrt(du * du + dv * dv);

	// More curvature near edges
	f32 curvatureFactor = 1.0f + distFromCenter * 0.1f;

	// Also account for altitude - higher blocks see more of the surface
	f32 altitude = static_cast<f32>(blockPos.depth) * MAP_BLOCKSIZE * BS;
	f32 altitudeFactor = 1.0f + std::abs(altitude) / config.radius * 0.05f;

	return curvatureFactor * altitudeFactor;
}

bool isBlockAboveHorizon(const SphereBlockPos &blockPos,
                         v3f cameraWorldPos,
                         const PlanetConfig &config)
{
	v3f blockCenter = blockPos.getWorldCenter(config);

	// Camera's altitude above planet center
	v3f camRelative = cameraWorldPos - config.center;
	f32 camDist = camRelative.getLength();

	// Horizon angle from camera's position
	if (camDist <= config.radius) {
		// Camera is below or at surface, all surface blocks are visible
		return true;
	}

	f32 horizonAngle = std::acos(config.radius / camDist);

	// Angle to block center from camera
	v3f blockRelative = blockCenter - config.center;
	v3f camDir = camRelative / camDist;
	v3f blockDir = blockRelative;
	blockDir.normalize();

	f32 angleToBlock = std::acos(
		std::max(-1.0f, std::min(1.0f, camDir.dotProduct(blockDir))));

	// Block is visible if angle is less than horizon angle plus some margin
	// for block size and altitude
	f32 blockAltitude = blockRelative.getLength() - config.radius;
	f32 blockMargin = MAP_BLOCKSIZE * BS / config.radius;  // Angular size of block
	f32 altitudeMargin = blockAltitude > 0 ? std::atan2(blockAltitude, config.radius) : 0;

	return angleToBlock < (M_PI - horizonAngle) + blockMargin + altitudeMargin;
}

} // namespace MeshTransformUtils

} // namespace quadsphere
