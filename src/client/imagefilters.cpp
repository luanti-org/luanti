// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 Aaron Suen <warr1024@gmail.com>

#include "imagefilters.h"
#include "util/numeric.h"
#include "util/bitmap.h"
#include "exceptions.h"
#include <cmath>
#include <cassert>
#include <algorithm>
#include <IVideoDriver.h>

template <bool IS_A8R8G8B8>
static void imageCleanTransparentWithInlining(video::IImage *src, u32 threshold)
{
	void *const src_data = src->getData();
	const core::dimension2d<u32> dim = src->getDimension();

	// position math helpers
	auto addp = [=](v2u32 p, v2u32 operand) -> v2u32 {
		return {
			std::min(p.X + operand.X, dim.Width),
			std::min(p.Y + operand.Y, dim.Height),
		};
	};
	auto subp = [=](v2u32 p, v2u32 operand) -> v2u32 {
		return {
			p.X <= operand.X ? 0 : (p.X - operand.X),
			p.Y <= operand.Y ? 0 : (p.Y - operand.Y),
		};
	};

	// pixel accessors
	auto get_pixel = [=](u32 x, u32 y) -> video::SColor {
		if constexpr (IS_A8R8G8B8) {
			return reinterpret_cast<u32 *>(src_data)[y*dim.Width + x];
		} else {
			return src->getPixel(x, y);
		}
	};
	auto set_pixel = [=](u32 x, u32 y, video::SColor color) {
		if constexpr (IS_A8R8G8B8) {
			u32 *dest = &reinterpret_cast<u32 *>(src_data)[y*dim.Width + x];
			*dest = color.color;
		} else {
			src->setPixel(x, y, color);
		}
	};

	Bitmap bitmap(dim.Width, dim.Height);

	// First pass: Mark all opaque pixels
	// Note: loop y around x for better cache locality.
	v2u32 bmin = dim, bmax = {0,0}; // bounding box of opaque pixels
	for (v2u32 pp; pp.Y < dim.Height; pp.Y++)
	for (pp.X = 0; pp.X < dim.Width; pp.X++) {
		if (get_pixel(pp.X, pp.Y).getAlpha() > threshold) {
			bitmap.set(pp.X, pp.Y);
			bmin = componentwise_min(bmin, pp);
			bmax = componentwise_max(bmax, pp);
		}
	}

	// Exit early if there is nothing to propagate
	if (bitmap.all() || bitmap.none())
		return;
	assert(bmin <= bmax);

	Bitmap newmap = bitmap;

	// Cap iterations to keep runtime reasonable, for higher-res textures we can
	// get away with filling less pixels.
	int iter_max = 11 - std::max(dim.Width, dim.Height) / 16;
	iter_max = std::max(iter_max, 2);

	// Then repeatedly look for transparent pixels, filling them in until
	// we're finished.
	for (int iter = 0; iter < iter_max; iter++) {

	// We can only make progress on pixels that have any neighbors. We're keeping
	// track so only iterate the relevant area.
	const v2u32 cstart = subp(bmin, {1,1}), cend = addp(bmax, {2,2});
	v2u32 cp;
	for (cp.Y = cstart.Y; cp.Y < cend.Y; cp.Y++)
	for (cp.X = cstart.X; cp.X < cend.X; cp.X++) {
		// Skip pixels we have already processed
		if (bitmap.get(cp.X, cp.Y))
			continue;

		u32 ss = 0, sr = 0, sg = 0, sb = 0;

		// Walk nine neighbor pixels (clipped to image bounds)
		const v2u32 sstart = subp(cp, {1,1}), send = addp(cp, {2,2});
		for (u32 sy = sstart.Y; sy < send.Y; sy++)
		for (u32 sx = sstart.X; sx < send.X; sx++) {
			// Ignore pixels we haven't processed
			if (!bitmap.get(sx, sy))
				continue;

			// Add RGB values weighted by alpha IF the pixel is opaque, otherwise
			// use full weight since we want to propagate colors.
			video::SColor d = get_pixel(sx, sy);
			u32 a = d.getAlpha() <= threshold ? 255 : d.getAlpha();
			ss += a;
			sr += a * d.getRed();
			sg += a * d.getGreen();
			sb += a * d.getBlue();
		}

		// Set color to average weighted by alpha
		if (ss > 0) {
			video::SColor c = get_pixel(cp.X, cp.Y);
			c.setRed(sr / ss);
			c.setGreen(sg / ss);
			c.setBlue(sb / ss);
			set_pixel(cp.X, cp.Y, c);

			newmap.set(cp.X, cp.Y);
			bmin = componentwise_min(bmin, cp);
			bmax = componentwise_max(bmax, cp);
		}
	}

	if (newmap.all())
		return;

	// Apply changes to bitmap for next run. This is done so we don't introduce
	// a bias in color propagation in the direction pixels are processed.
	bitmap = newmap;

	}
}

