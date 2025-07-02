// Copyright (C) 2002-2012 Nikolaus Gebhardt / Thomas Alten
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "SoftwareDriver2_helper.h"

//! f18 - fixpoint 14.18 limit to 16k Textures
#define CBLIT_USE_FIXPOINT18

#if defined(CBLIT_USE_FIXPOINT18)
typedef int f18;
#define f18_one 262144
#define f18_zero 0
#define f32_to_f18(x) ((f18)floorf(((x) * 262144.f) + 0.f))
#define f32_to_f32(x) (x)
#define f18_floor(x) ((x) >> 18)
#define f18_round(x) ((x + 131.072) >> 18)
#else
typedef float f18;
#define f18_one 1.f
#define f18_zero_dot_five 0.5f
#define f18_zero 0.f
#define f32_to_f18(x) (x)
#define f32_to_f32(x) (x)
#define f18_floor(x) ((int)(x))
#define f18_round(x) ((int)(x + 0.5f))
#endif

struct SBlitJob
{
	AbsRectangle Dest;
	AbsRectangle Source;

	u32 argb;

	const void *src;
	void *dst;

	u32 width; // draw size
	u32 height;

	u32 srcPixelMul; // pixel byte size
	u32 dstPixelMul;

	int srcPitch; // scanline byte size. allow negative for mirror
	u32 dstPitch;

	bool stretch;
	f32 x_stretch;
	f32 y_stretch;
};

// Bitfields Cohen Sutherland
enum eClipCode
{
	CLIPCODE_EMPTY = 0,
	CLIPCODE_BOTTOM = 1,
	CLIPCODE_TOP = 2,
	CLIPCODE_LEFT = 4,
	CLIPCODE_RIGHT = 8
};

inline u32 GetClipCode(const AbsRectangle &r, const core::position2d<s32> &p)
{
	u32 code = CLIPCODE_EMPTY;

	if (p.X < r.x0)
		code = CLIPCODE_LEFT;
	else if (p.X > r.x1)
		code = CLIPCODE_RIGHT;

	if (p.Y < r.y0)
		code |= CLIPCODE_TOP;
	else if (p.Y > r.y1)
		code |= CLIPCODE_BOTTOM;

	return code;
}

/*
 */
inline void GetClip(AbsRectangle &clipping, video::IImage *t)
{
	clipping.x0 = 0;
	clipping.y0 = 0;
	clipping.x1 = t->getDimension().Width - 1;
	clipping.y1 = t->getDimension().Height - 1;
}

/*
	return alpha in [0;256] Granularity from 32-Bit ARGB
	add highbit alpha ( alpha > 127 ? + 1 )
*/
static inline u32 extractAlpha(const u32 c)
{
	return (c >> 24) + (c >> 31);
}

/*
	return alpha in [0;255] Granularity and 32-Bit ARGB
	add highbit alpha ( alpha > 127 ? + 1 )
*/
static inline u32 packAlpha(const u32 c)
{
	return (c > 127 ? c - 1 : c) << 24;
}

/*!
	Scale Color by (1/value)
	value 0 - 256 ( alpha )
*/
inline u32 PixelLerp32(const u32 source, const u32 value)
{
	u32 srcRB = source & 0x00FF00FF;
	u32 srcXG = (source & 0xFF00FF00) >> 8;

	srcRB *= value;
	srcXG *= value;

	srcRB >>= 8;
	// srcXG >>= 8;

	srcXG &= 0xFF00FF00;
	srcRB &= 0x00FF00FF;

	return srcRB | srcXG;
}

/*!
 */
