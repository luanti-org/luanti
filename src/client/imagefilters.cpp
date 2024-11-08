// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 Aaron Suen <warr1024@gmail.com>

#include "imagefilters.h"
#include "util/numeric.h"
#include <cmath>
#include <cassert>
#include <vector>
#include <algorithm>
#include <IVideoDriver.h>

// Simple 2D bitmap class with just the functionality needed here
class Bitmap {
	u32 linesize, lines;
	std::vector<u8> data;

	static inline u32 bytepos(u32 index) { return index >> 3; }
	static inline u8 bitpos(u32 index) { return index & 7; }

public:
	Bitmap(u32 width, u32 height) :  linesize(width), lines(height),
		data(bytepos(width * height) + 1) {}

	inline bool get(u32 x, u32 y) const {
		u32 index = y * linesize + x;
		return data[bytepos(index)] & (1 << bitpos(index));
	}

	inline void set(u32 x, u32 y) {
		u32 index = y * linesize + x;
		data[bytepos(index)] |= 1 << bitpos(index);
	}

	inline bool all() const {
		for (u32 i = 0; i < data.size() - 1; i++) {
			if (data[i] != 0xff)
				return false;
		}
		// last byte not entirely filled
		for (u8 i = 0; i < bitpos(linesize * lines); i++) {
			bool value_of_bit = data.back() & (1 << i);
			if (!value_of_bit)
				return false;
		}
		return true;
	}

	inline void copy(Bitmap &to) const {
		assert(to.linesize == linesize && to.lines == lines);
		to.data = data;
	}
};

