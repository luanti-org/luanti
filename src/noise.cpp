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

#include <cmath>
#include "noise.h"
#include <iostream>
#include <cstring> // memset
#include "debug.h"
#include "util/numeric.h"
#include "util/string.h"
#include "exceptions.h"

#define NOISE_MAGIC_X    1619
#define NOISE_MAGIC_Y    31337
#define NOISE_MAGIC_Z    52591
// Unsigned magic seed prevents undefined behavior.
#define NOISE_MAGIC_SEED 1013U

#define myfloor(x) ((x) < 0 ? (int)(x) - 1 : (int)(x))

const FlagDesc flagdesc_noiseparams[] = {
	{"defaults",    NOISE_FLAG_DEFAULTS},
	{"eased",       NOISE_FLAG_EASED},
	{"absvalue",    NOISE_FLAG_ABSVALUE},
	{"pointbuffer", NOISE_FLAG_POINTBUFFER},
	{"simplex",     NOISE_FLAG_SIMPLEX},
	{NULL,          0}
};

///////////////////////////////////////////////////////////////////////////////

PcgRandom::PcgRandom(u64 state, u64 seq)
{
	seed(state, seq);
}

void PcgRandom::seed(u64 state, u64 seq)
{
	m_state = 0U;
	m_inc = (seq << 1u) | 1u;
	next();
	m_state += state;
	next();
}


u32 PcgRandom::next()
{
	u64 oldstate = m_state;
	m_state = oldstate * 6364136223846793005ULL + m_inc;

	u32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	u32 rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}


u32 PcgRandom::range(u32 bound)
{
	// If the bound is 0, we cover the whole RNG's range
	if (bound == 0)
		return next();

	if (bound == 1)
		return 0;

	/*
		This is an optimization of the expression:
		  0x100000000ull % bound
		since 64-bit modulo operations typically much slower than 32.
	*/
	u32 threshold = -bound % bound;
	u32 r;

	/*
		If the bound is not a multiple of the RNG's range, it may cause bias,
		e.g. a RNG has a range from 0 to 3 and we take want a number 0 to 2.
		Using rand() % 3, the number 0 would be twice as likely to appear.
		With a very large RNG range, the effect becomes less prevalent but
		still present.

		This can be solved by modifying the range of the RNG to become a
		multiple of bound by dropping values above the a threshold.

		In our example, threshold == 4 % 3 == 1, so reject values < 1
		(that is, 0), thus making the range == 3 with no bias.

		This loop may look dangerous, but will always terminate due to the
		RNG's property of uniformity.
	*/
	while ((r = next()) < threshold)
		;

	return r % bound;
}


s32 PcgRandom::range(s32 min, s32 max)
{
	if (max < min)
		throw PrngException("Invalid range (max < min)");

	// We have to cast to s64 because otherwise this could overflow,
	// and signed overflow is undefined behavior.
	u32 bound = (s64)max - (s64)min + 1;
	return range(bound) + min;
}


void PcgRandom::bytes(void *out, size_t len)
{
	u8 *outb = (u8 *)out;
	int bytes_left = 0;
	u32 r;

	while (len--) {
		if (bytes_left == 0) {
			bytes_left = sizeof(u32);
			r = next();
		}

		*outb = r & 0xFF;
		outb++;
		bytes_left--;
		r >>= CHAR_BIT;
	}
}


s32 PcgRandom::randNormalDist(s32 min, s32 max, int num_trials)
{
	s32 accum = 0;
	for (int i = 0; i != num_trials; i++)
		accum += range(min, max);
	return myround((float)accum / num_trials);
}

void PcgRandom::getState(u64 state[2]) const
{
	state[0] = m_state;
	state[1] = m_inc;
}

void PcgRandom::setState(const u64 state[2])
{
	m_state = state[0];
	m_inc = state[1];
}

///////////////////////////////////////////////////////////////////////////////

float noise2d(int x, int y, s32 seed)
{
	unsigned int n = (NOISE_MAGIC_X * x + NOISE_MAGIC_Y * y
			+ NOISE_MAGIC_SEED * seed) & 0x7fffffff;
	n = (n >> 13) ^ n;
	n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 1.f - (float)(int)n / 0x40000000;
}