static void executeBlit_TextureCopy_x_to_x(const SBlitJob *job)
{
	if (job->stretch) {
		const f18 wscale = f32_to_f18(job->x_stretch);
		const f18 hscale = f32_to_f18(job->y_stretch);

		f18 src_y = f18_zero;

		if (job->srcPixelMul == 4) {
			const u32 *src = (u32 *)(job->src);
			u32 *dst = (u32 *)(job->dst);

			for (u32 dy = 0; dy < job->height; ++dy, src_y += hscale) {
				src = (u32 *)((u8 *)(job->src) + job->srcPitch * f18_floor(src_y));

				f18 src_x = f18_zero;
				for (u32 dx = 0; dx < job->width; ++dx, src_x += wscale) {
					dst[dx] = src[f18_floor(src_x)];
				}
				dst = (u32 *)((u8 *)(dst) + job->dstPitch);
			}
		} else if (job->srcPixelMul == 2) {
			const u16 *src = (u16 *)(job->src);
			u16 *dst = (u16 *)(job->dst);

			for (u32 dy = 0; dy < job->height; ++dy, src_y += hscale) {
				src = (u16 *)((u8 *)(job->src) + job->srcPitch * f18_floor(src_y));

				f18 src_x = f18_zero;
				for (u32 dx = 0; dx < job->width; ++dx, src_x += wscale) {
					dst[dx] = src[f18_floor(src_x)];
				}
				dst = (u16 *)((u8 *)(dst) + job->dstPitch);
			}
		}
	} else {
		const size_t widthPitch = job->width * job->dstPixelMul;
		const void *src = (void *)job->src;
		void *dst = (void *)job->dst;

		for (u32 dy = 0; dy < job->height; ++dy) {
			memcpy(dst, src, widthPitch);

			src = (void *)((u8 *)(src) + job->srcPitch);
			dst = (void *)((u8 *)(dst) + job->dstPitch);
		}
	}
}

/*!
 */
static void executeBlit_TextureCopy_32_to_16(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u32 *src = static_cast<const u32 *>(job->src);
	u16 *dst = static_cast<u16 *>(job->dst);

	if (job->stretch) {
		const float wscale = job->x_stretch;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (u32 *)((u8 *)(job->src) + job->srcPitch * src_y);

			for (u32 dx = 0; dx < w; ++dx) {
				const u32 src_x = (u32)(dx * wscale);
				// 16 bit Blitter depends on pre-multiplied color
				const u32 s = PixelLerp32(src[src_x] | 0xFF000000, extractAlpha(src[src_x]));
				dst[dx] = video::A8R8G8B8toA1R5G5B5(s);
			}
			dst = (u16 *)((u8 *)(dst) + job->dstPitch);
		}
	} else {
		for (u32 dy = 0; dy != h; ++dy) {
			for (u32 dx = 0; dx != w; ++dx) {
				// 16 bit Blitter depends on pre-multiplied color
				const u32 s = PixelLerp32(src[dx] | 0xFF000000, extractAlpha(src[dx]));
				dst[dx] = video::A8R8G8B8toA1R5G5B5(s);
			}

			src = (u32 *)((u8 *)(src) + job->srcPitch);
			dst = (u16 *)((u8 *)(dst) + job->dstPitch);
		}
	}
}

/*!
 */
static void executeBlit_TextureCopy_24_to_16(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u8 *src = static_cast<const u8 *>(job->src);
	u16 *dst = static_cast<u16 *>(job->dst);

	if (job->stretch) {
		const float wscale = job->x_stretch * 3.f;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (u8 *)(job->src) + job->srcPitch * src_y;

			for (u32 dx = 0; dx < w; ++dx) {
				const u8 *src_x = src + (u32)(dx * wscale);
				dst[dx] = video::RGBA16(src_x[0], src_x[1], src_x[2]);
			}
			dst = (u16 *)((u8 *)(dst) + job->dstPitch);
		}
	} else {
		for (u32 dy = 0; dy != h; ++dy) {
			const u8 *s = src;
			for (u32 dx = 0; dx != w; ++dx) {
				dst[dx] = video::RGBA16(s[0], s[1], s[2]);
				s += 3;
			}

			src = src + job->srcPitch;
			dst = (u16 *)((u8 *)(dst) + job->dstPitch);
		}
	}
}

/*!
 */