template <bool IS_A8R8G8B8>
static void imageCleanTransparentWithInlining(video::IImage *src, u32 threshold)
{
	void *const src_data = src->getData();
	const core::dimension2d<u32> dim = src->getDimension();

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
	for (u32 ctry = 0; ctry < dim.Height; ctry++)
	for (u32 ctrx = 0; ctrx < dim.Width; ctrx++) {
		if (get_pixel(ctrx, ctry).getAlpha() > threshold)
			bitmap.set(ctrx, ctry);
	}

	// Exit early if all pixels opaque
	if (bitmap.all())
		return;

	Bitmap newmap = bitmap;

	// Cap iterations to keep runtime reasonable, for higher-res textures we can
	// get away with filling less pixels.
	int iter_max = 11 - std::max(dim.Width, dim.Height) / 16;
	iter_max = std::max(iter_max, 2);

	// Then repeatedly look for transparent pixels, filling them in until
	// we're finished.
	for (int iter = 0; iter < iter_max; iter++) {

	for (u32 ctry = 0; ctry < dim.Height; ctry++)
	for (u32 ctrx = 0; ctrx < dim.Width; ctrx++) {
		// Skip pixels we have already processed
		if (bitmap.get(ctrx, ctry))
			continue;

		// Sample size and total weighted r, g, b values
		u32 ss = 0, sr = 0, sg = 0, sb = 0;

		// Walk each neighbor pixel (clipped to image bounds)
		for (u32 sy = (ctry < 1) ? 0 : (ctry - 1);
				sy <= (ctry + 1) && sy < dim.Height; sy++)
		for (u32 sx = (ctrx < 1) ? 0 : (ctrx - 1);
				sx <= (ctrx + 1) && sx < dim.Width; sx++) {
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

		// Set pixel to average weighted by alpha
		if (ss > 0) {
			video::SColor c = get_pixel(ctrx, ctry);
			c.setRed(sr / ss);
			c.setGreen(sg / ss);
			c.setBlue(sb / ss);
			set_pixel(ctrx, ctry, c);
			newmap.set(ctrx, ctry);
		}
	}

	if (newmap.all())
		return;

	// Apply changes to bitmap for next run. This is done so we don't introduce
	// a bias in color propagation in the direction pixels are processed.
	newmap.copy(bitmap);

	}
}

/* Fill in RGB values for transparent pixels, to correct for odd colors
 * appearing at borders when blending.  This is because many PNG optimizers
 * like to discard RGB values of transparent pixels, but when blending then
 * with non-transparent neighbors, their RGB values will show up nonetheless.
 *
 * This function modifies the original image in-place.
 *
 * Parameter "threshold" is the alpha level below which pixels are considered
 * transparent. Should be 127 when the texture is used with ALPHA_CHANNEL_REF,
 * 0 when alpha blending is used.
 */
void imageCleanTransparent(video::IImage *src, u32 threshold)
{
	if (src->getColorFormat() == video::ECF_A8R8G8B8)
		imageCleanTransparentWithInlining<true>(src, threshold);
	else
		imageCleanTransparentWithInlining<false>(src, threshold);
}

/* Scale a region of an image into another image, using nearest-neighbor with
 * anti-aliasing; treat pixels as crisp rectangles, but blend them at boundaries
 * to prevent non-integer scaling ratio artifacts.  Note that this may cause
 * some blending at the edges where pixels don't line up perfectly, but this
 * filter is designed to produce the most accurate results for both upscaling
 * and downscaling.
 */
void imageScaleNNAA(video::IImage *src, const core::rect<s32> &srcrect, video::IImage *dest)
{
	double sx, sy, minsx, maxsx, minsy, maxsy, area, ra, ga, ba, aa, pw, ph, pa;
	u32 dy, dx;
	video::SColor pxl;

	// Cache rectangle boundaries.
	double sox = srcrect.UpperLeftCorner.X * 1.0;
	double soy = srcrect.UpperLeftCorner.Y * 1.0;
	double sw = srcrect.getWidth() * 1.0;
	double sh = srcrect.getHeight() * 1.0;

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
			SWAP(double, minsx, maxsx);
		minsy = soy + (dy * sh / dim.Height);
		minsy = rangelim(minsy, 0, soy + sh);
		maxsy = minsy + sh / dim.Height;
		maxsy = rangelim(maxsy, 0, soy + sh);
		if (minsy > maxsy)
			SWAP(double, minsy, maxsy);

		// Total area, and integral of r, g, b values over that area,
		// initialized to zero, to be summed up in next loops.
		area = 0;
		ra = 0;
		ga = 0;
		ba = 0;
		aa = 0;

		// Loop over the integral pixel positions described by those bounds.
		for (sy = floor(minsy); sy < maxsy; sy++)
		for (sx = floor(minsx); sx < maxsx; sx++) {

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
			pxl.setRed(ra / area + 0.5);
			pxl.setGreen(ga / area + 0.5);
			pxl.setBlue(ba / area + 0.5);
			pxl.setAlpha(aa / area + 0.5);
		} else {
			pxl.setRed(0);
			pxl.setGreen(0);
			pxl.setBlue(0);
			pxl.setAlpha(0);
		}
		dest->setPixel(dx, dy, pxl);
	}
}

/* Check and align image to npot2 if required by hardware
 * @param image image to check for npot2 alignment
 * @param driver driver to use for image operations
 * @return image or copy of image aligned to npot2
 */
video::IImage *Align2Npot2(video::IImage *image, video::IVideoDriver *driver)
{
	if (image == nullptr)
		return image;

	if (driver->queryFeature(video::EVDF_TEXTURE_NPOT))
		return image;

	core::dimension2d<u32> dim = image->getDimension();
	unsigned int height = npot2(dim.Height);
	unsigned int width  = npot2(dim.Width);

	if (dim.Height == height && dim.Width == width)
		return image;

	if (dim.Height > height)
		height *= 2;
	if (dim.Width > width)
		width *= 2;

	video::IImage *targetimage =
			driver->createImage(video::ECF_A8R8G8B8,
					core::dimension2d<u32>(width, height));

	if (targetimage != nullptr)
		image->copyToScaling(targetimage);
	image->drop();
	return targetimage;
}
