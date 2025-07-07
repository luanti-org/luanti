// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "position2d.h"
#include "rect.h"
#include "SColor.h"
#include <cstring>

namespace video
{

//! Interface for software image data.
/** Image loaders create these images from files. IVideoDrivers convert
these images into their (hardware) textures.
NOTE: Floating point formats are not well supported yet. Basically only getData() works for them.
*/
class IImage : public virtual IReferenceCounted
{
public:
	//! constructor
	IImage(ECOLOR_FORMAT format, const core::dimension2d<u32> &size, bool deleteMemory) :
			Format(format), Size(size), Data(0), BytesPerPixel(0), Pitch(0), DeleteMemory(deleteMemory)
	{
		BytesPerPixel = getBitsPerPixelFromFormat(Format) / 8;
		Pitch = BytesPerPixel * Size.Width;
	}

	//! destructor
	virtual ~IImage()
	{
		if (DeleteMemory)
			delete[] Data;
	}

	//! Returns the color format
	ECOLOR_FORMAT getColorFormat() const
	{
		return Format;
	}

	//! Returns width and height of image data.
	const core::dimension2d<u32> &getDimension() const
	{
		return Size;
	}

	//! Returns bits per pixel.
	u32 getBitsPerPixel() const
	{
		return getBitsPerPixelFromFormat(Format);
	}

	//! Returns bytes per pixel
	u32 getBytesPerPixel() const
	{
		return BytesPerPixel;
	}

	//! Returns image data size in bytes
	u32 getImageDataSizeInBytes() const
	{
		return getDataSizeFromFormat(Format, Size.Width, Size.Height);
	}

	//! Returns image data size in pixels
	u32 getImageDataSizeInPixels() const
	{
		return Size.Width * Size.Height;
	}

	//! Returns pitch of image
	u32 getPitch() const
	{
		return Pitch;
	}

	//! Returns mask for red value of a pixel
	u32 getRedMask() const
	{
		switch (Format) {
		case ECF_A1R5G5B5:
			return 0x1F << 10;
		case ECF_R5G6B5:
			return 0x1F << 11;
		case ECF_R8G8B8:
			return 0x00FF0000;
		case ECF_A8R8G8B8:
			return 0x00FF0000;
		default:
			return 0x0;
		}
	}

	//! Returns mask for green value of a pixel
	u32 getGreenMask() const
	{
		switch (Format) {
		case ECF_A1R5G5B5:
			return 0x1F << 5;
		case ECF_R5G6B5:
			return 0x3F << 5;
		case ECF_R8G8B8:
			return 0x0000FF00;
		case ECF_A8R8G8B8:
			return 0x0000FF00;
		default:
			return 0x0;
		}
	}

	//! Returns mask for blue value of a pixel
	u32 getBlueMask() const
	{
		switch (Format) {
		case ECF_A1R5G5B5:
			return 0x1F;
		case ECF_R5G6B5:
			return 0x1F;
		case ECF_R8G8B8:
			return 0x000000FF;
		case ECF_A8R8G8B8:
			return 0x000000FF;
		default:
			return 0x0;
		}
	}

	//! Returns mask for alpha value of a pixel
	u32 getAlphaMask() const
	{
		switch (Format) {
		case ECF_A1R5G5B5:
			return 0x1 << 15;
		case ECF_R5G6B5:
			return 0x0;
		case ECF_R8G8B8:
			return 0x0;
		case ECF_A8R8G8B8:
			return 0xFF000000;
		default:
			return 0x0;
		}
	}

	//! Use this to get a pointer to the image data.
	/**
	\return Pointer to the image data. What type of data is pointed to
	depends on the color format of the image. For example if the color
	format is ECF_A8R8G8B8, it is of u32. */
	void *getData() const
	{
		return Data;
	}

	//! Get the mipmap size for this image for a certain mipmap level
	/** level 0 will be full image size. Every further level is half the size.
		Doesn't care if the image actually has mipmaps, just which size would be needed. */
	core::dimension2du getMipMapsSize(u32 mipmapLevel) const
	{
		return getMipMapsSize(Size, mipmapLevel);
	}

