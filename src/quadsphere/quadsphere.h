// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Quad-sphere planet mathematics

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"
#include <cmath>

namespace quadsphere {

// The six faces of the cube that maps to a sphere
enum class CubeFace : u8 {
	FRONT = 0,   // +Z face
	BACK = 1,    // -Z face
	RIGHT = 2,   // +X face
	LEFT = 3,    // -X face
	TOP = 4,     // +Y face
	BOTTOM = 5   // -Y face
};

// Number of cube faces
constexpr u8 CUBE_FACE_COUNT = 6;

// Direction for face neighbor lookup
enum class FaceDirection : u8 {
	UP = 0,
	DOWN = 1,
	LEFT = 2,
	RIGHT = 3
};

// Position on the quad-sphere surface
struct QuadSpherePos {
	CubeFace face;    // Which cube face
	f32 u;            // U coordinate on face [0, 1]
	f32 v;            // V coordinate on face [0, 1]
	f32 altitude;     // Height above (positive) or below (negative) sphere surface

	QuadSpherePos() : face(CubeFace::FRONT), u(0.5f), v(0.5f), altitude(0.0f) {}
	QuadSpherePos(CubeFace f, f32 u_, f32 v_, f32 alt = 0.0f)
		: face(f), u(u_), v(v_), altitude(alt) {}
};

/**
 * Convert cube face UV coordinates to a point on the unit sphere.
 * Uses the "spherified cube" projection for uniform distribution.
 *
 * @param face Which cube face
 * @param u U coordinate on face [0, 1]
 * @param v V coordinate on face [0, 1]
 * @return Normalized point on unit sphere
 */
v3f cubeToSphere(CubeFace face, f32 u, f32 v);

/**
 * Convert cube face UV to point on sphere with given radius and altitude.
 *
 * @param face Which cube face
 * @param u U coordinate on face [0, 1]
 * @param v V coordinate on face [0, 1]
 * @param radius Sphere radius
 * @param altitude Height above/below sphere surface
 * @return World position
 */
v3f cubeToWorld(CubeFace face, f32 u, f32 v, f32 radius, f32 altitude = 0.0f);

/**
 * Convert a world position to quad-sphere coordinates.
 *
 * @param worldPos Position in world space
 * @param center Planet center position
 * @param radius Planet radius
 * @return QuadSpherePos with face, UV, and altitude
 */
QuadSpherePos worldToQuadSphere(v3f worldPos, v3f center, f32 radius);

/**
 * Convert a point on the unit sphere to the cube face it belongs to
 * and its UV coordinates on that face.
 *
 * @param spherePoint Normalized point on unit sphere
 * @param[out] face Which cube face
 * @param[out] u U coordinate on face [0, 1]
 * @param[out] v V coordinate on face [0, 1]
 */
void sphereToCube(v3f spherePoint, CubeFace &face, f32 &u, f32 &v);

/**
 * Get the surface normal at a point on the sphere.
 * This is the direction pointing away from the planet center.
 *
 * @param worldPos Position in world space
 * @param center Planet center position
 * @return Normalized surface normal (local "up" direction)
 */
v3f getSurfaceNormal(v3f worldPos, v3f center);

/**
 * Get the gravity direction at a point (toward planet center).
 *
 * @param worldPos Position in world space
 * @param center Planet center position
 * @return Normalized gravity direction (opposite of surface normal)
 */
v3f getGravityDirection(v3f worldPos, v3f center);

/**
 * Calculate geodesic distance along sphere surface between two points.
 *
 * @param pos1 First world position
 * @param pos2 Second world position
 * @param center Planet center
 * @param radius Planet radius
 * @return Arc length distance along sphere surface
 */
f32 geodesicDistance(v3f pos1, v3f pos2, v3f center, f32 radius);

/**
 * Get the neighboring face when moving in a direction off the edge of a face.
 *
 * @param face Current face
 * @param dir Direction of movement
 * @return The adjacent face
 */
CubeFace getNeighborFace(CubeFace face, FaceDirection dir);

/**
 * Transform UV coordinates when transitioning from one face to another.
 * Handles the rotation/mirroring needed at face boundaries.
 *
 * @param fromFace Source face
 * @param toFace Destination face
 * @param dir Direction of transition
 * @param u U coordinate (modified in place)
 * @param v V coordinate (modified in place)
 */
void transformUVAcrossFaces(CubeFace fromFace, CubeFace toFace,
                             FaceDirection dir, f32 &u, f32 &v);

/**
 * Check if UV coordinates are within the valid [0, 1] range for a face.
 */
inline bool isValidUV(f32 u, f32 v) {
	return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
}

/**
 * Get the axis-aligned direction vector for a cube face's outward normal.
 */
v3f getFaceNormal(CubeFace face);

/**
 * Get the tangent vectors (U and V directions) for a cube face.
 * These define the local coordinate frame on the face.
 *
 * @param face Which cube face
 * @param[out] tangentU Direction of increasing U
 * @param[out] tangentV Direction of increasing V
 */
void getFaceTangents(CubeFace face, v3f &tangentU, v3f &tangentV);

/**
 * Convert a cube face enum to a human-readable string.
 */
const char* faceName(CubeFace face);

/**
 * Wrap a world position to stay within the planet's quad-sphere surface.
 * If the position has moved past a face boundary, it is wrapped to the
 * adjacent face, enabling circumnavigation of the planet.
 *
 * @param worldPos Current world position
 * @param center Planet center
 * @param radius Planet radius
 * @param[out] wrapped The wrapped position (may be same as input if no wrap needed)
 * @return true if position was wrapped to a new face
 */
bool wrapPositionOnPlanet(v3f worldPos, v3f center, f32 radius, v3f &wrapped);

/**
 * Check if a world position is within the planet's quad-sphere bounds.
 * Returns false if the position has drifted outside the valid coordinate space.
 *
 * @param worldPos World position to check
 * @param center Planet center
 * @param radius Planet radius
 * @return true if position is within valid bounds
 */
bool isWithinPlanetBounds(v3f worldPos, v3f center, f32 radius);

} // namespace quadsphere