void imageCleanTransparent(video::IImage *src, u32 threshold)
{
	if (src->getColorFormat() == video::ECF_A8R8G8B8)
		imageCleanTransparentWithInlining<true>(src, threshold);
	else
		imageCleanTransparentWithInlining<false>(src, threshold);
}

/**********************************/

namespace {
	// For more colorspace transformations, see for example
	// <https://github.com/tobspr/GLSL-Color-Spaces/blob/master/ColorSpaces.inc.glsl>

	inline float linear_to_srgb_component(float v)
	{
		if (v > 0.0031308f)
			return 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
		return 12.92f * v;
	}
	inline float srgb_to_linear_component(float v)
	{
		if (v > 0.04045f)
			return powf((v + 0.055f) / 1.055f, 2.4f);
		return v / 12.92f;
	}

	template <float (*F)(float)>
	struct LUT8 {
		std::array<float, 256> t;
		LUT8() {
			for (size_t i = 0; i < t.size(); i++)
				t[i] = F(i / 255.0f);
		}
	};
	LUT8<srgb_to_linear_component> srgb_to_linear_lut;

	v3f srgb_to_linear(const video::SColor col_srgb)
	{
		v3f col(srgb_to_linear_lut.t[col_srgb.getRed()],
			srgb_to_linear_lut.t[col_srgb.getGreen()],
			srgb_to_linear_lut.t[col_srgb.getBlue()]);
		return col;
	}

	video::SColor linear_to_srgb(const v3f col_linear)
	{
		v3f col;
		// we can't LUT this without losing precision, but thankfully we call
		// it just once :)
		col.X = linear_to_srgb_component(col_linear.X);
		col.Y = linear_to_srgb_component(col_linear.Y);
		col.Z = linear_to_srgb_component(col_linear.Z);
		col *= 255.0f;
		col.X = core::clamp<float>(col.X, 0.0f, 255.0f);
		col.Y = core::clamp<float>(col.Y, 0.0f, 255.0f);
		col.Z = core::clamp<float>(col.Z, 0.0f, 255.0f);
		return video::SColor(0xff, myround(col.X), myround(col.Y),
			myround(col.Z));
	}
}

template <bool IS_A8R8G8B8>
static video::SColor imageAverageColorInline(const video::IImage *src)
{
	void *const src_data = src->getData();
	const core::dimension2du dim = src->getDimension();

	auto get_pixel = [=](u32 x, u32 y) -> video::SColor {
		if constexpr (IS_A8R8G8B8) {
			return reinterpret_cast<u32 *>(src_data)[y*dim.Width + x];
		} else {
			return src->getPixel(x, y);
		}
	};

	u32 total = 0;
	v3f col_acc;
	// limit runtime cost
	const u32 stepx = std::max(1U, dim.Width / 16),
		stepy = std::max(1U, dim.Height / 16);
	for (u32 y = 0; y < dim.Height; y += stepy) {
		for (u32 x = 0; x < dim.Width; x += stepx) {
			video::SColor c = get_pixel(x, y);
			if (c.getAlpha() > 0) {
				total++;
				col_acc += srgb_to_linear(c);
			}
		}
	}

	video::SColor ret(0, 0, 0, 0);
	if (total > 0) {
		col_acc /= total;
		ret = linear_to_srgb(col_acc);
	}
	ret.setAlpha(255);
	return ret;
}