	//! Calculate mipmap size for a certain level
	/** level 0 will be full image size. Every further level is half the size.	*/
	static core::dimension2du getMipMapsSize(const core::dimension2du &sizeLevel0, u32 mipmapLevel)
	{
		core::dimension2du result(sizeLevel0);
		u32 i = 0;
		while (i != mipmapLevel) {
			if (result.Width > 1)
				result.Width >>= 1;
			if (result.Height > 1)
				result.Height >>= 1;
			++i;

			if (result.Width == 1 && result.Height == 1 && i < mipmapLevel)
				return core::dimension2du(0, 0);
		}
		return result;
	}

	//! Returns a pixel
	virtual SColor getPixel(u32 x, u32 y) const = 0;

	//! Sets a pixel
	virtual void setPixel(u32 x, u32 y, const SColor &color, bool blend = false) = 0;

	//! Copies this surface into another, if it has the exact same size and format.
	/**	\return True if it was copied, false otherwise.
	*/
	virtual bool copyToNoScaling(void *target, u32 width, u32 height, ECOLOR_FORMAT format = ECF_A8R8G8B8, u32 pitch = 0) const = 0;

	//! Copies the image into the target, scaling the image to fit
	virtual void copyToScaling(void *target, u32 width, u32 height, ECOLOR_FORMAT format = ECF_A8R8G8B8, u32 pitch = 0) = 0;

	//! Copies the image into the target, scaling the image to fit
	virtual void copyToScaling(IImage *target) = 0;

	//! copies this surface into another
	virtual void copyTo(IImage *target, const core::position2d<s32> &pos = core::position2d<s32>(0, 0)) = 0;

	//! copies this surface into another
	virtual void copyTo(IImage *target, const core::position2d<s32> &pos, const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0) = 0;

	//! copies this surface into another, scaling it to fit, applying a box filter
	virtual void copyToScalingBoxFilter(IImage *target, s32 bias = 0, bool blend = false) = 0;

	//! fills the surface with given color
	virtual void fill(const SColor &color) = 0;

	//! get the amount of Bits per Pixel of the given color format
	static u32 getBitsPerPixelFromFormat(const ECOLOR_FORMAT format)
	{
		switch (format) {
		case ECF_A1R5G5B5:
			return 16;
		case ECF_R5G6B5:
			return 16;
		case ECF_R8G8B8:
			return 24;
		case ECF_A8R8G8B8:
			return 32;
		case ECF_D16:
			return 16;
		case ECF_D24:
			return 32;
		case ECF_D32:
			return 32;
		case ECF_D24S8:
			return 32;
		case ECF_R8:
			return 8;
		case ECF_R8G8:
			return 16;
		case ECF_R16:
			return 16;
		case ECF_R16G16:
			return 32;
		case ECF_A2R10G10B10:
			return 32;
		case ECF_R16F:
			return 16;
		case ECF_G16R16F:
			return 32;
		case ECF_A16B16G16R16F:
			return 64;
		case ECF_R32F:
			return 32;
		case ECF_G32R32F:
			return 64;
		case ECF_A32B32G32R32F:
			return 128;
		default:
			return 0;
		}
	}

	//! calculate image data size in bytes for selected format, width and height.
	static u32 getDataSizeFromFormat(ECOLOR_FORMAT format, u32 width, u32 height)
	{
		// non-compressed formats
		u32 imageSize = getBitsPerPixelFromFormat(format) / 8 * width;
		imageSize *= height;

		return imageSize;
	}

	//! check if this is compressed color format
	static bool isCompressedFormat(const ECOLOR_FORMAT format)
	{
		return false;
	}

	//! check if the color format is only viable for depth/stencil textures
	static bool isDepthFormat(const ECOLOR_FORMAT format)
	{
		switch (format) {
		case ECF_D16:
		case ECF_D24:
		case ECF_D32:
		case ECF_D24S8:
			return true;
		default:
			return false;
		}
	}

	//! Check if the color format uses floating point values for pixels
	static bool isFloatingPointFormat(const ECOLOR_FORMAT format)
	{
		if (isCompressedFormat(format))
			return false;

		switch (format) {
		case ECF_R16F:
		case ECF_G16R16F:
		case ECF_A16B16G16R16F:
		case ECF_R32F:
		case ECF_G32R32F:
		case ECF_A32B32G32R32F:
			return true;
		default:
			break;
		}
		return false;
	}

protected:
	ECOLOR_FORMAT Format;
	core::dimension2d<u32> Size;

	u8 *Data;

	u32 BytesPerPixel;
	u32 Pitch;

	bool DeleteMemory;
};

} // end namespace video
