// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CImageLoaderBMP.h"

#include "IReadFile.h"
#include "SColor.h"
#include "CColorConverter.h"
#include "CImage.h"
#include "coreutil.h"
#include "os.h"

namespace irr
{
namespace video
{

//! constructor
CImageLoaderBMP::CImageLoaderBMP()
{
#ifdef _DEBUG
	setDebugName("CImageLoaderBMP");
#endif
}

//! returns true if the file maybe is able to be loaded by this class
//! based on the file extension (e.g. ".tga")
bool CImageLoaderBMP::isALoadableFileExtension(const io::path &filename) const
{
	return core::hasFileExtension(filename, "bmp");
}

//! returns true if the file maybe is able to be loaded by this class
bool CImageLoaderBMP::isALoadableFileFormat(io::IReadFile *file) const
{
	u16 headerID;
	file->read(&headerID, sizeof(u16));
#ifdef __BIG_ENDIAN__
	headerID = os::Byteswap::byteswap(headerID);
#endif
	return headerID == 0x4d42;
}

// UB-safe overflow check
static inline bool overflowCheck(const void *base, size_t offset, const void *end)
{
	auto baseI = reinterpret_cast<uintptr_t>(base),
		 endI = reinterpret_cast<uintptr_t>(end);
	return baseI > endI || offset >= (endI - baseI);
}
// check whether &p[0] to &p[_off - 1] can be accessed
#define CHECKP(_off)                                \
	if ((_off) < 0 || overflowCheck(p, _off, pEnd)) \
	goto exit
// same for d
#define CHECKD(_off)                                   \
	if ((_off) < 0 || overflowCheck(d, _off, destEnd)) \
	goto exit

void CImageLoaderBMP::decompress8BitRLE(u8 *&bmpData, s32 size, s32 width, s32 height, s32 pitch) const
{
	u8 *p = bmpData;
	const u8 *pEnd = bmpData + size;
	u8 *newBmp = new u8[(width + pitch) * height];
	u8 *d = newBmp;
	const u8 *destEnd = newBmp + (width + pitch) * height;
	s32 line = 0;

	while (p < pEnd && d < destEnd) {
		if (*p == 0) {
			++p;
			CHECKP(1);

			switch (*p) {
			case 0: // end of line
				++p;
				++line;
				d = newBmp + (line * (width + pitch));
				break;
			case 1: // end of bmp
				goto exit;
			case 2:
				++p;
				CHECKP(2);
				d += (u8)*p;
				++p; // delta
				d += ((u8)*p) * (width + pitch);
				++p;
				break;
			default: {
				// absolute mode
				s32 count = (u8)*p;
				++p;
				s32 readAdditional = ((2 - (count % 2)) % 2);

				CHECKP(count);
				CHECKD(count);
				for (s32 i = 0; i < count; ++i) {
					*d = *p;
					++p;
					++d;
				}

				CHECKP(readAdditional);
				for (s32 i = 0; i < readAdditional; ++i)
					++p;
			}
			}
		} else {
			s32 count = (u8)*p;
			++p;
			CHECKP(1);
			u8 color = *p;
			++p;
			CHECKD(count);
			for (s32 i = 0; i < count; ++i) {
				*d = color;
				++d;
			}
		}
	}

exit:
	delete[] bmpData;
	bmpData = newBmp;
}

// how many bytes will be touched given the current state of decompress4BitRLE
static inline u32 shiftedCount(s32 count, s32 shift)
{
	_IRR_DEBUG_BREAK_IF(count < 0)
	u32 ret = count / 2;
	if (shift == 0 || count % 2 == 1)
		++ret;
	return ret;
}

void CImageLoaderBMP::decompress4BitRLE(u8 *&bmpData, s32 size, s32 width, s32 height, s32 pitch) const
{
	const s32 lineWidth = (width + 1) / 2 + pitch;
	u8 *p = bmpData;
	const u8 *pEnd = bmpData + size;
	u8 *newBmp = new u8[lineWidth * height];
	u8 *d = newBmp;
	const u8 *destEnd = newBmp + lineWidth * height;
	s32 line = 0;
	s32 shift = 4;

	while (p < pEnd && d < destEnd) {
		if (*p == 0) {
			++p;
			CHECKP(1);

			switch (*p) {
			case 0: // end of line
				++p;
				++line;
				d = newBmp + (line * lineWidth);
				shift = 4;
				break;
			case 1: // end of bmp
				goto exit;
			case 2: {
				++p;
				CHECKP(2);
				s32 x = (u8)*p;
				++p;
				s32 y = (u8)*p;
				++p;
				d += x / 2 + y * lineWidth;
				shift = x % 2 == 0 ? 4 : 0;
			} break;
			default: {
				// absolute mode
				s32 count = (u8)*p;
				++p;
				s32 readAdditional = ((2 - ((count) % 2)) % 2);
				s32 readShift = 4;

				CHECKP(shiftedCount(count, readShift));
				CHECKD(shiftedCount(count, shift));
				for (s32 i = 0; i < count; ++i) {
					s32 color = (((u8)*p) >> readShift) & 0x0f;
					readShift -= 4;
					if (readShift < 0) {
						++*p; // <- bug?
						readShift = 4;
					}

					u8 mask = 0x0f << shift;
					*d = (*d & (~mask)) | ((color << shift) & mask);

					shift -= 4;
					if (shift < 0) {
						shift = 4;
						++d;
					}
				}

				CHECKP(readAdditional);
				for (s32 i = 0; i < readAdditional; ++i)
					++p;
			}
			}
		} else {
			s32 count = (u8)*p;
			++p;
			CHECKP(1);
			s32 color1 = (u8)*p;
			color1 = color1 & 0x0f;
			s32 color2 = (u8)*p;
			color2 = (color2 >> 4) & 0x0f;
			++p;

			CHECKD(shiftedCount(count, shift));
			for (s32 i = 0; i < count; ++i) {
				u8 mask = 0x0f << shift;
				u8 toSet = (shift == 0 ? color1 : color2) << shift;
				*d = (*d & (~mask)) | (toSet & mask);

				shift -= 4;
				if (shift < 0) {
					shift = 4;
					++d;
				}
			}
		}
	}

exit:
	delete[] bmpData;
	bmpData = newBmp;
}

#undef CHECKOFF
#undef CHECKP
#undef CHECKD

//! creates a surface from the file
IImage *CImageLoaderBMP::loadImage(io::IReadFile *file) const
{
	SBMPHeader header;

	file->read(&header, sizeof(header));

#ifdef __BIG_ENDIAN__
	header.Id = os::Byteswap::byteswap(header.Id);
	header.FileSize = os::Byteswap::byteswap(header.FileSize);
	header.BitmapDataOffset = os::Byteswap::byteswap(header.BitmapDataOffset);
	header.BitmapHeaderSize = os::Byteswap::byteswap(header.BitmapHeaderSize);
	header.Width = os::Byteswap::byteswap(header.Width);
	header.Height = os::Byteswap::byteswap(header.Height);
	header.Planes = os::Byteswap::byteswap(header.Planes);
	header.BPP = os::Byteswap::byteswap(header.BPP);
	header.Compression = os::Byteswap::byteswap(header.Compression);
	header.BitmapDataSize = os::Byteswap::byteswap(header.BitmapDataSize);
	header.PixelPerMeterX = os::Byteswap::byteswap(header.PixelPerMeterX);
	header.PixelPerMeterY = os::Byteswap::byteswap(header.PixelPerMeterY);
	header.Colors = os::Byteswap::byteswap(header.Colors);
	header.ImportantColors = os::Byteswap::byteswap(header.ImportantColors);
#endif

	s32 pitch = 0;

	//! return if the header is false

	if (header.Id != 0x4d42)
		return 0;

	if (header.Compression > 2) { // we'll only handle RLE-Compression
		os::Printer::log("Compression mode not supported.", ELL_ERROR);
		return 0;
	}

	if (header.BPP > 32 || !checkImageDimensions(header.Width, header.Height)) {
		os::Printer::log("Rejecting BMP with unreasonable size or BPP.", ELL_ERROR);
		return 0;
	}

	// adjust bitmap data size to dword boundary
	header.BitmapDataSize += (4 - (header.BitmapDataSize % 4)) % 4;

	// read palette

	long pos = file->getPos();
	constexpr s32 paletteAllocSize = 256;
	s32 paletteSize = (header.BitmapDataOffset - pos) / 4;
	paletteSize = core::clamp(paletteSize, 0, paletteAllocSize);

	s32 *paletteData = 0;
	if (paletteSize) {
		// always allocate an 8-bit palette to ensure enough space
		paletteData = new s32[paletteAllocSize];
		memset(paletteData, 0, paletteAllocSize * sizeof(s32));
		file->read(paletteData, paletteSize * sizeof(s32));
#ifdef __BIG_ENDIAN__
		for (s32 i = 0; i < paletteSize; ++i)
			paletteData[i] = os::Byteswap::byteswap(paletteData[i]);
#endif
	}

	// read image data

	if (!header.BitmapDataSize) {
		// okay, lets guess the size
		// some tools simply don't set it
		header.BitmapDataSize = static_cast<u32>(file->getSize()) - header.BitmapDataOffset;
	}

	file->seek(header.BitmapDataOffset);

	s32 widthInBytes;
	{
		f32 t = (header.Width) * (header.BPP / 8.0f);
		widthInBytes = (s32)t;
		t -= widthInBytes;
		if (t != 0.0f)
			++widthInBytes;
	}

	const s32 lineSize = widthInBytes + ((4 - (widthInBytes % 4))) % 4;
	pitch = lineSize - widthInBytes;

	u8 *bmpData = new u8[header.BitmapDataSize];
	file->read(bmpData, header.BitmapDataSize);

	// decompress data if needed
	switch (header.Compression) {
	case 1: // 8 bit rle
		decompress8BitRLE(bmpData, header.BitmapDataSize, header.Width, header.Height, pitch);
		header.BitmapDataSize = (header.Width + pitch) * header.Height;
		break;
	case 2: // 4 bit rle
		decompress4BitRLE(bmpData, header.BitmapDataSize, header.Width, header.Height, pitch);
		header.BitmapDataSize = ((header.Width + 1) / 2 + pitch) * header.Height;
		break;
	}

	if (header.BitmapDataSize < lineSize * header.Height) {
		os::Printer::log("Bitmap data is cut off.", ELL_ERROR);

		delete[] paletteData;
		delete[] bmpData;
		return 0;
	}

	// create surface
	core::dimension2d<u32> dim;
	dim.Width = header.Width;
	dim.Height = header.Height;

	IImage *image = 0;
	switch (header.BPP) {
	case 1:
		image = new CImage(ECF_A1R5G5B5, dim);
		if (image)
			CColorConverter::convert1BitTo16Bit(bmpData, (s16 *)image->getData(), header.Width, header.Height, pitch, true);
		break;
	case 4:
		image = new CImage(ECF_A1R5G5B5, dim);
		if (image)
			CColorConverter::convert4BitTo16Bit(bmpData, (s16 *)image->getData(), header.Width, header.Height, paletteData, pitch, true);
		break;
	case 8:
		image = new CImage(ECF_A1R5G5B5, dim);
		if (image)
			CColorConverter::convert8BitTo16Bit(bmpData, (s16 *)image->getData(), header.Width, header.Height, paletteData, pitch, true);
		break;
	case 16:
		image = new CImage(ECF_A1R5G5B5, dim);
		if (image)
			CColorConverter::convert16BitTo16Bit((s16 *)bmpData, (s16 *)image->getData(), header.Width, header.Height, pitch, true);
		break;
	case 24:
		image = new CImage(ECF_R8G8B8, dim);
		if (image)
			CColorConverter::convert24BitTo24Bit(bmpData, (u8 *)image->getData(), header.Width, header.Height, pitch, true, true);
		break;
	case 32: // thx to Reinhard Ostermeier
		image = new CImage(ECF_A8R8G8B8, dim);
		if (image)
			CColorConverter::convert32BitTo32Bit((s32 *)bmpData, (s32 *)image->getData(), header.Width, header.Height, pitch, true);
		break;
	};

	// clean up

	delete[] paletteData;
	delete[] bmpData;

	return image;
}

//! creates a loader which is able to load windows bitmaps
IImageLoader *createImageLoaderBMP()
{
	return new CImageLoaderBMP;
}

} // end namespace video
} // end namespace irr
