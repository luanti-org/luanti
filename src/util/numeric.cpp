// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "numeric.h"

#include "log.h"
#include "constants.h" // BS, MAP_BLOCKSIZE
#include "noise.h" // PcgRandom
#include <cstring>
#include <cmath>


// myrand

static PcgRandom g_pcgrand;

u32 myrand()
{
	return g_pcgrand.next();
}

void mysrand(u64 seed)
{
	g_pcgrand.seed(seed);
}

void myrand_bytes(void *out, size_t len)
{
	g_pcgrand.bytes(out, len);
}

float myrand_float()
{
	u32 uv = g_pcgrand.next();
	return (float)uv / (float)U32_MAX;
}

int myrand_range(int min, int max)
{
	return g_pcgrand.range(min, max);
}

float myrand_range(float min, float max)
{
	return (max-min) * myrand_float() + min;
}


u64 murmur_hash_64_ua(const void *key, size_t len, unsigned int seed)
{
	const u64 m = 0xc6a4a7935bd1e995ULL;
	const int r = 47;
	u64 h = seed ^ (len * m);

	const u8 *data = (const u8 *)key;
	const u8 *end = data + (len / 8) * 8;

	while (data != end) {
		u64 k;
		memcpy(&k, data, sizeof(u64));
		data += sizeof(u64);

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	switch (len & 7) {
		case 7: h ^= (u64)data[6] << 48; [[fallthrough]];
		case 6: h ^= (u64)data[5] << 40; [[fallthrough]];
		case 5: h ^= (u64)data[4] << 32; [[fallthrough]];
		case 4: h ^= (u64)data[3] << 24; [[fallthrough]];
		case 3: h ^= (u64)data[2] << 16; [[fallthrough]];
		case 2: h ^= (u64)data[1] << 8;  [[fallthrough]];
		case 1: h ^= (u64)data[0];
				h *= m;
	}

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}


bool isBlockInSight(v3s16 blockpos_b, v3f camera_pos, v3f camera_dir,
		f32 camera_fov, f32 range, f32 *distance_ptr)
{
	v3s16 blockpos_nodes = blockpos_b * MAP_BLOCKSIZE;

	// Block center position
	v3f blockpos = v3f::from(blockpos_nodes + MAP_BLOCKSIZE / 2) * BS;

	// Block position relative to camera
	v3f blockpos_relative = blockpos - camera_pos;

	// Total distance
	f32 d = MYMAX(0, blockpos_relative.getLength() - BLOCK_MAX_RADIUS);

	if (distance_ptr)
		*distance_ptr = d;

	// If block is far away, it's not in sight
	if (d > range)
		return false;

	// If block is (nearly) touching the camera, don't
	// bother validating further (that is, render it anyway)
	if (d == 0)
		return true;

	// Adjust camera position, for purposes of computing the angle,
	// such that a block that has any portion visible with the
	// current camera position will have the center visible at the
	// adjusted position
	f32 adjdist = BLOCK_MAX_RADIUS / cos((M_PI - camera_fov) / 2);

	// Block position relative to adjusted camera
	v3f blockpos_adj = blockpos - (camera_pos - camera_dir * adjdist);

	// Distance in camera direction (+=front, -=back)
	f32 dforward = blockpos_adj.dotProduct(camera_dir);

	// Cosine of the angle between the camera direction
	// and the block direction (camera_dir is an unit vector)
	f32 cosangle = dforward / blockpos_adj.getLength();

	// If block is not in the field of view, skip it
	// HOTFIX: use sligthly increased angle (+10%) to fix too aggressive
	// culling. Somebody have to find out whats wrong with the math here.
	// Previous value: camera_fov / 2
	if (cosangle < std::cos(camera_fov * 0.55f))
		return false;

	return true;
}


inline float adjustDist(float dist, float zoom_fov)
{
	// 1.775 ~= 72 * PI / 180 * 1.4, the default FOV on the client.
	// The heuristic threshold for zooming is half of that.
	static constexpr const float threshold_fov = 1.775f / 2.0f;
	if (zoom_fov < 0.001f || zoom_fov > threshold_fov)
		return dist;

	return dist * std::cbrt((1.0f - std::cos(threshold_fov)) /
		(1.0f - std::cos(zoom_fov / 2.0f)));
}

s16 adjustDist(s16 dist, float zoom_fov)
{
	return std::round(adjustDist((float)dist, zoom_fov));
}

void setPitchYawRollRad(core::matrix4 &m, v3f rot)
{
	f64 a1 = rot.Z, a2 = rot.X, a3 = rot.Y;
	f64 c1 = cos(a1), s1 = sin(a1);
	f64 c2 = cos(a2), s2 = sin(a2);
	f64 c3 = cos(a3), s3 = sin(a3);
	f32 *M = m.pointer();

	M[0] = s1 * s2 * s3 + c1 * c3;
	M[1] = s1 * c2;
	M[2] = s1 * s2 * c3 - c1 * s3;

	M[4] = c1 * s2 * s3 - s1 * c3;
	M[5] = c1 * c2;
	M[6] = c1 * s2 * c3 + s1 * s3;

	M[8] = c2 * s3;
	M[9] = -s2;
	M[10] = c2 * c3;
}

v3f getPitchYawRollRad(const core::matrix4 &m)
{
	const f32 *M = m.pointer();

	f64 a1 = atan2(M[1], M[5]);
	f32 c2 = std::sqrt((f64)M[10]*M[10] + (f64)M[8]*M[8]);
	f32 a2 = atan2f(-M[9], c2);
	f64 c1 = cos(a1);
	f64 s1 = sin(a1);
	f32 a3 = atan2f(s1*M[6] - c1*M[2], c1*M[0] - s1*M[4]);

	return v3f(a2, a3, a1);
}
