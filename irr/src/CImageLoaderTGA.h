// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IImageLoader.h"

namespace video
{

// byte-align structures
#include "irrpack.h"

// these structs are also used in the TGA writer
struct STGAHeader
{
	u8 IdLength;
	u8 ColorMapType;
	u8 ImageType;
	u8 FirstEntryIndex[2];
	u16 ColorMapLength;
	u8 ColorMapEntrySize;
	u8 XOrigin[2];
	u8 YOrigin[2];
	u16 ImageWidth;
	u16 ImageHeight;
	u8 PixelDepth;
	u8 ImageDescriptor;
} PACK_STRUCT;

struct STGAFooter
{
	u32 ExtensionOffset;
	u32 DeveloperOffset;
	c8 Signature[18];
} PACK_STRUCT;

// Default alignment
#include "irrunpack.h"

/*!
	Surface Loader for targa images
*/
class CImageLoaderTGA : public IImageLoader
{
public:
	//! returns true if the file maybe is able to be loaded by this class
	//! based on the file extension (e.g. ".tga")
	bool isALoadableFileExtension(const io::path &filename) const override;

	//! returns true if the file maybe is able to be loaded by this class
	bool isALoadableFileFormat(io::IReadFile *file) const override;

	//! creates a surface from the file
	IImage *loadImage(io::IReadFile *file) const override;

private:
	//! loads a compressed tga. Was written and sent in by Jon Pry, thank you very much!
	u8 *loadCompressedImage(io::IReadFile *file, const STGAHeader &header) const;
};

} // end namespace video