float noise3d(int x, int y, int z, s32 seed)
{
	unsigned int n = (NOISE_MAGIC_X * x + NOISE_MAGIC_Y * y + NOISE_MAGIC_Z * z
			+ NOISE_MAGIC_SEED * seed) & 0x7fffffff;
	n = (n >> 13) ^ n;
	n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 1.f - (float)(int)n / 0x40000000;
}


inline float dotProduct(float vx, float vy, float wx, float wy)
{
	return vx * wx + vy * wy;
}


inline float linearInterpolation(float v0, float v1, float t)
{
	return v0 + (v1 - v0) * t;
}


inline float biLinearInterpolation(
	float v00, float v10,
	float v01, float v11,
	float x, float y,
	bool eased)
{
	// Inlining will optimize this branch out when possible
	if (eased) {
		x = easeCurve(x);
		y = easeCurve(y);
	}
	float u = linearInterpolation(v00, v10, x);
	float v = linearInterpolation(v01, v11, x);
	return linearInterpolation(u, v, y);
}


inline float triLinearInterpolation(
	float v000, float v100, float v010, float v110,
	float v001, float v101, float v011, float v111,
	float x, float y, float z,
	bool eased)
{
	// Inlining will optimize this branch out when possible
	if (eased) {
		x = easeCurve(x);
		y = easeCurve(y);
		z = easeCurve(z);
	}
	float u = biLinearInterpolation(v000, v100, v010, v110, x, y, false);
	float v = biLinearInterpolation(v001, v101, v011, v111, x, y, false);
	return linearInterpolation(u, v, z);
}

float noise2d_value(float x, float y, s32 seed, bool eased)
{
	// Calculate the integer coordinates
	int x0 = myfloor(x);
	int y0 = myfloor(y);
	// Calculate the remaining part of the coordinates
	float xl = x - (float)x0;
	float yl = y - (float)y0;
	// Get values for corners of square
	float v00 = noise2d(x0, y0, seed);
	float v10 = noise2d(x0+1, y0, seed);
	float v01 = noise2d(x0, y0+1, seed);
	float v11 = noise2d(x0+1, y0+1, seed);
	// Interpolate
	return biLinearInterpolation(v00, v10, v01, v11, xl, yl, eased);
}


float noise3d_value(float x, float y, float z, s32 seed, bool eased)
{
	// Calculate the integer coordinates
	int x0 = myfloor(x);
	int y0 = myfloor(y);
	int z0 = myfloor(z);
	// Calculate the remaining part of the coordinates
	float xl = x - (float)x0;
	float yl = y - (float)y0;
	float zl = z - (float)z0;
	// Get values for corners of cube
	float v000 = noise3d(x0,     y0,     z0,     seed);
	float v100 = noise3d(x0 + 1, y0,     z0,     seed);
	float v010 = noise3d(x0,     y0 + 1, z0,     seed);
	float v110 = noise3d(x0 + 1, y0 + 1, z0,     seed);
	float v001 = noise3d(x0,     y0,     z0 + 1, seed);
	float v101 = noise3d(x0 + 1, y0,     z0 + 1, seed);
	float v011 = noise3d(x0,     y0 + 1, z0 + 1, seed);
	float v111 = noise3d(x0 + 1, y0 + 1, z0 + 1, seed);
	// Interpolate
	return triLinearInterpolation(
		v000, v100, v010, v110,
		v001, v101, v011, v111,
		xl, yl, zl,
		eased);
}


float noise2d_fractal(float x, float y, s32 seed,
	int octaves, float persistence, bool eased)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;
	for (int i = 0; i < octaves; i++)
	{
		a += g * noise2d_value(x * f, y * f, seed + i, eased);
		f *= 2.0;
		g *= persistence;
	}
	return a;
}


float contour(float v)
{
	v = std::fabs(v);
	if (v >= 1.0)
		return 0.0;
	return (1.0 - v);
}


///////////////////////// [ Fractal value noise ] ////////////////////////////


float NoiseFractal2D(const NoiseParams *np, float x, float y, s32 seed)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	x /= np->spread.X;
	y /= np->spread.Y;
	seed += np->seed;

	for (size_t i = 0; i < np->octaves; i++) {
		float noiseval = noise2d_value(x * f, y * f, seed + i,
			np->flags & (NOISE_FLAG_DEFAULTS | NOISE_FLAG_EASED));

		if (np->flags & NOISE_FLAG_ABSVALUE)
			noiseval = std::fabs(noiseval);

		a += g * noiseval;
		f *= np->lacunarity;
		g *= np->persist;
	}

	return np->offset + a * np->scale;
}


