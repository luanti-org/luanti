// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IImage.h"
#include "rect.h"

namespace video
{

//! check sanity of image dimensions to prevent issues later, for use by CImageLoaders
inline bool checkImageDimensions(u32 width, u32 height)
{
	// 4 * 23000 * 23000 is just under S32_MAX
	return width <= 23000 && height <= 23000;
}

//! IImage implementation with a lot of special image operations for
//! 16 bit A1R5G5B5/32 Bit A8R8G8B8 images, which are used by the SoftwareDevice.
class CImage final : public IImage
{
public:
	//! constructor from raw image data
	/** \param useForeignMemory: If true, the image will use the data pointer
	directly and own it from now on, which means it will also try to delete [] the
	data when the image will be destructed. If false, the memory will by copied. */
	CImage(ECOLOR_FORMAT format, const core::dimension2d<u32> &size, void *data,
			bool ownForeignMemory = true, bool deleteMemory = true);

	//! constructor for empty image
	CImage(ECOLOR_FORMAT format, const core::dimension2d<u32> &size);

	//! returns a pixel
	SColor getPixel(u32 x, u32 y) const override;

	//! sets a pixel
	void setPixel(u32 x, u32 y, const SColor &color, bool blend = false) override;

	//! copies this surface into another, if it has the exact same size and format.
	bool copyToNoScaling(void *target, u32 width, u32 height, ECOLOR_FORMAT format, u32 pitch = 0) const override;

	//! copies this surface into another, scaling it to fit.
	void copyToScaling(void *target, u32 width, u32 height, ECOLOR_FORMAT format, u32 pitch = 0) override;

	//! copies this surface into another, scaling it to fit.
	void copyToScaling(IImage *target) override;

	//! copies this surface into another
	void copyTo(IImage *target, const core::position2d<s32> &pos = core::position2d<s32>(0, 0)) override;

	//! copies this surface into another
	void copyTo(IImage *target, const core::position2d<s32> &pos, const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0) override;

	//! copies this surface into another, scaling it to fit, applying a box filter
	void copyToScalingBoxFilter(IImage *target, s32 bias = 0, bool blend = false) override;

	//! fills the surface with given color
	void fill(const SColor &color) override;

private:
	inline SColor getPixelBox(s32 x, s32 y, s32 fx, s32 fy, s32 bias) const;
};

} // end namespace video
