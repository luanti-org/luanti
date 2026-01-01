// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Quad-sphere planet mathematics implementation

#include "quadsphere.h"
#include <algorithm>

namespace quadsphere {

// Lookup table for neighbor faces
// [face][direction] = neighbor face
static const CubeFace NEIGHBOR_FACES[6][4] = {
	// FRONT (+Z): UP=TOP, DOWN=BOTTOM, LEFT=LEFT, RIGHT=RIGHT
	{ CubeFace::TOP, CubeFace::BOTTOM, CubeFace::LEFT, CubeFace::RIGHT },
	// BACK (-Z): UP=TOP, DOWN=BOTTOM, LEFT=RIGHT, RIGHT=LEFT
	{ CubeFace::TOP, CubeFace::BOTTOM, CubeFace::RIGHT, CubeFace::LEFT },
	// RIGHT (+X): UP=TOP, DOWN=BOTTOM, LEFT=FRONT, RIGHT=BACK
	{ CubeFace::TOP, CubeFace::BOTTOM, CubeFace::FRONT, CubeFace::BACK },
	// LEFT (-X): UP=TOP, DOWN=BOTTOM, LEFT=BACK, RIGHT=FRONT
	{ CubeFace::TOP, CubeFace::BOTTOM, CubeFace::BACK, CubeFace::FRONT },
	// TOP (+Y): UP=BACK, DOWN=FRONT, LEFT=LEFT, RIGHT=RIGHT
	{ CubeFace::BACK, CubeFace::FRONT, CubeFace::LEFT, CubeFace::RIGHT },
	// BOTTOM (-Y): UP=FRONT, DOWN=BACK, LEFT=LEFT, RIGHT=RIGHT
	{ CubeFace::FRONT, CubeFace::BACK, CubeFace::LEFT, CubeFace::RIGHT }
};

v3f cubeToSphere(CubeFace face, f32 u, f32 v)
{
	// Convert UV [0,1] to cube face coordinates [-1, 1]
	f32 x2 = (u * 2.0f - 1.0f);
	f32 y2 = (v * 2.0f - 1.0f);

	// Get the cube point based on face
	v3f cubePoint;
	switch (face) {
		case CubeFace::FRONT:   // +Z
			cubePoint = v3f(x2, y2, 1.0f);
			break;
		case CubeFace::BACK:    // -Z
			cubePoint = v3f(-x2, y2, -1.0f);
			break;
		case CubeFace::RIGHT:   // +X
			cubePoint = v3f(1.0f, y2, -x2);
			break;
		case CubeFace::LEFT:    // -X
			cubePoint = v3f(-1.0f, y2, x2);
			break;
		case CubeFace::TOP:     // +Y
			cubePoint = v3f(x2, 1.0f, -y2);
			break;
		case CubeFace::BOTTOM:  // -Y
			cubePoint = v3f(x2, -1.0f, y2);
			break;
	}

	// Spherified cube projection
	// This gives more uniform distribution than simple normalization
	f32 x = cubePoint.X;
	f32 y = cubePoint.Y;
	f32 z = cubePoint.Z;

	f32 x2s = x * x;
	f32 y2s = y * y;
	f32 z2s = z * z;

	v3f spherePoint;
	spherePoint.X = x * std::sqrt(1.0f - y2s * 0.5f - z2s * 0.5f + y2s * z2s / 3.0f);
	spherePoint.Y = y * std::sqrt(1.0f - z2s * 0.5f - x2s * 0.5f + z2s * x2s / 3.0f);
	spherePoint.Z = z * std::sqrt(1.0f - x2s * 0.5f - y2s * 0.5f + x2s * y2s / 3.0f);

	return spherePoint;
}

v3f cubeToWorld(CubeFace face, f32 u, f32 v, f32 radius, f32 altitude)
{
	v3f sphereNormal = cubeToSphere(face, u, v);
	return sphereNormal * (radius + altitude);
}

void sphereToCube(v3f spherePoint, CubeFace &face, f32 &u, f32 &v)
{
	// Normalize input (in case it's not already)
	f32 len = spherePoint.getLength();
	if (len > 0.0001f) {
		spherePoint /= len;
	}

	f32 ax = std::abs(spherePoint.X);
	f32 ay = std::abs(spherePoint.Y);
	f32 az = std::abs(spherePoint.Z);

	// Determine which face based on dominant axis
	f32 x2, y2;
	if (ax >= ay && ax >= az) {
		// X-dominant: LEFT or RIGHT face
		if (spherePoint.X > 0) {
			face = CubeFace::RIGHT;
			// Project onto +X face
			x2 = -spherePoint.Z / ax;
			y2 = spherePoint.Y / ax;
		} else {
			face = CubeFace::LEFT;
			// Project onto -X face
			x2 = spherePoint.Z / ax;
			y2 = spherePoint.Y / ax;
		}
	} else if (ay >= ax && ay >= az) {
		// Y-dominant: TOP or BOTTOM face
		if (spherePoint.Y > 0) {
			face = CubeFace::TOP;
			// Project onto +Y face
			x2 = spherePoint.X / ay;
			y2 = -spherePoint.Z / ay;
		} else {
			face = CubeFace::BOTTOM;
			// Project onto -Y face
			x2 = spherePoint.X / ay;
			y2 = spherePoint.Z / ay;
		}
	} else {
		// Z-dominant: FRONT or BACK face
		if (spherePoint.Z > 0) {
			face = CubeFace::FRONT;
			// Project onto +Z face
			x2 = spherePoint.X / az;
			y2 = spherePoint.Y / az;
		} else {
			face = CubeFace::BACK;
			// Project onto -Z face
			x2 = -spherePoint.X / az;
			y2 = spherePoint.Y / az;
		}
	}

	// Convert from [-1, 1] to [0, 1]
	u = (x2 + 1.0f) * 0.5f;
	v = (y2 + 1.0f) * 0.5f;

	// Clamp to valid range
	u = std::max(0.0f, std::min(1.0f, u));
	v = std::max(0.0f, std::min(1.0f, v));
}

QuadSpherePos worldToQuadSphere(v3f worldPos, v3f center, f32 radius)
{
	v3f relPos = worldPos - center;
	f32 distance = relPos.getLength();

	QuadSpherePos result;

	if (distance < 0.0001f) {
		// At center, arbitrary face
		result.face = CubeFace::FRONT;
		result.u = 0.5f;
		result.v = 0.5f;
		result.altitude = -radius;
		return result;
	}

	v3f normalized = relPos / distance;
	sphereToCube(normalized, result.face, result.u, result.v);
	result.altitude = distance - radius;

	return result;
}

v3f getSurfaceNormal(v3f worldPos, v3f center)
{
	v3f diff = worldPos - center;
	f32 len = diff.getLength();
	if (len < 0.0001f) {
		return v3f(0, 1, 0);  // Arbitrary up at center
	}
	return diff / len;
}

v3f getGravityDirection(v3f worldPos, v3f center)
{
	return -getSurfaceNormal(worldPos, center);
}

f32 geodesicDistance(v3f pos1, v3f pos2, v3f center, f32 radius)
{
	v3f n1 = getSurfaceNormal(pos1, center);
	v3f n2 = getSurfaceNormal(pos2, center);

	// Angle between the two surface normals
	f32 dot = n1.dotProduct(n2);
	dot = std::max(-1.0f, std::min(1.0f, dot));  // Clamp for acos
	f32 angle = std::acos(dot);

	// Arc length = radius * angle
	return radius * angle;
}

CubeFace getNeighborFace(CubeFace face, FaceDirection dir)
{
	return NEIGHBOR_FACES[static_cast<u8>(face)][static_cast<u8>(dir)];
}

void transformUVAcrossFaces(CubeFace fromFace, CubeFace toFace,
                             FaceDirection dir, f32 &u, f32 &v)
{
	// This handles the coordinate transformation when crossing face boundaries.
	// The transformation depends on which faces are involved and the direction.

	f32 newU = u;
	f32 newV = v;

	// Handle transitions based on source and destination faces
	// The general pattern: when you walk off an edge, your position on the
	// new face depends on how the faces are oriented relative to each other.

	switch (fromFace) {
		case CubeFace::FRONT:
			switch (dir) {
				case FaceDirection::UP:    // -> TOP
					newU = u; newV = 1.0f;
					break;
				case FaceDirection::DOWN:  // -> BOTTOM
					newU = u; newV = 0.0f;
					break;
				case FaceDirection::LEFT:  // -> LEFT
					newU = 1.0f; newV = v;
					break;
				case FaceDirection::RIGHT: // -> RIGHT
					newU = 0.0f; newV = v;
					break;
			}
			break;

		case CubeFace::BACK:
			switch (dir) {
				case FaceDirection::UP:    // -> TOP
					newU = 1.0f - u; newV = 0.0f;
					break;
				case FaceDirection::DOWN:  // -> BOTTOM
					newU = 1.0f - u; newV = 1.0f;
					break;
				case FaceDirection::LEFT:  // -> RIGHT
					newU = 1.0f; newV = v;
					break;
				case FaceDirection::RIGHT: // -> LEFT
					newU = 0.0f; newV = v;
					break;
			}
			break;

		case CubeFace::RIGHT:
			switch (dir) {
				case FaceDirection::UP:    // -> TOP
					newU = 1.0f; newV = 1.0f - u;
					break;
				case FaceDirection::DOWN:  // -> BOTTOM
					newU = 1.0f; newV = u;
					break;
				case FaceDirection::LEFT:  // -> FRONT
					newU = 1.0f; newV = v;
					break;
				case FaceDirection::RIGHT: // -> BACK
					newU = 0.0f; newV = v;
					break;
			}
			break;

		case CubeFace::LEFT:
			switch (dir) {
				case FaceDirection::UP:    // -> TOP
					newU = 0.0f; newV = u;
					break;
				case FaceDirection::DOWN:  // -> BOTTOM
					newU = 0.0f; newV = 1.0f - u;
					break;
				case FaceDirection::LEFT:  // -> BACK
					newU = 1.0f; newV = v;
					break;
				case FaceDirection::RIGHT: // -> FRONT
					newU = 0.0f; newV = v;
					break;
			}
			break;

		case CubeFace::TOP:
			switch (dir) {
				case FaceDirection::UP:    // -> BACK
					newU = 1.0f - u; newV = 1.0f;
					break;
				case FaceDirection::DOWN:  // -> FRONT
					newU = u; newV = 1.0f;
					break;
				case FaceDirection::LEFT:  // -> LEFT
					newU = v; newV = 1.0f;
					break;
				case FaceDirection::RIGHT: // -> RIGHT
					newU = 1.0f - v; newV = 1.0f;
					break;
			}
			break;

		case CubeFace::BOTTOM:
			switch (dir) {
				case FaceDirection::UP:    // -> FRONT
					newU = u; newV = 0.0f;
					break;
				case FaceDirection::DOWN:  // -> BACK
					newU = 1.0f - u; newV = 0.0f;
					break;
				case FaceDirection::LEFT:  // -> LEFT
					newU = 1.0f - v; newV = 0.0f;
					break;
				case FaceDirection::RIGHT: // -> RIGHT
					newU = v; newV = 0.0f;
					break;
			}
			break;
	}

	u = newU;
	v = newV;
}

v3f getFaceNormal(CubeFace face)
{
	switch (face) {
		case CubeFace::FRONT:  return v3f(0, 0, 1);
		case CubeFace::BACK:   return v3f(0, 0, -1);
		case CubeFace::RIGHT:  return v3f(1, 0, 0);
		case CubeFace::LEFT:   return v3f(-1, 0, 0);
		case CubeFace::TOP:    return v3f(0, 1, 0);
		case CubeFace::BOTTOM: return v3f(0, -1, 0);
	}
	return v3f(0, 1, 0);  // Should never reach
}

void getFaceTangents(CubeFace face, v3f &tangentU, v3f &tangentV)
{
	switch (face) {
		case CubeFace::FRONT:
			tangentU = v3f(1, 0, 0);
			tangentV = v3f(0, 1, 0);
			break;
		case CubeFace::BACK:
			tangentU = v3f(-1, 0, 0);
			tangentV = v3f(0, 1, 0);
			break;
		case CubeFace::RIGHT:
			tangentU = v3f(0, 0, -1);
			tangentV = v3f(0, 1, 0);
			break;
		case CubeFace::LEFT:
			tangentU = v3f(0, 0, 1);
			tangentV = v3f(0, 1, 0);
			break;
		case CubeFace::TOP:
			tangentU = v3f(1, 0, 0);
			tangentV = v3f(0, 0, -1);
			break;
		case CubeFace::BOTTOM:
			tangentU = v3f(1, 0, 0);
			tangentV = v3f(0, 0, 1);
			break;
	}
}

const char* faceName(CubeFace face)
{
	switch (face) {
		case CubeFace::FRONT:  return "FRONT";
		case CubeFace::BACK:   return "BACK";
		case CubeFace::RIGHT:  return "RIGHT";
		case CubeFace::LEFT:   return "LEFT";
		case CubeFace::TOP:    return "TOP";
		case CubeFace::BOTTOM: return "BOTTOM";
	}
	return "UNKNOWN";
}

} // namespace quadsphere