static void executeBlit_TextureCopy_16_to_32(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u16 *src = static_cast<const u16 *>(job->src);
	u32 *dst = static_cast<u32 *>(job->dst);

	if (job->stretch) {
		const float wscale = job->x_stretch;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (u16 *)((u8 *)(job->src) + job->srcPitch * src_y);

			for (u32 dx = 0; dx < w; ++dx) {
				const u32 src_x = (u32)(dx * wscale);
				dst[dx] = video::A1R5G5B5toA8R8G8B8(src[src_x]);
			}
			dst = (u32 *)((u8 *)(dst) + job->dstPitch);
		}
	} else {
		for (u32 dy = 0; dy != h; ++dy) {
			for (u32 dx = 0; dx != w; ++dx) {
				dst[dx] = video::A1R5G5B5toA8R8G8B8(src[dx]);
			}

			src = (u16 *)((u8 *)(src) + job->srcPitch);
			dst = (u32 *)((u8 *)(dst) + job->dstPitch);
		}
	}
}

static void executeBlit_TextureCopy_16_to_24(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u16 *src = static_cast<const u16 *>(job->src);
	u8 *dst = static_cast<u8 *>(job->dst);

	if (job->stretch) {
		const float wscale = job->x_stretch;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (u16 *)((u8 *)(job->src) + job->srcPitch * src_y);

			for (u32 dx = 0; dx < w; ++dx) {
				const u32 src_x = (u32)(dx * wscale);
				u32 color = video::A1R5G5B5toA8R8G8B8(src[src_x]);
				u8 *writeTo = &dst[dx * 3];
				*writeTo++ = (color >> 16) & 0xFF;
				*writeTo++ = (color >> 8) & 0xFF;
				*writeTo++ = color & 0xFF;
			}
			dst += job->dstPitch;
		}
	} else {
		for (u32 dy = 0; dy != h; ++dy) {
			for (u32 dx = 0; dx != w; ++dx) {
				u32 color = video::A1R5G5B5toA8R8G8B8(src[dx]);
				u8 *writeTo = &dst[dx * 3];
				*writeTo++ = (color >> 16) & 0xFF;
				*writeTo++ = (color >> 8) & 0xFF;
				*writeTo++ = color & 0xFF;
			}

			src = (u16 *)((u8 *)(src) + job->srcPitch);
			dst += job->dstPitch;
		}
	}
}

/*!
 */
static void executeBlit_TextureCopy_24_to_32(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u8 *src = static_cast<const u8 *>(job->src);
	u32 *dst = static_cast<u32 *>(job->dst);

	if (job->stretch) {
		const float wscale = job->x_stretch * 3.f;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (const u8 *)job->src + (job->srcPitch * src_y);

			for (u32 dx = 0; dx < w; ++dx) {
				const u8 *s = src + (u32)(dx * wscale);
				dst[dx] = 0xFF000000 | s[0] << 16 | s[1] << 8 | s[2];
			}
			dst = (u32 *)((u8 *)(dst) + job->dstPitch);
		}
	} else {
		for (u32 dy = 0; dy < job->height; ++dy) {
			const u8 *s = src;

			for (u32 dx = 0; dx < job->width; ++dx) {
				dst[dx] = 0xFF000000 | s[0] << 16 | s[1] << 8 | s[2];
				s += 3;
			}

			src = src + job->srcPitch;
			dst = (u32 *)((u8 *)(dst) + job->dstPitch);
		}
	}
}

static void executeBlit_TextureCopy_32_to_24(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u32 *src = static_cast<const u32 *>(job->src);
	u8 *dst = static_cast<u8 *>(job->dst);

	if (job->stretch) {
		const float wscale = job->x_stretch;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (u32 *)((u8 *)(job->src) + job->srcPitch * src_y);

			for (u32 dx = 0; dx < w; ++dx) {
				const u32 src_x = src[(u32)(dx * wscale)];
				u8 *writeTo = &dst[dx * 3];
				*writeTo++ = (src_x >> 16) & 0xFF;
				*writeTo++ = (src_x >> 8) & 0xFF;
				*writeTo++ = src_x & 0xFF;
			}
			dst += job->dstPitch;
		}
	} else {
		for (u32 dy = 0; dy != h; ++dy) {
			for (u32 dx = 0; dx != w; ++dx) {
				u8 *writeTo = &dst[dx * 3];
				*writeTo++ = (src[dx] >> 16) & 0xFF;
				*writeTo++ = (src[dx] >> 8) & 0xFF;
				*writeTo++ = src[dx] & 0xFF;
			}

			src = (u32 *)((u8 *)(src) + job->srcPitch);
			dst += job->dstPitch;
		}
	}
}