float NoiseFractal3D(const NoiseParams *np, float x, float y, float z, s32 seed)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	x /= np->spread.X;
	y /= np->spread.Y;
	z /= np->spread.Z;
	seed += np->seed;

	for (size_t i = 0; i < np->octaves; i++) {
		float noiseval = noise3d_value(x * f, y * f, z * f, seed + i,
			np->flags & NOISE_FLAG_EASED);

		if (np->flags & NOISE_FLAG_ABSVALUE)
			noiseval = std::fabs(noiseval);

		a += g * noiseval;
		f *= np->lacunarity;
		g *= np->persist;
	}

	return np->offset + a * np->scale;
}


Noise::Noise(const NoiseParams *np_, s32 seed, u32 sx, u32 sy, u32 sz)
{
	np = *np_;
	this->seed = seed;
	this->sx   = sx;
	this->sy   = sy;
	this->sz   = sz;

	allocBuffers();
}


Noise::~Noise()
{
	delete[] value_buf;
	delete[] persist_buf;
	delete[] noise_buf;
	delete[] result;
}


void Noise::allocBuffers()
{
	if (sx < 1)
		sx = 1;
	if (sy < 1)
		sy = 1;
	if (sz < 1)
		sz = 1;

	this->noise_buf = NULL;
	resizeNoiseBuf(sz > 1);

	delete[] value_buf;
	delete[] persist_buf;
	delete[] result;

	try {
		size_t bufsize = sx * sy * sz;
		this->persist_buf = NULL;
		this->value_buf = new float[bufsize];
		this->result = new float[bufsize];
	} catch (std::bad_alloc &e) {
		throw InvalidNoiseParamsException();
	}
}


void Noise::setSize(u32 sx, u32 sy, u32 sz)
{
	this->sx = sx;
	this->sy = sy;
	this->sz = sz;

	allocBuffers();
}


void Noise::setSpreadFactor(v3f spread)
{
	this->np.spread = spread;

	resizeNoiseBuf(sz > 1);
}


void Noise::setOctaves(int octaves)
{
	this->np.octaves = octaves;

	resizeNoiseBuf(sz > 1);
}


void Noise::resizeNoiseBuf(bool is3d)
{
	// Maximum possible spread value factor
	float ofactor = (np.lacunarity > 1.0) ?
		pow(np.lacunarity, np.octaves - 1) :
		np.lacunarity;

	// Noise lattice point count
	// (int)(sz * spread * ofactor) is # of lattice points crossed due to length
	float num_noise_points_x = sx * ofactor / np.spread.X;
	float num_noise_points_y = sy * ofactor / np.spread.Y;
	float num_noise_points_z = sz * ofactor / np.spread.Z;

	// Protect against obviously invalid parameters
	if (num_noise_points_x > 1000000000.f ||
			num_noise_points_y > 1000000000.f ||
			num_noise_points_z > 1000000000.f)
		throw InvalidNoiseParamsException();

	// Protect against an octave having a spread < 1, causing broken noise values
	if (np.spread.X / ofactor < 1.0f ||
			np.spread.Y / ofactor < 1.0f ||
			np.spread.Z / ofactor < 1.0f) {
		errorstream << "A noise parameter has too many octaves: "
			<< np.octaves << " octaves" << std::endl;
		throw InvalidNoiseParamsException("A noise parameter has too many octaves");
	}

	// + 2 for the two initial endpoints
	// + 1 for potentially crossing a boundary due to offset
	size_t nlx = (size_t)std::ceil(num_noise_points_x) + 3;
	size_t nly = (size_t)std::ceil(num_noise_points_y) + 3;
	size_t nlz = is3d ? (size_t)std::ceil(num_noise_points_z) + 3 : 1;

	delete[] noise_buf;
	try {
		noise_buf = new float[nlx * nly * nlz];
	} catch (std::bad_alloc &e) {
		throw InvalidNoiseParamsException();
	}
}


