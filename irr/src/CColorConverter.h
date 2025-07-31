// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include "IImage.h"

namespace video
{

class CColorConverter
{
public:

	//! converts a 8 bit palettized or non palettized image (A8) into R8G8B8
	static void convert8BitTo24Bit(const u8 *in, u8 *out, s32 width, s32 height, const u8 *palette, s32 linepad = 0, bool flip = false);

	//! converts a 8 bit palettized or non palettized image (A8) into A8R8G8B8
	static void convert8BitTo32Bit(const u8 *in, u8 *out, s32 width, s32 height, const u8 *palette, s32 linepad = 0, bool flip = false);

	//! converts R8G8B8 16 bit data to A1R5G5B5 data
	static void convert16BitTo16Bit(const s16 *in, s16 *out, s32 width, s32 height, s32 linepad = 0, bool flip = false);

	//! copies R8G8B8 24 bit data to 24 data, and flips and
	//! mirrors the image during the process.
	static void convert24BitTo24Bit(const u8 *in, u8 *out, s32 width, s32 height, s32 linepad = 0, bool flip = false, bool bgr = false);

	//! Resizes the surface to a new size and converts it at the same time
	//! to an A8R8G8B8 format, returning the pointer to the new buffer.
	static void convert16bitToA8R8G8B8andResize(const s16 *in, s32 *out, s32 newWidth, s32 newHeight, s32 currentWidth, s32 currentHeight);

	//! copies X8R8G8B8 32 bit data, and flips and
	//! mirrors the image during the process.
	static void convert32BitTo32Bit(const s32 *in, s32 *out, s32 width, s32 height, s32 linepad, bool flip = false);

	//! Functions for converting one image format to another efficiently
	//! and hopefully correctly.
	//! Note that the format descriptions refer to the ECOLOR_FORMAT's and not to memory layout.
	//! Reasons for that go back to DX9 naming which tended to flip 32-bit and 16-bit byte orders so ARGB usually means BGRA.
	//!
	//! \param sP pointer to source pixel data
	//! \param sN number of source pixels to copy
	//! \param dP pointer to destination data buffer. must be big enough
	//! to hold sN pixels in the output format.
	static void convert_A1R5G5B5toR8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_A1R5G5B5toB8G8R8(const void *sP, s32 sN, void *dP);
	static void convert_A1R5G5B5toA8R8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_A1R5G5B5toA1R5G5B5(const void *sP, s32 sN, void *dP);
	static void convert_A1R5G5B5toR5G5B5A1(const void *sP, s32 sN, void *dP);
	static void convert_A1R5G5B5toR5G6B5(const void *sP, s32 sN, void *dP);

	static void convert_A8R8G8B8toR8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_A8R8G8B8toB8G8R8(const void *sP, s32 sN, void *dP);
	static void convert_A8R8G8B8toA8R8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_A8R8G8B8toA1R5G5B5(const void *sP, s32 sN, void *dP);
	static void convert_A8R8G8B8toA1B5G5R5(const void *sP, s32 sN, void *dP);
	static void convert_A8R8G8B8toR5G6B5(const void *sP, s32 sN, void *dP);

	static void convert_A8R8G8B8toR3G3B2(const void *sP, s32 sN, void *dP);
	static void convert_R8G8B8toR8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_R8G8B8toA8R8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_R8G8B8toA1R5G5B5(const void *sP, s32 sN, void *dP);
	static void convert_R8G8B8toB8G8R8(const void *sP, s32 sN, void *dP);
	static void convert_R8G8B8toR5G6B5(const void *sP, s32 sN, void *dP);
	static void convert_A8R8G8B8toR8G8B8A8(const void *sP, s32 sN, void *dP);
	static void convert_A8R8G8B8toA8B8G8R8(const void *sP, s32 sN, void *dP);

	static void convert_R5G6B5toR5G6B5(const void *sP, s32 sN, void *dP);
	static void convert_R5G6B5toR8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_R5G6B5toB8G8R8(const void *sP, s32 sN, void *dP);
	static void convert_R5G6B5toA8R8G8B8(const void *sP, s32 sN, void *dP);
	static void convert_R5G6B5toA1R5G5B5(const void *sP, s32 sN, void *dP);
	static void convert_viaFormat(const void *sP, ECOLOR_FORMAT sF, s32 sN,
			void *dP, ECOLOR_FORMAT dF);
	// Check if convert_viaFormat is usable
	static bool canConvertFormat(ECOLOR_FORMAT sourceFormat, ECOLOR_FORMAT destFormat);
};

} // end namespace video
