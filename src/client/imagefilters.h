// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 Aaron Suen <warr1024@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <rect.h>
#include <SColor.h>

namespace video
{
	class IImage;
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
void imageCleanTransparent(video::IImage *src, u32 threshold);

/* Returns the gamma-correct average color of the image, with transparent pixels
 * ignored. */
video::SColor imageAverageColor(const video::IImage *img);

/* Scale a region of an image into another image, using nearest-neighbor with
 * anti-aliasing; treat pixels as crisp rectangles, but blend them at boundaries
 * to prevent non-integer scaling ratio artifacts.  Note that this may cause
 * some blending at the edges where pixels don't line up perfectly, but this
 * filter is designed to produce the most accurate results for both upscaling
 * and downscaling.
 */
void imageScaleNNAA(video::IImage *src, const core::rect<s32> &srcrect, video::IImage *dest);