/*!
 */
static void executeBlit_TextureBlend_16_to_16(const SBlitJob *job)
{
	const f18 wscale = f32_to_f18(job->x_stretch);
	const f18 hscale = f32_to_f18(job->y_stretch);

	f18 src_y = f18_zero;
	u16 *dst = (u16 *)job->dst;

	for (u32 dy = 0; dy < job->height; ++dy, src_y += hscale) {
		const u16 *src = (u16 *)((u8 *)(job->src) + job->srcPitch * f18_floor(src_y));
		f18 src_x = f18_zero;
		for (u32 dx = 0; dx < job->width; ++dx, src_x += wscale) {
			dst[dx] = PixelBlend16(dst[dx], src[f18_floor(src_x)]);
		}
		dst = (u16 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
 */
static void executeBlit_TextureBlend_32_to_32(const SBlitJob *job)
{
	const f18 wscale = f32_to_f18(job->x_stretch);
	const f18 hscale = f32_to_f18(job->y_stretch);

	f18 src_y = f18_zero;
	u32 *dst = (u32 *)job->dst;
	for (u32 dy = 0; dy < job->height; ++dy, src_y += hscale) {
		const u32 *src = (u32 *)((u8 *)(job->src) + job->srcPitch * f18_floor(src_y));

		f18 src_x = f18_zero;
		for (u32 dx = 0; dx < job->width; ++dx, src_x += wscale) {
			dst[dx] = PixelBlend32(dst[dx], src[f18_floor(src_x)]);
		}
		dst = (u32 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
 */
static void executeBlit_TextureBlendColor_16_to_16(const SBlitJob *job)
{
	const u16 blend = video::A8R8G8B8toA1R5G5B5(job->argb);

	const f18 wscale = f32_to_f18(job->x_stretch);
	const f18 hscale = f32_to_f18(job->y_stretch);

	f18 src_y = f18_zero;
	u16 *dst = (u16 *)job->dst;
	for (u32 dy = 0; dy < job->height; ++dy, src_y += hscale) {
		const u16 *src = (u16 *)((u8 *)(job->src) + job->srcPitch * f18_floor(src_y));
		f18 src_x = f18_zero;
		for (u32 dx = 0; dx < job->width; ++dx, src_x += wscale) {
			u16 c0 = src[f18_floor(src_x)];
			if (0 == (c0 & 0x8000))
				continue;

			dst[dx] = PixelMul16_2(c0, blend);
		}
		dst = (u16 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
 */
static void executeBlit_TextureBlendColor_32_to_32(const SBlitJob *job)
{
	const f18 wscale = f32_to_f18(job->x_stretch);
	const f18 hscale = f32_to_f18(job->y_stretch);

	u32 *dst = (u32 *)job->dst;
	f18 src_y = f18_zero;
	for (u32 dy = 0; dy < job->height; ++dy, src_y += hscale) {
		const u32 *src = (u32 *)((u8 *)(job->src) + job->srcPitch * f18_floor(src_y));

		f18 src_x = f18_zero;
		for (u32 dx = 0; dx < job->width; ++dx, src_x += wscale) {
			dst[dx] = PixelBlend32(dst[dx], PixelMul32_2(src[f18_floor(src_x)], job->argb));
		}
		dst = (u32 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
 */
static void executeBlit_Color_16_to_16(const SBlitJob *job)
{
	const u16 c = video::A8R8G8B8toA1R5G5B5(job->argb);
	u16 *dst = (u16 *)job->dst;

	for (u32 dy = 0; dy < job->height; ++dy) {
		memset16(dst, c, job->srcPitch);
		dst = (u16 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
 */
static void executeBlit_Color_32_to_32(const SBlitJob *job)
{
	u32 *dst = (u32 *)job->dst;

	for (u32 dy = 0; dy < job->height; ++dy) {
		memset32(dst, job->argb, job->srcPitch);
		dst = (u32 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
 */
static void executeBlit_ColorAlpha_16_to_16(const SBlitJob *job)
{
	u16 *dst = (u16 *)job->dst;

	const u16 alpha = extractAlpha(job->argb) >> 3;
	if (0 == alpha)
		return;
	const u32 src = video::A8R8G8B8toA1R5G5B5(job->argb);

	for (u32 dy = 0; dy != job->height; ++dy) {
		for (u32 dx = 0; dx != job->width; ++dx) {
			dst[dx] = PixelBlend16(dst[dx], src, alpha);
		}
		dst = (u16 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
 */
static void executeBlit_ColorAlpha_32_to_32(const SBlitJob *job)
{
	const u32 alpha = extractAlpha(job->argb);
	if (0 == alpha)
		return;

	u32 *dst = (u32 *)job->dst;
	for (u32 dy = 0; dy < job->height; ++dy) {
		for (u32 dx = 0; dx < job->width; ++dx) {
			dst[dx] = PixelBlend32(dst[dx], job->argb, alpha);
		}
		dst = (u32 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
	Pixel =>
			color = sourceAlpha > 0 ? source, else dest
			alpha = max(destAlpha, sourceAlpha)
*/
inline u16 PixelCombine16(const u16 c2, const u16 c1)
{
	if (video::getAlpha(c1) > 0)
		return c1;
	else
		return c2;
}

/*!
	Combine alpha channels (increases alpha / reduces transparency)
*/
static void executeBlit_TextureCombineColor_16_to_16(const SBlitJob *job)
{
	const u32 w = job->width * 2;
	const u32 h = job->height * 2;
	u16 *src = (u16 *)job->src;
	u16 *dst = (u16 *)job->dst;

	const u16 jobColor = video::A8R8G8B8toA1R5G5B5(job->argb);

	/*
		Stretch not supported.
	*/
	for (u32 dy = 0; dy != h; dy++) {
		for (u32 dx = 0; dx != w; dx++) {
			const u16 src_x = src[dx];
			const u16 dst_x = dst[dx];
			dst[dx] = PixelCombine16(dst_x, PixelMul16_2(src_x, jobColor));
		}
		src = (u16 *)((u8 *)(src) + job->srcPitch);
		dst = (u16 *)((u8 *)(dst) + job->dstPitch);
	}
}

/*!
	Combine alpha channels (increases alpha / reduces transparency)
*/
static void executeBlit_TextureCombineColor_16_to_24(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u16 *src = static_cast<const u16 *>(job->src);
	u8 *dst = static_cast<u8 *>(job->dst);

	const u16 jobColor = video::A8R8G8B8toA1R5G5B5(job->argb);

	if (job->stretch) {
		const float wscale = job->x_stretch;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (u16 *)((u8 *)(job->src) + job->srcPitch * src_y);

			for (u32 dx = 0; dx < w; ++dx) {
				const u32 src_x = (u32)(dx * wscale);
				u32 color = PixelMul16_2(video::A1R5G5B5toA8R8G8B8(src[src_x]), jobColor);
				u8 *writeTo = &dst[dx * 3];
				if (video::getAlpha(src[src_x]) > 0) { // only overlay if source has visible alpha (alpha == 1)
					*writeTo++ = (color >> 16) & 0xFF;
					*writeTo++ = (color >> 8) & 0xFF;
					*writeTo++ = color & 0xFF;
				}
			}
			dst += job->dstPitch;
		}
	} else {
		for (u32 dy = 0; dy != h; ++dy) {
			for (u32 dx = 0; dx != w; ++dx) {
				u32 color = PixelMul16_2(video::A1R5G5B5toA8R8G8B8(src[dx]), jobColor);
				u8 *writeTo = &dst[dx * 3];
				if (video::getAlpha(src[dx]) > 0) { // only overlay if source has visible alpha (alpha == 1)
					*writeTo++ = (color >> 16) & 0xFF;
					*writeTo++ = (color >> 8) & 0xFF;
					*writeTo++ = color & 0xFF;
				}
			}

			src = (u16 *)((u8 *)(src) + job->srcPitch);
			dst += job->dstPitch;
		}
	}
}

/*!
	Pixel =>
			color = dest * ( 1 - SourceAlpha ) + source * SourceAlpha,
			alpha = destAlpha * ( 1 - SourceAlpha ) + sourceAlpha

	where "1" means "full scale" (255)
*/
inline u32 PixelCombine32(const u32 c2, const u32 c1)
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
	// stretches [0;255] to [0;256] to avoid division by 255. use division 256 == shr 8
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

	u32 sa = c1 >> 24;
	u32 da = c2 >> 24;
	u32 blendAlpha_fix8 = (sa * 256 + da * (256 - alpha)) >> 8;
	return blendAlpha_fix8 << 24 | rb | xg;
}

/*!
	Combine alpha channels (increases alpha / reduces transparency)
	Destination alpha is treated as full 255
*/
static void executeBlit_TextureCombineColor_32_to_24(const SBlitJob *job)
{
	const u32 w = job->width;
	const u32 h = job->height;
	const u32 *src = static_cast<const u32 *>(job->src);
	u8 *dst = static_cast<u8 *>(job->dst);

	if (job->stretch) {
		const float wscale = job->x_stretch;
		const float hscale = job->y_stretch;

		for (u32 dy = 0; dy < h; ++dy) {
			const u32 src_y = (u32)(dy * hscale);
			src = (u32 *)((u8 *)(job->src) + job->srcPitch * src_y);

			for (u32 dx = 0; dx < w; ++dx) {
				const u32 src_x = src[(u32)(dx * wscale)];
				u8 *writeTo = &dst[dx * 3];
				const u32 dst_x = 0xFF000000 | writeTo[0] << 16 | writeTo[1] << 8 | writeTo[2];
				const u32 combo = PixelCombine32(dst_x, PixelMul32_2(src_x, job->argb));
				*writeTo++ = (combo >> 16) & 0xFF;
				*writeTo++ = (combo >> 8) & 0xFF;
				*writeTo++ = combo & 0xFF;
			}
			dst += job->dstPitch;
		}
	} else {
		for (u32 dy = 0; dy != h; ++dy) {
			for (u32 dx = 0; dx != w; ++dx) {
				u8 *writeTo = &dst[dx * 3];
				const u32 dst_x = 0xFF000000 | writeTo[0] << 16 | writeTo[1] << 8 | writeTo[2];
				const u32 combo = PixelCombine32(dst_x, PixelMul32_2(src[dx], job->argb));
				*writeTo++ = (combo >> 16) & 0xFF;
				*writeTo++ = (combo >> 8) & 0xFF;
				*writeTo++ = combo & 0xFF;
			}

			src = (u32 *)((u8 *)(src) + job->srcPitch);
			dst += job->dstPitch;
		}
	}
}

/*!
	Combine alpha channels (increases alpha / reduces transparency)
*/
static void executeBlit_TextureCombineColor_32_to_32(const SBlitJob *job)
{
	u32 *src = (u32 *)job->src;
	u32 *dst = (u32 *)job->dst;

	for (u32 dy = 0; dy != job->height; ++dy) {
		for (u32 dx = 0; dx != job->width; ++dx) {
			dst[dx] = PixelCombine32(dst[dx], PixelMul32_2(src[dx], job->argb));
		}
		src = (u32 *)((u8 *)(src) + job->srcPitch);
		dst = (u32 *)((u8 *)(dst) + job->dstPitch);
	}
}

// Blitter Operation
enum eBlitter
{
	BLITTER_INVALID = 0,
	BLITTER_COLOR,
	BLITTER_COLOR_ALPHA,
	BLITTER_TEXTURE,
	BLITTER_TEXTURE_ALPHA_BLEND,
	BLITTER_TEXTURE_ALPHA_COLOR_BLEND,
	BLITTER_TEXTURE_COMBINE_ALPHA,
};

typedef void (*tExecuteBlit)(const SBlitJob *job);

/*!
 */
struct blitterTable
{
	eBlitter operation;
	s32 destFormat;
	s32 sourceFormat;
	tExecuteBlit func;
};

static const blitterTable blitTable[] = {
		{BLITTER_TEXTURE,                   -2,                  -2,                  executeBlit_TextureCopy_x_to_x},
		{BLITTER_TEXTURE,                   video::ECF_A1R5G5B5, video::ECF_A8R8G8B8, executeBlit_TextureCopy_32_to_16},
		{BLITTER_TEXTURE,                   video::ECF_A1R5G5B5, video::ECF_R8G8B8,   executeBlit_TextureCopy_24_to_16},
		{BLITTER_TEXTURE,                   video::ECF_A8R8G8B8, video::ECF_A1R5G5B5, executeBlit_TextureCopy_16_to_32},
		{BLITTER_TEXTURE,                   video::ECF_A8R8G8B8, video::ECF_R8G8B8,   executeBlit_TextureCopy_24_to_32},
		{BLITTER_TEXTURE,                   video::ECF_R8G8B8,   video::ECF_A1R5G5B5, executeBlit_TextureCopy_16_to_24},
		{BLITTER_TEXTURE,                   video::ECF_R8G8B8,   video::ECF_A8R8G8B8, executeBlit_TextureCopy_32_to_24},
		{BLITTER_TEXTURE_ALPHA_BLEND,       video::ECF_A1R5G5B5, video::ECF_A1R5G5B5, executeBlit_TextureBlend_16_to_16},
		{BLITTER_TEXTURE_ALPHA_BLEND,       video::ECF_A8R8G8B8, video::ECF_A8R8G8B8, executeBlit_TextureBlend_32_to_32},
		{BLITTER_TEXTURE_ALPHA_COLOR_BLEND, video::ECF_A1R5G5B5, video::ECF_A1R5G5B5, executeBlit_TextureBlendColor_16_to_16},
		{BLITTER_TEXTURE_ALPHA_COLOR_BLEND, video::ECF_A8R8G8B8, video::ECF_A8R8G8B8, executeBlit_TextureBlendColor_32_to_32},
		{BLITTER_COLOR,                     video::ECF_A1R5G5B5, -1,                  executeBlit_Color_16_to_16},
		{BLITTER_COLOR,                     video::ECF_A8R8G8B8, -1,                  executeBlit_Color_32_to_32},
		{BLITTER_COLOR_ALPHA,               video::ECF_A1R5G5B5, -1,                  executeBlit_ColorAlpha_16_to_16},
		{BLITTER_COLOR_ALPHA,               video::ECF_A8R8G8B8, -1,                  executeBlit_ColorAlpha_32_to_32},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_A8R8G8B8, video::ECF_A8R8G8B8, executeBlit_TextureCombineColor_32_to_32},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_A8R8G8B8, video::ECF_R8G8B8,   executeBlit_TextureCopy_24_to_32},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_R8G8B8,   video::ECF_A8R8G8B8, executeBlit_TextureCombineColor_32_to_24},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_R8G8B8,   video::ECF_R8G8B8,   executeBlit_TextureCopy_x_to_x},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_A1R5G5B5, video::ECF_R8G8B8,   executeBlit_TextureCopy_24_to_16},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_A1R5G5B5, video::ECF_A1R5G5B5, executeBlit_TextureCombineColor_16_to_16},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_A1R5G5B5, video::ECF_R8G8B8,   executeBlit_TextureCopy_24_to_16},
		{BLITTER_TEXTURE_COMBINE_ALPHA,     video::ECF_R8G8B8,   video::ECF_A1R5G5B5, executeBlit_TextureCombineColor_16_to_24},
		{BLITTER_INVALID,                   -1,                  -1,                  0},
	};

static inline tExecuteBlit getBlitter2(eBlitter operation, const video::IImage *dest, const video::IImage *source)
{
	video::ECOLOR_FORMAT sourceFormat = (video::ECOLOR_FORMAT)(source ? source->getColorFormat() : -1);
	video::ECOLOR_FORMAT destFormat = (video::ECOLOR_FORMAT)(dest ? dest->getColorFormat() : -1);

	const blitterTable *b = blitTable;

	while (b->operation != BLITTER_INVALID) {
		if (b->operation == operation) {
			if ((b->destFormat == -1 || b->destFormat == destFormat) &&
					(b->sourceFormat == -1 || b->sourceFormat == sourceFormat))
				return b->func;
			else if (b->destFormat == -2 && (sourceFormat == destFormat))
				return b->func;
		}
		b += 1;
	}
	return 0;
}

// bounce clipping to texture
inline void setClip(AbsRectangle &out, const core::rect<s32> *clip,
		const video::IImage *tex, s32 passnative, const core::dimension2d<u32> *tex_org)
{
	if (0 == tex) {
		if (clip && passnative) {
			out.x0 = clip->UpperLeftCorner.X;
			out.x1 = clip->LowerRightCorner.X;
			out.y0 = clip->UpperLeftCorner.Y;
			out.y1 = clip->LowerRightCorner.Y;
		} else {
			out.x0 = 0;
			out.x1 = 0;
			out.y0 = 0;
			out.y1 = 0;
		}
		return;
	}

	const s32 w = tex->getDimension().Width;
	const s32 h = tex->getDimension().Height;

	// driver could have changed texture size.
	if (clip && tex_org && ((u32)w != tex_org->Width || (u32)h != tex_org->Height)) {
		out.x0 = core::s32_clamp((clip->UpperLeftCorner.X * w) / tex_org->Width, 0, w - 1);
		out.x1 = core::s32_clamp((clip->LowerRightCorner.X * w) / tex_org->Width, out.x0, w);
		out.y0 = core::s32_clamp((clip->UpperLeftCorner.Y * h) / tex_org->Height, 0, h - 1);
		out.y1 = core::s32_clamp((clip->LowerRightCorner.Y * h) / tex_org->Height, out.y0, h);
	} else if (clip) {
		// y-1 to prevent starting on illegal memory (not ideal!).
		out.x0 = core::s32_clamp(clip->UpperLeftCorner.X, 0, w - 1);
		out.x1 = core::s32_clamp(clip->LowerRightCorner.X, passnative ? 0 : out.x0, w);
		out.y0 = core::s32_clamp(clip->UpperLeftCorner.Y, 0, h - 1);
		out.y1 = core::s32_clamp(clip->LowerRightCorner.Y, passnative ? 0 : out.y0, h);
	} else {
		out.x0 = 0;
		out.y0 = 0;
		out.x1 = w;
		out.y1 = h;
	}
}

/*!
	a generic 2D Blitter
*/
static s32 Blit(eBlitter operation,
		video::IImage *dest,
		const core::rect<s32> *destClipping,
		const core::position2d<s32> *destPos,
		video::IImage *const source,
		const core::rect<s32> *sourceClipping,
		u32 argb)
{
	tExecuteBlit blitter = getBlitter2(operation, dest, source);
	if (0 == blitter) {
		return 0;
	}

	// Clipping
	AbsRectangle sourceClip;
	AbsRectangle destClip;
	AbsRectangle v;

	SBlitJob job;

	setClip(sourceClip, sourceClipping, source, 1, 0);
	setClip(destClip, destClipping, dest, 0, 0);

	v.x0 = destPos ? destPos->X : 0;
	v.y0 = destPos ? destPos->Y : 0;
	v.x1 = v.x0 + (sourceClip.x1 - sourceClip.x0);
	v.y1 = v.y0 + (sourceClip.y1 - sourceClip.y0);

	if (!intersect(job.Dest, destClip, v))
		return 0;

	job.width = job.Dest.x1 - job.Dest.x0;
	job.height = job.Dest.y1 - job.Dest.y0;

	job.Source.x0 = sourceClip.x0 + (job.Dest.x0 - v.x0);
	job.Source.x1 = job.Source.x0 + job.width;
	job.Source.y0 = sourceClip.y0 + (job.Dest.y0 - v.y0);
	job.Source.y1 = job.Source.y0 + job.height;

	job.argb = argb;

	job.stretch = false;
	job.x_stretch = 1.f;
	job.y_stretch = 1.f;

	if (source) {
		job.srcPitch = source->getPitch();
		job.srcPixelMul = source->getBytesPerPixel();
		job.src = (void *)((u8 *)source->getData() + (job.Source.y0 * job.srcPitch) + (job.Source.x0 * job.srcPixelMul));
	} else {
		// use srcPitch for color operation on dest
		job.srcPitch = job.width * dest->getBytesPerPixel();
	}

	job.dstPitch = dest->getPitch();
	job.dstPixelMul = dest->getBytesPerPixel();
	job.dst = (void *)((u8 *)dest->getData() + (job.Dest.y0 * job.dstPitch) + (job.Dest.x0 * job.dstPixelMul));

	blitter(&job);

	return 1;
}
