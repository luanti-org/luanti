// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Sphere-aware block position implementation

#include "sphere_block_pos.h"
#include "constants.h"
#include <sstream>
#include <cmath>

namespace quadsphere {

// Face offsets in legacy coordinate space
// Layout:
//           [TOP]      (0, 1)
//   [LEFT] [FRONT] [RIGHT] [BACK]
//   (-1,0)  (0,0)   (1,0)  (2,0)
//          [BOTTOM]   (0, -1)
static const s16 FACE_OFFSET_X[] = {
	0,   // FRONT
	2,   // BACK
	1,   // RIGHT
	-1,  // LEFT
	0,   // TOP
	0    // BOTTOM
};

static const s16 FACE_OFFSET_Z[] = {
	0,   // FRONT
	0,   // BACK
	0,   // RIGHT
	0,   // LEFT
	1,   // TOP
	-1   // BOTTOM
};

v3s16 SphereBlockPos::toLegacy(const PlanetConfig &config) const
{
	s16 faceSize = config.face_blocks;

	s16 x = u + FACE_OFFSET_X[static_cast<u8>(face)] * faceSize;
	s16 y = depth;
	s16 z = v + FACE_OFFSET_Z[static_cast<u8>(face)] * faceSize;

	return v3s16(x, y, z);
}

SphereBlockPos SphereBlockPos::fromLegacy(v3s16 pos, const PlanetConfig &config)
{
	s16 faceSize = config.face_blocks;
	SphereBlockPos result;
	result.depth = pos.Y;

	// Determine which face based on x, z coordinates
	s16 faceX = (pos.X >= 0) ? (pos.X / faceSize) : ((pos.X + 1) / faceSize - 1);
	s16 faceZ = (pos.Z >= 0) ? (pos.Z / faceSize) : ((pos.Z + 1) / faceSize - 1);

	// Map face coordinates to face enum
	if (faceZ == 1) {
		result.face = CubeFace::TOP;
	} else if (faceZ == -1) {
		result.face = CubeFace::BOTTOM;
	} else if (faceX == 0) {
		result.face = CubeFace::FRONT;
	} else if (faceX == 1) {
		result.face = CubeFace::RIGHT;
	} else if (faceX == 2) {
		result.face = CubeFace::BACK;
	} else if (faceX == -1) {
		result.face = CubeFace::LEFT;
	} else {
		// Invalid position, default to front
		result.face = CubeFace::FRONT;
	}

	// Calculate u, v within the face
	result.u = pos.X - FACE_OFFSET_X[static_cast<u8>(result.face)] * faceSize;
	result.v = pos.Z - FACE_OFFSET_Z[static_cast<u8>(result.face)] * faceSize;

	// Wrap to valid range
	result.u = ((result.u % faceSize) + faceSize) % faceSize;
	result.v = ((result.v % faceSize) + faceSize) % faceSize;

	return result;
}

void SphereBlockPos::getUV(const PlanetConfig &config, f32 &out_u, f32 &out_v) const
{
	f32 blockSize = config.getBlockUVSize();
	out_u = (static_cast<f32>(u) + 0.5f) * blockSize;
	out_v = (static_cast<f32>(v) + 0.5f) * blockSize;
}

v3f SphereBlockPos::getWorldCenter(const PlanetConfig &config) const
{
	f32 uv_u, uv_v;
	getUV(config, uv_u, uv_v);

	f32 altitude = static_cast<f32>(depth) * MAP_BLOCKSIZE * BS;

	return cubeToWorld(face, uv_u, uv_v, config.radius, altitude) + config.center;
}

v3f SphereBlockPos::getNodeWorldPos(v3s16 nodeOffset, const PlanetConfig &config) const
{
	f32 blockSize = config.getBlockUVSize();

	f32 uv_u = (static_cast<f32>(u) + (nodeOffset.X + 0.5f) / MAP_BLOCKSIZE) * blockSize;
	f32 uv_v = (static_cast<f32>(v) + (nodeOffset.Z + 0.5f) / MAP_BLOCKSIZE) * blockSize;

	f32 altitude = (static_cast<f32>(depth) * MAP_BLOCKSIZE + nodeOffset.Y + 0.5f) * BS;

	return cubeToWorld(face, uv_u, uv_v, config.radius, altitude) + config.center;
}

SphereBlockPos SphereBlockPos::getNeighbor(s16 du, s16 dv, s16 ddepth,
                                           const PlanetConfig &config) const
{
	SphereBlockPos result = *this;
	result.u += du;
	result.v += dv;
	result.depth += ddepth;

	s16 faceSize = config.face_blocks;

	// Handle face transitions
	while (result.u < 0) {
		// Moving left off face
		f32 fu = 0.0f;
		f32 fv = (static_cast<f32>(result.v) + 0.5f) / faceSize;
		CubeFace newFace = getNeighborFace(result.face, FaceDirection::LEFT);
		transformUVAcrossFaces(result.face, newFace, FaceDirection::LEFT, fu, fv);

		result.face = newFace;
		result.u = static_cast<s16>(fu * faceSize);
		result.v = static_cast<s16>(fv * faceSize);
		result.u += (result.u + 1);  // Adjust for crossing
		if (result.u < 0) result.u += faceSize;
	}

	while (result.u >= faceSize) {
		// Moving right off face
		f32 fu = 1.0f;
		f32 fv = (static_cast<f32>(result.v) + 0.5f) / faceSize;
		CubeFace newFace = getNeighborFace(result.face, FaceDirection::RIGHT);
		transformUVAcrossFaces(result.face, newFace, FaceDirection::RIGHT, fu, fv);

		result.face = newFace;
		result.u = static_cast<s16>(fu * faceSize);
		result.v = static_cast<s16>(fv * faceSize);
		result.u = result.u - faceSize;
		if (result.u >= faceSize) result.u -= faceSize;
	}

	while (result.v < 0) {
		// Moving down off face
		f32 fu = (static_cast<f32>(result.u) + 0.5f) / faceSize;
		f32 fv = 0.0f;
		CubeFace newFace = getNeighborFace(result.face, FaceDirection::DOWN);
		transformUVAcrossFaces(result.face, newFace, FaceDirection::DOWN, fu, fv);

		result.face = newFace;
		result.u = static_cast<s16>(fu * faceSize);
		result.v = static_cast<s16>(fv * faceSize);
		result.v += (result.v + 1);
		if (result.v < 0) result.v += faceSize;
	}

	while (result.v >= faceSize) {
		// Moving up off face
		f32 fu = (static_cast<f32>(result.u) + 0.5f) / faceSize;
		f32 fv = 1.0f;
		CubeFace newFace = getNeighborFace(result.face, FaceDirection::UP);
		transformUVAcrossFaces(result.face, newFace, FaceDirection::UP, fu, fv);

		result.face = newFace;
		result.u = static_cast<s16>(fu * faceSize);
		result.v = static_cast<s16>(fv * faceSize);
		result.v = result.v - faceSize;
		if (result.v >= faceSize) result.v -= faceSize;
	}

	return result;
}

void SphereBlockPos::getFaceNeighbors(const PlanetConfig &config,
                                      SphereBlockPos neighbors[4]) const
{
	neighbors[0] = getNeighbor(-1, 0, 0, config);  // Left
	neighbors[1] = getNeighbor(1, 0, 0, config);   // Right
	neighbors[2] = getNeighbor(0, -1, 0, config);  // Down
	neighbors[3] = getNeighbor(0, 1, 0, config);   // Up
}

bool SphereBlockPos::isValid(const PlanetConfig &config) const
{
	return u >= 0 && u < config.face_blocks &&
	       v >= 0 && v < config.face_blocks &&
	       config.isValidAltitude(depth);
}

SphereBlockPos SphereBlockPos::fromWorldPos(v3f worldPos, const PlanetConfig &config)
{
	QuadSpherePos qsp = worldToQuadSphere(worldPos, config.center, config.radius);

	SphereBlockPos result;
	result.face = qsp.face;

	f32 blockSize = config.getBlockUVSize();
	result.u = static_cast<s16>(qsp.u / blockSize);
	result.v = static_cast<s16>(qsp.v / blockSize);
	result.depth = static_cast<s16>(qsp.altitude / (MAP_BLOCKSIZE * BS));

	// Clamp to valid range
	result.u = std::max<s16>(0, std::min<s16>(result.u, config.face_blocks - 1));
	result.v = std::max<s16>(0, std::min<s16>(result.v, config.face_blocks - 1));

	return result;
}

size_t SphereBlockPos::hash() const
{
	// Combine all components into a single hash
	size_t h = 0;
	h ^= std::hash<u8>()(static_cast<u8>(face)) + 0x9e3779b9 + (h << 6) + (h >> 2);
	h ^= std::hash<s16>()(u) + 0x9e3779b9 + (h << 6) + (h >> 2);
	h ^= std::hash<s16>()(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
	h ^= std::hash<s16>()(depth) + 0x9e3779b9 + (h << 6) + (h >> 2);
	return h;
}

std::string SphereBlockPos::toString() const
{
	std::ostringstream ss;
	ss << "SphereBlockPos(" << faceName(face)
	   << ", u=" << u << ", v=" << v << ", depth=" << depth << ")";
	return ss.str();
}

// SphereBlockIterator implementation

void SphereBlockIterator::iterateInRange(const SphereBlockPos &center,
                                         f32 range,
                                         const PlanetConfig &config,
                                         const Callback &callback)
{
	// Calculate block range based on geodesic distance
	f32 blockSize = config.getApproxBlockWorldSize();
	s16 blockRange = static_cast<s16>(std::ceil(range / blockSize)) + 1;

	// Also iterate through depth layers
	s16 depthRange = static_cast<s16>(std::ceil(range / (MAP_BLOCKSIZE * BS))) + 1;

	for (s16 dv = -blockRange; dv <= blockRange; ++dv) {
		for (s16 du = -blockRange; du <= blockRange; ++du) {
			for (s16 dd = -depthRange; dd <= depthRange; ++dd) {
				SphereBlockPos pos = center.getNeighbor(du, dv, dd, config);
				if (!pos.isValid(config))
					continue;

				// Check actual distance
				f32 dist = SphereGeometry::euclideanDistance(center, pos, config);
				if (dist <= range) {
					callback(pos);
				}
			}
		}
	}
}

void SphereBlockIterator::iterateShell(s8 faceIndex, s16 depth,
                                       const PlanetConfig &config,
                                       const Callback &callback)
{
	auto iterateFace = [&](CubeFace face) {
		for (s16 v = 0; v < config.face_blocks; ++v) {
			for (s16 u = 0; u < config.face_blocks; ++u) {
				callback(SphereBlockPos(face, u, v, depth));
			}
		}
	};

	if (faceIndex < 0) {
		// Iterate all faces
		for (u8 f = 0; f < CUBE_FACE_COUNT; ++f) {
			iterateFace(static_cast<CubeFace>(f));
		}
	} else {
		iterateFace(static_cast<CubeFace>(faceIndex));
	}
}

size_t SphereBlockIterator::estimateBlockCount(f32 range, const PlanetConfig &config)
{
	f32 blockSize = config.getApproxBlockWorldSize();
	s16 blockRange = static_cast<s16>(std::ceil(range / blockSize)) + 1;
	s16 depthRange = static_cast<s16>(std::ceil(range / (MAP_BLOCKSIZE * BS))) + 1;

	// Approximate as a sphere of blocks
	size_t diameter = 2 * blockRange + 1;
	size_t depthDiameter = 2 * depthRange + 1;
	return diameter * diameter * depthDiameter;
}

// SphereGeometry implementation

f32 SphereGeometry::geodesicDistance(const SphereBlockPos &a,
                                     const SphereBlockPos &b,
                                     const PlanetConfig &config)
{
	v3f posA = a.getWorldCenter(config);
	v3f posB = b.getWorldCenter(config);

	return quadsphere::geodesicDistance(posA, posB, config.center, config.radius);
}

f32 SphereGeometry::euclideanDistance(const SphereBlockPos &a,
                                      const SphereBlockPos &b,
                                      const PlanetConfig &config)
{
	v3f posA = a.getWorldCenter(config);
	v3f posB = b.getWorldCenter(config);

	return (posB - posA).getLength();
}

v3f SphereGeometry::directionTo(const SphereBlockPos &from,
                                const SphereBlockPos &to,
                                const PlanetConfig &config)
{
	v3f posFrom = from.getWorldCenter(config);
	v3f posTo = to.getWorldCenter(config);

	v3f diff = posTo - posFrom;
	f32 len = diff.getLength();
	if (len < 0.0001f)
		return v3f(0, 1, 0);
	return diff / len;
}

bool SphereGeometry::isVisible(const SphereBlockPos &from,
                               const SphereBlockPos &to,
                               const PlanetConfig &config)
{
	v3f posFrom = from.getWorldCenter(config);
	v3f posTo = to.getWorldCenter(config);

	// Get surface altitude of viewer
	f32 viewerAlt = (posFrom - config.center).getLength() - config.radius;

	// Calculate horizon distance
	f32 horizonAngle = std::acos(config.radius / (config.radius + viewerAlt));

	// Get angle to target
	v3f normFrom = getSurfaceNormal(posFrom, config.center);
	v3f normTo = getSurfaceNormal(posTo, config.center);
	f32 dot = normFrom.dotProduct(normTo);
	dot = std::max(-1.0f, std::min(1.0f, dot));
	f32 angleToTarget = std::acos(dot);

	// Target is visible if within horizon angle (plus some margin for altitude)
	f32 targetAlt = (posTo - config.center).getLength() - config.radius;
	f32 altitudeBonus = std::atan2(targetAlt, config.radius);

	return angleToTarget < horizonAngle + altitudeBonus + 0.1f;  // 0.1 rad margin
}

} // namespace quadsphere
