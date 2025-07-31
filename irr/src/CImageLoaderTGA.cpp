// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CImageLoaderTGA.h"

#include "IReadFile.h"
#include "coreutil.h"
#include "os.h"
#include "CColorConverter.h"
#include "CImage.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

namespace video
{

//! returns true if the file maybe is able to be loaded by this class
//! based on the file extension (e.g. ".tga")
bool CImageLoaderTGA::isALoadableFileExtension(const io::path &filename) const
{
	return core::hasFileExtension(filename, "tga");
}

//! loads a compressed tga.
u8 *CImageLoaderTGA::loadCompressedImage(io::IReadFile *file, const STGAHeader &header) const
{
	// This was written and sent in by Jon Pry, thank you very much!
	// I only changed the formatting a little bit.

	const u32 bytesPerPixel = header.PixelDepth / 8;
	const u32 imageSize = header.ImageHeight * header.ImageWidth * bytesPerPixel;
	u8 *data = new u8[imageSize];
	u32 currentByte = 0;

	while (currentByte < imageSize) {
		u8 chunkheader = 0;
		file->read(&chunkheader, sizeof(u8)); // Read The Chunk's Header

		if (chunkheader < 128) { // If The Chunk Is A 'RAW' Chunk
			chunkheader++; // Add 1 To The Value To Get Total Number Of Raw Pixels

			const u32 bytesToRead = bytesPerPixel * chunkheader;
			if (currentByte + bytesToRead <= imageSize) {
				file->read(&data[currentByte], bytesToRead);
				currentByte += bytesToRead;
			} else {
				os::Printer::log("Compressed TGA file RAW chunk tries writing beyond buffer", file->getFileName(), ELL_WARNING);
				break;
			}
		} else {
			// thnx to neojzs for some fixes with this code

			// If It's An RLE Header
			chunkheader -= 127; // Subtract 127 To Get Rid Of The ID Bit

			u32 dataOffset = currentByte;
			if (dataOffset + bytesPerPixel < imageSize) {
				file->read(&data[dataOffset], bytesPerPixel);
				currentByte += bytesPerPixel;
			} else {
				os::Printer::log("Compressed TGA file RLE headertries writing beyond buffer", file->getFileName(), ELL_WARNING);
				break;
			}

			for (u32 counter = 1; counter < chunkheader; counter++) {
				if (currentByte + bytesPerPixel <= imageSize) {
					for (u32 elementCounter = 0; elementCounter < bytesPerPixel; elementCounter++) {
						data[currentByte + elementCounter] = data[dataOffset + elementCounter];
					}
				}

				currentByte += bytesPerPixel;
			}
		}
	}

	return data;
}

//! returns true if the file maybe is able to be loaded by this class
bool CImageLoaderTGA::isALoadableFileFormat(io::IReadFile *file) const
{
	if (!file)
		return false;

	STGAFooter footer;
	memset(&footer, 0, sizeof(STGAFooter));
	file->seek(file->getSize() - sizeof(STGAFooter));
	file->read(&footer, sizeof(STGAFooter));
	return (!strcmp(footer.Signature, "TRUEVISION-XFILE.")); // very old tgas are refused.
}

/// Converts *byte order* BGR to *endianness order* ARGB (SColor "=" u32)
static void convert_BGR8_to_SColor(const u8 *src, u32 n, u32 *dst)
{
	for (u32 i = 0; i < n; ++i) {
		const u8 *bgr = &src[3 * i];
		dst[i] = 0xff000000 | (bgr[2] << 16) | (bgr[1] << 8) | bgr[0];
	}
}

/// Converts *byte order* BGRA to *endianness order* ARGB (SColor "=" u32)
/// Note: This just copies from src to dst on little endian.
static void convert_BGRA8_to_SColor(const u8 *src, u32 n, u32 *dst)
{
	for (u32 i = 0; i < n; ++i) {
		const u8 *bgra = &src[4 * i];
		dst[i] = (bgra[3] << 24) | (bgra[2] << 16) | (bgra[1] << 8) | bgra[0];
	}
}

//! creates a surface from the file
IImage *CImageLoaderTGA::loadImage(io::IReadFile *file) const
{
	STGAHeader header;
	u32 *palette = 0;

	file->read(&header, sizeof(STGAHeader));

#ifdef __BIG_ENDIAN__
	header.ColorMapLength = os::Byteswap::byteswap(header.ColorMapLength);
	header.ImageWidth = os::Byteswap::byteswap(header.ImageWidth);
	header.ImageHeight = os::Byteswap::byteswap(header.ImageHeight);
#endif

	if (!checkImageDimensions(header.ImageWidth, header.ImageHeight)) {
		os::Printer::log("Image dimensions too large in file", file->getFileName(), ELL_ERROR);
		return 0;
	}

	// skip image identification field
	if (header.IdLength)
		file->seek(header.IdLength, true);

	if (header.ColorMapType) {
		// Create 32 bit palette
		// `core::max_()` is not used here because it takes its inputs as references. Since `header` is packed, use the macro `MAX()` instead:
		const u16 paletteSize = MAX((u16)256u, header.ColorMapLength); // ColorMapLength can lie, but so far we only use palette for 8-bit, so ensure it has 256 entries
		palette = new u32[paletteSize];

		if (paletteSize > header.ColorMapLength) {
			// To catch images using palette colors with invalid indices
			const u32 errorCol = video::SColor(255, 255, 0, 205).color; // bright magenta
			for (u16 i = header.ColorMapLength; i < paletteSize; ++i)
				palette[i] = errorCol;
		}

		// read color map
		u8 *colorMap = new u8[header.ColorMapEntrySize / 8 * header.ColorMapLength];
		file->read(colorMap, header.ColorMapEntrySize / 8 * header.ColorMapLength);

		// convert to 32-bit palette
		switch (header.ColorMapEntrySize) {
		case 16:
			CColorConverter::convert_A1R5G5B5toA8R8G8B8(colorMap, header.ColorMapLength, palette);
			break;
		case 24:
			convert_BGR8_to_SColor(colorMap, header.ColorMapLength, palette);
			break;
		case 32:
			convert_BGRA8_to_SColor(colorMap, header.ColorMapLength, palette);
			break;
		}
		delete[] colorMap;
	}

	// read image

	u8 *data = 0;

	if (header.ImageType == 1 ||     // Uncompressed, color-mapped images.
			header.ImageType == 2 || // Uncompressed, RGB images
			header.ImageType == 3    // Uncompressed, black and white images
	) {
		const s32 imageSize = header.ImageHeight * header.ImageWidth * (header.PixelDepth / 8);
		data = new u8[imageSize];
		file->read(data, imageSize);
	} else if (header.ImageType == 10) {
		// Runlength encoded RGB images
		data = loadCompressedImage(file, header);
	} else {
		os::Printer::log("Unsupported TGA file type", file->getFileName(), ELL_ERROR);
		delete[] palette;
		return 0;
	}

	IImage *image = 0;

	switch (header.PixelDepth) {
	case 8: {
		if (header.ImageType == 3) { // grey image
			image = new CImage(ECF_R8G8B8,
					core::dimension2d<u32>(header.ImageWidth, header.ImageHeight));
			if (image)
				CColorConverter::convert8BitTo24Bit((u8 *)data,
						(u8 *)image->getData(),
						header.ImageWidth, header.ImageHeight,
						0, 0, (header.ImageDescriptor & 0x20) == 0);
		} else {
			// Colormap is converted to A8R8G8B8 at this point – thus the code can handle all color formats.
			// This wastes some texture memory, but is less of a third of the code that does this optimally.
			// If you want to refactor this: The possible color formats here are A1R5G5B5, B8G8R8, B8G8R8A8.
			image = new CImage(ECF_A8R8G8B8, core::dimension2d<u32>(header.ImageWidth, header.ImageHeight));
			if (image)
				CColorConverter::convert8BitTo32Bit((u8 *)data,
						(u8 *)image->getData(),
						header.ImageWidth, header.ImageHeight,
						(u8 *)palette, 0,
						(header.ImageDescriptor & 0x20) == 0);
		}
	} break;
	case 16:
		image = new CImage(ECF_A1R5G5B5,
				core::dimension2d<u32>(header.ImageWidth, header.ImageHeight));
		if (image)
			CColorConverter::convert16BitTo16Bit((s16 *)data,
					(s16 *)image->getData(), header.ImageWidth, header.ImageHeight, 0, (header.ImageDescriptor & 0x20) == 0);
		break;
	case 24:
		image = new CImage(ECF_R8G8B8,
				core::dimension2d<u32>(header.ImageWidth, header.ImageHeight));
		if (image)
			CColorConverter::convert24BitTo24Bit(
					(u8 *)data, (u8 *)image->getData(), header.ImageWidth, header.ImageHeight, 0, (header.ImageDescriptor & 0x20) == 0, true);
		break;
	case 32:
		image = new CImage(ECF_A8R8G8B8,
				core::dimension2d<u32>(header.ImageWidth, header.ImageHeight));
		if (image)
			CColorConverter::convert32BitTo32Bit((s32 *)data,
					(s32 *)image->getData(), header.ImageWidth, header.ImageHeight, 0, (header.ImageDescriptor & 0x20) == 0);
		break;
	default:
		os::Printer::log("Unsupported TGA format", file->getFileName(), ELL_ERROR);
		break;
	}

	delete[] data;
	delete[] palette;

	return image;
}

//! creates a loader which is able to load tgas
IImageLoader *createImageLoaderTGA()
{
	return new CImageLoaderTGA();
}

} // end namespace video
