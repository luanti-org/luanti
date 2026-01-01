// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Face boundary seam handling implementation

#include "face_seams.h"
#include "constants.h"
#include <cmath>

namespace quadsphere {

// Rotation lookup table for face transitions
// This encodes how coordinates rotate when moving from one face to another
// Values are number of 90-degree clockwise rotations
const u8 FaceSeamHandler::TRANSITION_ROTATIONS[6][6] = {
	// To:  FRONT  BACK  RIGHT  LEFT  TOP  BOTTOM
	/*FRONT*/  { 0, 2, 0, 0, 0, 0 },
	/*BACK*/   { 2, 0, 0, 0, 2, 2 },
	/*RIGHT*/  { 0, 0, 0, 2, 1, 3 },
	/*LEFT*/   { 0, 0, 2, 0, 3, 1 },
	/*TOP*/    { 0, 2, 3, 1, 0, 2 },
	/*BOTTOM*/ { 0, 2, 1, 3, 2, 0 }
};

bool FaceSeamHandler::isAtBoundary(const SphereBlockPos &pos) const
{
	return pos.u == 0 || pos.u == m_config.face_blocks - 1 ||
	       pos.v == 0 || pos.v == m_config.face_blocks - 1;
}

bool FaceSeamHandler::isAtEdge(const SphereBlockPos &pos, FaceDirection edge) const
{
	switch (edge) {
		case FaceDirection::LEFT:  return pos.u == 0;
		case FaceDirection::RIGHT: return pos.u == m_config.face_blocks - 1;
		case FaceDirection::DOWN:  return pos.v == 0;
		case FaceDirection::UP:    return pos.v == m_config.face_blocks - 1;
	}
	return false;
}

bool FaceSeamHandler::isAtCorner(const SphereBlockPos &pos) const
{
	bool atHorizontalEdge = (pos.u == 0 || pos.u == m_config.face_blocks - 1);
	bool atVerticalEdge = (pos.v == 0 || pos.v == m_config.face_blocks - 1);
	return atHorizontalEdge && atVerticalEdge;
}

std::vector<SphereBlockPos> FaceSeamHandler::getMeshNeighborhood(
	const SphereBlockPos &center) const
{
	std::vector<SphereBlockPos> result;
	result.reserve(27);  // 3x3x3 neighborhood

	for (s16 dz = -1; dz <= 1; ++dz) {
		for (s16 dy = -1; dy <= 1; ++dy) {
			for (s16 dx = -1; dx <= 1; ++dx) {
				SphereBlockPos neighbor = center.getNeighbor(dx, dz, dy, m_config);
				if (neighbor.isValid(m_config)) {
					result.push_back(neighbor);
				}
			}
		}
	}

	return result;
}

std::vector<SphereBlockPos> FaceSeamHandler::getLODNeighborhood(
	const SphereBlockPos &center) const
{
	std::vector<SphereBlockPos> result;
	result.reserve(125);  // 5x5x5 neighborhood

	for (s16 dz = -2; dz <= 2; ++dz) {
		for (s16 dy = -2; dy <= 2; ++dy) {
			for (s16 dx = -2; dx <= 2; ++dx) {
				SphereBlockPos neighbor = center.getNeighbor(dx, dz, dy, m_config);
				if (neighbor.isValid(m_config)) {
					result.push_back(neighbor);
				}
			}
		}
	}

	return result;
}

v3s16 FaceSeamHandler::transformLocalPosition(v3s16 localPos,
                                              const SphereBlockPos &fromBlock,
                                              const SphereBlockPos &toBlock) const
{
	if (fromBlock.face == toBlock.face) {
		// Same face, adjust for block position difference
		return localPos;
	}

	// Get rotation count for this face transition
	u8 rotations = TRANSITION_ROTATIONS[static_cast<u8>(fromBlock.face)]
	                                   [static_cast<u8>(toBlock.face)];

	v3s16 result = localPos;
	s16 maxCoord = MAP_BLOCKSIZE - 1;

	// Apply rotations (around Y axis in local space)
	for (u8 r = 0; r < rotations; ++r) {
		s16 newX = maxCoord - result.Z;
		s16 newZ = result.X;
		result.X = newX;
		result.Z = newZ;
	}

	return result;
}

u8 FaceSeamHandler::getTransitionRotation(CubeFace fromFace, CubeFace toFace,
                                          FaceDirection dir) const
{
	return TRANSITION_ROTATIONS[static_cast<u8>(fromFace)][static_cast<u8>(toFace)];
}

bool FaceSeamHandler::checkBoundaryCrossing(const SphereBlockPos &from,
                                            const SphereBlockPos &to,
                                            SphereBlockPos &crossingPoint) const
{
	if (from.face == to.face) {
		return false;  // Same face, no boundary crossing
	}

	// Find the point where the boundary is crossed
	// For now, use the midpoint as an approximation
	crossingPoint = from;

	// Determine which edge is being crossed based on face transition
	if (from.u == m_config.face_blocks - 1 || from.u == 0) {
		crossingPoint.u = (from.u == 0) ? 0 : m_config.face_blocks - 1;
		crossingPoint.v = (from.v + to.v) / 2;  // Approximate
	} else {
		crossingPoint.u = (from.u + to.u) / 2;
		crossingPoint.v = (from.v == 0) ? 0 : m_config.face_blocks - 1;
	}

	return true;
}

std::vector<u32> FaceSeamHandler::getSharedVertexIndices(
	const SphereBlockPos &block) const
{
	std::vector<u32> indices;

	// Vertices at face edges need to be shared
	// Vertex indices depend on the mesh format used

	if (isAtEdge(block, FaceDirection::LEFT)) {
		// Left edge vertices: x = 0
		for (u32 y = 0; y <= MAP_BLOCKSIZE; ++y) {
			for (u32 z = 0; z <= MAP_BLOCKSIZE; ++z) {
				indices.push_back(y * (MAP_BLOCKSIZE + 1) * (MAP_BLOCKSIZE + 1) +
				                  z * (MAP_BLOCKSIZE + 1) + 0);
			}
		}
	}

	if (isAtEdge(block, FaceDirection::RIGHT)) {
		// Right edge vertices: x = MAP_BLOCKSIZE
		for (u32 y = 0; y <= MAP_BLOCKSIZE; ++y) {
			for (u32 z = 0; z <= MAP_BLOCKSIZE; ++z) {
				indices.push_back(y * (MAP_BLOCKSIZE + 1) * (MAP_BLOCKSIZE + 1) +
				                  z * (MAP_BLOCKSIZE + 1) + MAP_BLOCKSIZE);
			}
		}
	}

	if (isAtEdge(block, FaceDirection::DOWN)) {
		// Bottom edge vertices: z = 0
		for (u32 y = 0; y <= MAP_BLOCKSIZE; ++y) {
			for (u32 x = 0; x <= MAP_BLOCKSIZE; ++x) {
				indices.push_back(y * (MAP_BLOCKSIZE + 1) * (MAP_BLOCKSIZE + 1) +
				                  0 * (MAP_BLOCKSIZE + 1) + x);
			}
		}
	}

	if (isAtEdge(block, FaceDirection::UP)) {
		// Top edge vertices: z = MAP_BLOCKSIZE
		for (u32 y = 0; y <= MAP_BLOCKSIZE; ++y) {
			for (u32 x = 0; x <= MAP_BLOCKSIZE; ++x) {
				indices.push_back(y * (MAP_BLOCKSIZE + 1) * (MAP_BLOCKSIZE + 1) +
				                  MAP_BLOCKSIZE * (MAP_BLOCKSIZE + 1) + x);
			}
		}
	}

	return indices;
}

// SeamVertexWelder implementation

bool SeamVertexWelder::weldVertex(v3f vertex, const SphereBlockPos &block,
                                  v3f &welded) const
{
	if (!isAtSeam(vertex, block)) {
		return false;
	}

	welded = getCanonicalPosition(vertex, block);
	return true;
}

bool SeamVertexWelder::isAtSeam(v3f vertex, const SphereBlockPos &block) const
{
	// Get the UV coordinates for this block
	f32 blockUVSize = m_config.getBlockUVSize();
	f32 u_min = static_cast<f32>(block.u) * blockUVSize;
	f32 u_max = (static_cast<f32>(block.u) + 1.0f) * blockUVSize;
	f32 v_min = static_cast<f32>(block.v) * blockUVSize;
	f32 v_max = (static_cast<f32>(block.v) + 1.0f) * blockUVSize;

	// Check if block is at face edge
	bool atLeftEdge = (block.u == 0);
	bool atRightEdge = (block.u == m_config.face_blocks - 1);
	bool atBottomEdge = (block.v == 0);
	bool atTopEdge = (block.v == m_config.face_blocks - 1);

	if (!atLeftEdge && !atRightEdge && !atBottomEdge && !atTopEdge) {
		return false;
	}

	// Convert vertex to sphere coordinates
	QuadSpherePos vpos = worldToQuadSphere(vertex, m_config.center, m_config.radius);

	// Check if vertex is at the edge
	bool atUEdge = (atLeftEdge && vpos.u < u_min + WELD_TOLERANCE) ||
	               (atRightEdge && vpos.u > u_max - WELD_TOLERANCE);
	bool atVEdge = (atBottomEdge && vpos.v < v_min + WELD_TOLERANCE) ||
	               (atTopEdge && vpos.v > v_max - WELD_TOLERANCE);

	return atUEdge || atVEdge;
}

v3f SeamVertexWelder::getCanonicalPosition(v3f vertex, const SphereBlockPos &block) const
{
	QuadSpherePos vpos = worldToQuadSphere(vertex, m_config.center, m_config.radius);

	// Snap UV to exact face edge
	f32 blockUVSize = m_config.getBlockUVSize();

	if (block.u == 0 && vpos.u < blockUVSize * 0.5f) {
		vpos.u = 0.0f;
	}
	if (block.u == m_config.face_blocks - 1 &&
	    vpos.u > 1.0f - blockUVSize * 0.5f) {
		vpos.u = 1.0f;
	}
	if (block.v == 0 && vpos.v < blockUVSize * 0.5f) {
		vpos.v = 0.0f;
	}
	if (block.v == m_config.face_blocks - 1 &&
	    vpos.v > 1.0f - blockUVSize * 0.5f) {
		vpos.v = 1.0f;
	}

	// Convert back to world position
	return cubeToWorld(vpos.face, vpos.u, vpos.v, m_config.radius, vpos.altitude)
	       + m_config.center;
}

// SeamDebugRenderer implementation

std::vector<std::pair<v3f, v3f>> SeamDebugRenderer::getBoundaryLines(
	const PlanetConfig &config, s16 depth)
{
	std::vector<std::pair<v3f, v3f>> lines;

	f32 altitude = static_cast<f32>(depth) * MAP_BLOCKSIZE * BS;

	// For each face, draw its 4 edges
	for (u8 f = 0; f < CUBE_FACE_COUNT; ++f) {
		CubeFace face = static_cast<CubeFace>(f);

		// 4 corners of the face
		v3f corners[4] = {
			cubeToWorld(face, 0.0f, 0.0f, config.radius, altitude) + config.center,
			cubeToWorld(face, 1.0f, 0.0f, config.radius, altitude) + config.center,
			cubeToWorld(face, 1.0f, 1.0f, config.radius, altitude) + config.center,
			cubeToWorld(face, 0.0f, 1.0f, config.radius, altitude) + config.center
		};

		// 4 edges
		lines.push_back({corners[0], corners[1]});
		lines.push_back({corners[1], corners[2]});
		lines.push_back({corners[2], corners[3]});
		lines.push_back({corners[3], corners[0]});
	}

	return lines;
}

std::vector<v3f> SeamDebugRenderer::getCornerPoints(
	const PlanetConfig &config, s16 depth)
{
	std::vector<v3f> points;

	f32 altitude = static_cast<f32>(depth) * MAP_BLOCKSIZE * BS;

	// The 8 corners of the cube (shared by 3 faces each)
	// These correspond to the corners where faces meet
	v3f cubeCorners[8] = {
		v3f(-1, -1, -1),
		v3f( 1, -1, -1),
		v3f( 1,  1, -1),
		v3f(-1,  1, -1),
		v3f(-1, -1,  1),
		v3f( 1, -1,  1),
		v3f( 1,  1,  1),
		v3f(-1,  1,  1)
	};

	for (int i = 0; i < 8; ++i) {
		// Normalize to sphere
		v3f normalized = cubeCorners[i];
		normalized.normalize();

		// Scale to radius + altitude
		v3f worldPos = normalized * (config.radius + altitude) + config.center;
		points.push_back(worldPos);
	}

	return points;
}

} // namespace quadsphere
