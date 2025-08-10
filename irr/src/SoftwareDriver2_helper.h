// Copyright (C) 2002-2012 Nikolaus Gebhardt / Thomas Alten
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrMath.h"
#include "SMaterial.h"

#ifndef REALINLINE
#ifdef _MSC_VER
#define REALINLINE __forceinline
#else
#define REALINLINE inline
#endif
#endif

// ----------------------- Generic ----------------------------------
//! align_next - align to next upper 2^n
#define align_next(num, to) (((num) + (to - 1)) & (~(to - 1)))

//! a more useful memset for pixel. dest must be aligned at least to 4 byte
// (standard memset only works with 8-bit values)
inline void memset32(void *dest, const u32 value, size_t bytesize)
{
	u32 *d = (u32 *)dest;

	size_t i;

	// loops unrolled to reduce the number of increments by factor ~8.
	i = bytesize >> (2 + 3);
	while (i) {
		d[0] = value;
		d[1] = value;
		d[2] = value;
		d[3] = value;

		d[4] = value;
		d[5] = value;
		d[6] = value;
		d[7] = value;

		d += 8;
		i -= 1;
	}

	i = (bytesize >> 2) & 7;
	while (i) {
		d[0] = value;
		d += 1;
		i -= 1;
	}
}

//! a more useful memset for pixel. dest must be aligned at least to 2 byte
// (standard memset only works with 8-bit values)
inline void memset16(void *dest, const u16 value, size_t bytesize)
{
	u16 *d = (u16 *)dest;

	size_t i;

	// loops unrolled to reduce the number of increments by factor ~8.
	i = bytesize >> (1 + 3);
	while (i) {
		d[0] = value;
		d[1] = value;
		d[2] = value;
		d[3] = value;

		d[4] = value;
		d[5] = value;
		d[6] = value;
		d[7] = value;

		d += 8;
		--i;
	}

	i = (bytesize >> 1) & 7;
	while (i) {
		d[0] = value;
		++d;
		--i;
	}
}

// ------------------ Video---------------------------------------
/*!
	Pixel = dest * ( 1 - alpha ) + source * alpha
	alpha [0;256]
*/
REALINLINE u32 PixelBlend32(const u32 c2, const u32 c1, const u32 alpha)
{
	u32 srcRB = c1 & 0x00FF00FF;
	u32 srcXG = c1 & 0x0000FF00;

	u32 dstRB = c2 & 0x00FF00FF;
	u32 dstXG = c2 & 0x0000FF00;

	u32 rb = srcRB - dstRB;
	u32 xg = srcXG - dstXG;

	rb *= alpha;
	xg *= alpha;
	rb >>= 8;
	xg >>= 8;

	rb += dstRB;
	xg += dstXG;

	rb &= 0x00FF00FF;
	xg &= 0x0000FF00;

	return rb | xg;
}

/*!
	Pixel = dest * ( 1 - SourceAlpha ) + source * SourceAlpha (OpenGL blending)
*/
inline u32 PixelBlend32(const u32 c2, const u32 c1)
{
	// alpha test
	u32 alpha = c1 & 0xFF000000;

	if (0 == alpha)
		return c2;
	if (0xFF000000 == alpha) {
		return c1;
	}

	alpha >>= 24;

	// add highbit alpha, if ( alpha > 127 ) alpha += 1;
	alpha += (alpha >> 7);

	u32 srcRB = c1 & 0x00FF00FF;
	u32 srcXG = c1 & 0x0000FF00;

	u32 dstRB = c2 & 0x00FF00FF;
	u32 dstXG = c2 & 0x0000FF00;

	u32 rb = srcRB - dstRB;
	u32 xg = srcXG - dstXG;

	rb *= alpha;
	xg *= alpha;
	rb >>= 8;
	xg >>= 8;

	rb += dstRB;
	xg += dstXG;

	rb &= 0x00FF00FF;
	xg &= 0x0000FF00;

	return (c1 & 0xFF000000) | rb | xg;
}

// 2D Region closed [x0;x1]
struct AbsRectangle
{
	s32 x0;
	s32 y0;
	s32 x1;
	s32 y1;
};

//! 2D Intersection test
inline bool intersect(AbsRectangle &dest, const AbsRectangle &a, const AbsRectangle &b)
{
	dest.x0 = core::s32_max(a.x0, b.x0);
	dest.y0 = core::s32_max(a.y0, b.y0);
	dest.x1 = core::s32_min(a.x1, b.x1);
	dest.y1 = core::s32_min(a.y1, b.y1);
	return dest.x0 < dest.x1 && dest.y0 < dest.y1;
}