/*
 * NB:  This algorithm is not optimal in terms of space complexity.  The entire
 * integer lattice of noise points could be done as 2 lines instead, and for 3D,
 * 2 lines + 2 planes.
 * However, this would require the noise calls to be interposed with the
 * interpolation loops, which may trash the icache, leading to lower overall
 * performance.
 * Another optimization that could save half as many noise calls is to carry over
 * values from the previous noise lattice as midpoints in the new lattice for the
 * next octave.
 */
#define idx(x, y) ((y) * nlx + (x))
void Noise::valueMap2D(
		float x, float y,
		float step_x, float step_y,
		s32 seed)
{
	float v00, v01, v10, v11, u, v, orig_u;
	u32 index, i, j, noisex, noisey;
	u32 nlx, nly;
	s32 x0, y0;

	bool eased = np.flags & (NOISE_FLAG_DEFAULTS | NOISE_FLAG_EASED);
	x0 = std::floor(x);
	y0 = std::floor(y);
	u = x - (float)x0;
	v = y - (float)y0;
	orig_u = u;

	//calculate noise point lattice
	nlx = (u32)(u + sx * step_x) + 2;
	nly = (u32)(v + sy * step_y) + 2;
	index = 0;
	for (j = 0; j != nly; j++)
		for (i = 0; i != nlx; i++)
			noise_buf[index++] = noise2d(x0 + i, y0 + j, seed);

	//calculate interpolations
	index  = 0;
	noisey = 0;
	for (j = 0; j != sy; j++) {
		v00 = noise_buf[idx(0, noisey)];
		v10 = noise_buf[idx(1, noisey)];
		v01 = noise_buf[idx(0, noisey + 1)];
		v11 = noise_buf[idx(1, noisey + 1)];

		u = orig_u;
		noisex = 0;
		for (i = 0; i != sx; i++) {
			value_buf[index++] =
				biLinearInterpolation(v00, v10, v01, v11, u, v, eased);

			u += step_x;
			if (u >= 1.0) {
				u -= 1.0;
				noisex++;
				v00 = v10;
				v01 = v11;
				v10 = noise_buf[idx(noisex + 1, noisey)];
				v11 = noise_buf[idx(noisex + 1, noisey + 1)];
			}
		}

		v += step_y;
		if (v >= 1.0) {
			v -= 1.0;
			noisey++;
		}
	}
}
#undef idx


#define idx(x, y, z) ((z) * nly * nlx + (y) * nlx + (x))
void Noise::valueMap3D(
		float x, float y, float z,
		float step_x, float step_y, float step_z,
		s32 seed)
{
	float v000, v010, v100, v110;
	float v001, v011, v101, v111;
	float u, v, w, orig_u, orig_v;
	u32 index, i, j, k, noisex, noisey, noisez;
	u32 nlx, nly, nlz;
	s32 x0, y0, z0;

	bool eased = np.flags & NOISE_FLAG_EASED;

	x0 = std::floor(x);
	y0 = std::floor(y);
	z0 = std::floor(z);
	u = x - (float)x0;
	v = y - (float)y0;
	w = z - (float)z0;
	orig_u = u;
	orig_v = v;

	//calculate noise point lattice
	nlx = (u32)(u + sx * step_x) + 2;
	nly = (u32)(v + sy * step_y) + 2;
	nlz = (u32)(w + sz * step_z) + 2;
	index = 0;
	for (k = 0; k != nlz; k++)
		for (j = 0; j != nly; j++)
			for (i = 0; i != nlx; i++)
				noise_buf[index++] = noise3d(x0 + i, y0 + j, z0 + k, seed);

	//calculate interpolations
	index  = 0;
	noisey = 0;
	noisez = 0;
	for (k = 0; k != sz; k++) {
		v = orig_v;
		noisey = 0;
		for (j = 0; j != sy; j++) {
			v000 = noise_buf[idx(0, noisey,     noisez)];
			v100 = noise_buf[idx(1, noisey,     noisez)];
			v010 = noise_buf[idx(0, noisey + 1, noisez)];
			v110 = noise_buf[idx(1, noisey + 1, noisez)];
			v001 = noise_buf[idx(0, noisey,     noisez + 1)];
			v101 = noise_buf[idx(1, noisey,     noisez + 1)];
			v011 = noise_buf[idx(0, noisey + 1, noisez + 1)];
			v111 = noise_buf[idx(1, noisey + 1, noisez + 1)];

			u = orig_u;
			noisex = 0;
			for (i = 0; i != sx; i++) {
				value_buf[index++] = triLinearInterpolation(
					v000, v100, v010, v110,
					v001, v101, v011, v111,
					u, v, w,
					eased);

				u += step_x;
				if (u >= 1.0) {
					u -= 1.0;
					noisex++;
					v000 = v100;
					v010 = v110;
					v100 = noise_buf[idx(noisex + 1, noisey,     noisez)];
					v110 = noise_buf[idx(noisex + 1, noisey + 1, noisez)];
					v001 = v101;
					v011 = v111;
					v101 = noise_buf[idx(noisex + 1, noisey,     noisez + 1)];
					v111 = noise_buf[idx(noisex + 1, noisey + 1, noisez + 1)];
				}
			}

			v += step_y;
			if (v >= 1.0) {
				v -= 1.0;
				noisey++;
			}
		}

		w += step_z;
		if (w >= 1.0) {
			w -= 1.0;
			noisez++;
		}
	}
}
#undef idx


