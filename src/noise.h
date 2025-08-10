/*
 * Minetest
 * Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>
 * Copyright (C) 2010-2014 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "irr_v3d.h"
#include "exceptions.h"
#include "util/string.h"

#if defined(RANDOM_MIN)
#undef RANDOM_MIN
#endif
#if defined(RANDOM_MAX)
#undef RANDOM_MAX
#endif

extern const FlagDesc flagdesc_noiseparams[];

// Note: this class is not polymorphic so that its high level of
// optimizability may be preserved in the common use case
class PseudoRandom {
public:
	const static u32 RANDOM_RANGE = 32767;

	inline PseudoRandom(s32 seed_=0)
	{
		seed(seed_);
	}

	inline void seed(s32 seed)
	{
		m_next = seed;
	}

	inline u32 next()
	{
		m_next = static_cast<u32>(m_next) * 1103515245U + 12345U;
		// Signed division is required due to backwards compatibility
		return static_cast<u32>(m_next / 65536) % (RANDOM_RANGE + 1U);
	}

	inline s32 range(s32 min, s32 max)
	{
		if (max < min)
			throw PrngException("Invalid range (max < min)");
		/*
		Here, we ensure the range is not too large relative to RANDOM_MAX,
		as otherwise the effects of bias would become noticeable.  Unlike
		PcgRandom, we cannot modify this RNG's range as it would change the
		output of this RNG for reverse compatibility.
		*/
		if (static_cast<u32>(max - min) > (RANDOM_RANGE + 1) / 5)
			throw PrngException("Range too large");

		return (next() % (max - min + 1)) + min;
	}

	// Allow save and restore of state
	inline s32 getState() const
	{
		return m_next;
	}
private:
	s32 m_next;
};

class PcgRandom {
public:
	const static s32 RANDOM_MIN   = -0x7fffffff - 1;
	const static s32 RANDOM_MAX   = 0x7fffffff;
	const static u32 RANDOM_RANGE = 0xffffffff;

	PcgRandom(u64 state=0x853c49e6748fea9bULL, u64 seq=0xda3e39cb94b95bdbULL);
	void seed(u64 state, u64 seq=0xda3e39cb94b95bdbULL);
	u32 next();
	u32 range(u32 bound);
	s32 range(s32 min, s32 max);
	void bytes(void *out, size_t len);
	s32 randNormalDist(s32 min, s32 max, int num_trials=6);

	// Allow save and restore of state
	void getState(u64 state[2]) const;
	void setState(const u64 state[2]);
private:
	u64 m_state;
	u64 m_inc;
};

#define NOISE_FLAG_DEFAULTS    0x01
#define NOISE_FLAG_EASED       0x02
#define NOISE_FLAG_ABSVALUE    0x04

//// TODO(hmmmm): implement these!
#define NOISE_FLAG_POINTBUFFER 0x08
#define NOISE_FLAG_SIMPLEX     0x10

struct NoiseParams {
	float offset = 0.0f;
	float scale = 1.0f;
	v3f spread = v3f(250, 250, 250);
	s32 seed = 12345;
	u16 octaves = 3;
	float persist = 0.6f;
	float lacunarity = 2.0f;
	u32 flags = NOISE_FLAG_DEFAULTS;

	NoiseParams() = default;

	NoiseParams(float offset_, float scale_, const v3f &spread_, s32 seed_,
		u16 octaves_, float persist_, float lacunarity_,
		u32 flags_=NOISE_FLAG_DEFAULTS)
	{
		offset     = offset_;
		scale      = scale_;
		spread     = spread_;
		seed       = seed_;
		octaves    = octaves_;
		persist    = persist_;
		lacunarity = lacunarity_;
		flags      = flags_;
	}
};

class Noise {
public:
	NoiseParams np;
	s32 seed;
	u32 sx;
	u32 sy;
	u32 sz;
	float *noise_buf = nullptr;
	float *value_buf = nullptr;
	float *persist_buf = nullptr;
	float *result = nullptr;

	Noise(const NoiseParams *np, s32 seed, u32 sx, u32 sy, u32 sz=1);
	~Noise();

	void setSize(u32 sx, u32 sy, u32 sz=1);
	void setSpreadFactor(v3f spread);
	void setOctaves(int octaves);

	void valueMap2D(
		float x, float y,
		float step_x, float step_y,
		s32 seed);
	void valueMap3D(
		float x, float y, float z,
		float step_x, float step_y, float step_z,
		s32 seed);

	float *noiseMap2D(float x, float y, float *persistence_map=NULL);
	float *noiseMap3D(float x, float y, float z, float *persistence_map=NULL);

	inline float *noiseMap2D_PO(float x, float xoff, float y, float yoff,
		float *persistence_map=NULL)
	{
		return noiseMap2D(
			x + xoff * np.spread.X,
			y + yoff * np.spread.Y,
			persistence_map);
	}

	inline float *noiseMap3D_PO(float x, float xoff, float y, float yoff,
		float z, float zoff, float *persistence_map=NULL)
	{
		return noiseMap3D(
			x + xoff * np.spread.X,
			y + yoff * np.spread.Y,
			z + zoff * np.spread.Z,
			persistence_map);
	}

private:
	void allocBuffers();
	void resizeNoiseBuf(bool is3d);
	void updateResults(float g, float *gmap, const float *persistence_map,
			size_t bufsize);

};

float NoiseFractal2D(const NoiseParams *np, float x, float y, s32 seed);
float NoiseFractal3D(const NoiseParams *np, float x, float y, float z, s32 seed);

inline float NoiseFractal2D_PO(NoiseParams *np, float x, float xoff,
	float y, float yoff, s32 seed)
{
	return NoiseFractal2D(np,
		x + xoff * np->spread.X,
		y + yoff * np->spread.Y,
		seed);
}

inline float NoiseFractal3D_PO(NoiseParams *np, float x, float xoff,
	float y, float yoff, float z, float zoff, s32 seed)
{
	return NoiseFractal3D(np,
		x + xoff * np->spread.X,
		y + yoff * np->spread.Y,
		z + zoff * np->spread.Z,
		seed);
}

// Return value: -1 ... 1
float noise2d(int x, int y, s32 seed);
float noise3d(int x, int y, int z, s32 seed);

float noise2d_value(float x, float y, s32 seed, bool eased=true);
float noise3d_value(float x, float y, float z, s32 seed, bool eased=false);

float noise2d_fractal(float x, float y, s32 seed,
		int octaves, float persistence, bool eased=true);

inline float easeCurve(float t)
{
	return t * t * t * (t * (6.f * t - 15.f) + 10.f);
}

float contour(float v);