video::SColor imageAverageColor(const video::IImage *img)
{
	if (img->getColorFormat() == video::ECF_A8R8G8B8)
		return imageAverageColorInline<true>(img);
	else
		return imageAverageColorInline<false>(img);
}

/**********************************/

void imageScaleNNAA(video::IImage *src, const core::rect<s32> &srcrect, video::IImage *dest)
{
	f32 sx, sy, minsx, maxsx, minsy, maxsy, area, ra, ga, ba, aa, pw, ph, pa;
	u32 dy, dx;
	video::SColor pxl;

	// Cache rectangle boundaries.
	const f32 sox = srcrect.UpperLeftCorner.X;
	const f32 soy = srcrect.UpperLeftCorner.Y;
	const f32 sw = srcrect.getWidth();
	const f32 sh = srcrect.getHeight();

	// Walk each destination image pixel.
	// Note: loop y around x for better cache locality.
	core::dimension2d<u32> dim = dest->getDimension();
	for (dy = 0; dy < dim.Height; dy++)
	for (dx = 0; dx < dim.Width; dx++) {

		// Calculate floating-point source rectangle bounds.
		// Do some basic clipping, and for mirrored/flipped rects,
		// make sure min/max are in the right order.
		minsx = sox + (dx * sw / dim.Width);
		minsx = rangelim(minsx, 0, sox + sw);
		maxsx = minsx + sw / dim.Width;
		maxsx = rangelim(maxsx, 0, sox + sw);
		if (minsx > maxsx)
			std::swap(minsx, maxsx);
		minsy = soy + (dy * sh / dim.Height);
		minsy = rangelim(minsy, 0, soy + sh);
		maxsy = minsy + sh / dim.Height;
		maxsy = rangelim(maxsy, 0, soy + sh);
		if (minsy > maxsy)
			std::swap(minsy, maxsy);

		// Total area, and integral of r, g, b values over that area,
		// initialized to zero, to be summed up in next loops.
		area = 0;
		ra = 0;
		ga = 0;
		ba = 0;
		aa = 0;

		// Loop over the integral pixel positions described by those bounds.
		for (sy = std::floor(minsy); sy < maxsy; sy++)
		for (sx = std::floor(minsx); sx < maxsx; sx++) {

			// Calculate width, height, then area of dest pixel
			// that's covered by this source pixel.
			pw = 1;
			if (minsx > sx)
				pw += sx - minsx;
			if (maxsx < (sx + 1))
				pw += maxsx - sx - 1;
			ph = 1;
			if (minsy > sy)
				ph += sy - minsy;
			if (maxsy < (sy + 1))
				ph += maxsy - sy - 1;
			pa = pw * ph;

			// Get source pixel and add it to totals, weighted
			// by covered area and alpha.
			pxl = src->getPixel((u32)sx, (u32)sy);
			area += pa;
			ra += pa * pxl.getRed();
			ga += pa * pxl.getGreen();
			ba += pa * pxl.getBlue();
			aa += pa * pxl.getAlpha();
		}

		// Set the destination image pixel to the average color.
		if (area > 0) {
			pxl.setRed(ra / area + 0.5f);
			pxl.setGreen(ga / area + 0.5f);
			pxl.setBlue(ba / area + 0.5f);
			pxl.setAlpha(aa / area + 0.5f);
		} else {
			pxl.setRed(0);
			pxl.setGreen(0);
			pxl.setBlue(0);
			pxl.setAlpha(0);
		}
		dest->setPixel(dx, dy, pxl);
	}
}

/**********************************/

void imageApplyMask(video::IImage *dest, const video::IImage *mask)
{
	if (dest->getColorFormat() != mask->getColorFormat())
		throw BaseException("imageApplyMask: color formats do not match");
	if (dest->getDimension() != mask->getDimension())
		throw BaseException("imageApplyMask: dimensions do not match");

	// Now it's trivial: just run through the entire buffer
	u8 *const dest_data = reinterpret_cast<u8*>(dest->getData());
	const u8 *const mask_data = reinterpret_cast<u8*>(mask->getData());
	const size_t nbytes = dest->getPitch() * dest->getDimension().Height;
	for (size_t i = 0; i < nbytes; i++)
		dest_data[i] &= mask_data[i];
}