float *Noise::noiseMap2D(float x, float y, float *persistence_map)
{
	float f = 1.0, g = 1.0;
	size_t bufsize = sx * sy;

	x /= np.spread.X;
	y /= np.spread.Y;

	memset(result, 0, sizeof(float) * bufsize);

	if (persistence_map) {
		if (!persist_buf)
			persist_buf = new float[bufsize];
		for (size_t i = 0; i != bufsize; i++)
			persist_buf[i] = 1.0;
	}

	for (size_t oct = 0; oct < np.octaves; oct++) {
		valueMap2D(x * f, y * f,
			f / np.spread.X, f / np.spread.Y,
			seed + np.seed + oct);

		updateResults(g, persist_buf, persistence_map, bufsize);

		f *= np.lacunarity;
		g *= np.persist;
	}

	if (std::fabs(np.offset - 0.f) > 0.00001 || std::fabs(np.scale - 1.f) > 0.00001) {
		for (size_t i = 0; i != bufsize; i++)
			result[i] = result[i] * np.scale + np.offset;
	}

	return result;
}


float *Noise::noiseMap3D(float x, float y, float z, float *persistence_map)
{
	float f = 1.0, g = 1.0;
	size_t bufsize = sx * sy * sz;

	x /= np.spread.X;
	y /= np.spread.Y;
	z /= np.spread.Z;

	memset(result, 0, sizeof(float) * bufsize);

	if (persistence_map) {
		if (!persist_buf)
			persist_buf = new float[bufsize];
		for (size_t i = 0; i != bufsize; i++)
			persist_buf[i] = 1.0;
	}

	for (size_t oct = 0; oct < np.octaves; oct++) {
		valueMap3D(x * f, y * f, z * f,
			f / np.spread.X, f / np.spread.Y, f / np.spread.Z,
			seed + np.seed + oct);

		updateResults(g, persist_buf, persistence_map, bufsize);

		f *= np.lacunarity;
		g *= np.persist;
	}

	if (std::fabs(np.offset - 0.f) > 0.00001 || std::fabs(np.scale - 1.f) > 0.00001) {
		for (size_t i = 0; i != bufsize; i++)
			result[i] = result[i] * np.scale + np.offset;
	}

	return result;
}


void Noise::updateResults(float g, float *gmap,
	const float *persistence_map, size_t bufsize)
{
	// This looks very ugly, but it is 50-70% faster than having
	// conditional statements inside the loop
	if (np.flags & NOISE_FLAG_ABSVALUE) {
		if (persistence_map) {
			for (size_t i = 0; i != bufsize; i++) {
				result[i] += gmap[i] * std::fabs(value_buf[i]);
				gmap[i] *= persistence_map[i];
			}
		} else {
			for (size_t i = 0; i != bufsize; i++)
				result[i] += g * std::fabs(value_buf[i]);
		}
	} else {
		if (persistence_map) {
			for (size_t i = 0; i != bufsize; i++) {
				result[i] += gmap[i] * value_buf[i];
				gmap[i] *= persistence_map[i];
			}
		} else {
			for (size_t i = 0; i != bufsize; i++)
				result[i] += g * value_buf[i];
		}
	}
}
