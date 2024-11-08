// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CImageWriterPNG.h"

#include "CColorConverter.h"
#include "IWriteFile.h"
#include "coreutil.h"
#include "os.h" // for logging

#include <png.h> // use system lib png

namespace irr
{
namespace video
{

IImageWriter *createImageWriterPNG()
{
	return new CImageWriterPNG;
}

// PNG function for error handling
static void png_cpexcept_error(png_structp png_ptr, png_const_charp msg)
{
	io::IWriteFile *file = reinterpret_cast<io::IWriteFile *>(png_get_error_ptr(png_ptr));
	std::string logmsg = std::string("PNG fatal error for ")
			+ file->getFileName().c_str() + ": " + msg;
	os::Printer::log(logmsg.c_str(), ELL_ERROR);
	longjmp(png_jmpbuf(png_ptr), 1);
}

// PNG function for warning handling
static void png_cpexcept_warning(png_structp png_ptr, png_const_charp msg)
{
	io::IWriteFile *file = reinterpret_cast<io::IWriteFile *>(png_get_error_ptr(png_ptr));
	std::string logmsg = std::string("PNG warning for ")
			+ file->getFileName().c_str() + ": " + msg;
	os::Printer::log(logmsg.c_str(), ELL_WARNING);
}

// PNG function for file writing
void PNGAPI user_write_data_fcn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	png_size_t check;

	io::IWriteFile *file = (io::IWriteFile *)png_get_io_ptr(png_ptr);
	check = (png_size_t)file->write((const void *)data, length);

	if (check != length)
		png_error(png_ptr, "Write Error");
}

CImageWriterPNG::CImageWriterPNG()
{
#ifdef _DEBUG
	setDebugName("CImageWriterPNG");
#endif
}

bool CImageWriterPNG::isAWriteableFileExtension(const io::path &filename) const
{
	return core::hasFileExtension(filename, "png");
}

bool CImageWriterPNG::writeImage(io::IWriteFile *file, IImage *image, u32 param) const
{
	if (!file || !image)
		return false;

	// Allocate the png write struct
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
			file, (png_error_ptr)png_cpexcept_error, (png_error_ptr)png_cpexcept_warning);
	if (!png_ptr) {
		os::Printer::log("PNGWriter: Internal PNG create write struct failure", file->getFileName(), ELL_ERROR);
		return false;
	}

	// Allocate the png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		os::Printer::log("PNGWriter: Internal PNG create info struct failure", file->getFileName(), ELL_ERROR);
		png_destroy_write_struct(&png_ptr, NULL);
		return false;
	}

	// for proper error handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return false;
	}

	png_set_write_fn(png_ptr, file, user_write_data_fcn, NULL);

	// Set info
	switch (image->getColorFormat()) {
	case ECF_A8R8G8B8:
	case ECF_A1R5G5B5:
		png_set_IHDR(png_ptr, info_ptr,
				image->getDimension().Width, image->getDimension().Height,
				8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		break;
	default:
		png_set_IHDR(png_ptr, info_ptr,
				image->getDimension().Width, image->getDimension().Height,
				8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	}

	s32 lineWidth = image->getDimension().Width;
	switch (image->getColorFormat()) {
	case ECF_R8G8B8:
	case ECF_R5G6B5:
		lineWidth *= 3;
		break;
	case ECF_A8R8G8B8:
	case ECF_A1R5G5B5:
		lineWidth *= 4;
		break;
	// TODO: Error handling in case of unsupported color format
	default:
		break;
	}
	u8 *tmpImage = new u8[image->getDimension().Height * lineWidth];
	if (!tmpImage) {
		os::Printer::log("PNGWriter: Internal PNG create image failure", file->getFileName(), ELL_ERROR);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return false;
	}

	u8 *data = (u8 *)image->getData();
	switch (image->getColorFormat()) {
	case ECF_R8G8B8:
		CColorConverter::convert_R8G8B8toR8G8B8(data, image->getDimension().Height * image->getDimension().Width, tmpImage);
		break;
	case ECF_A8R8G8B8:
		CColorConverter::convert_A8R8G8B8toA8R8G8B8(data, image->getDimension().Height * image->getDimension().Width, tmpImage);
		break;
	case ECF_R5G6B5:
		CColorConverter::convert_R5G6B5toR8G8B8(data, image->getDimension().Height * image->getDimension().Width, tmpImage);
		break;
	case ECF_A1R5G5B5:
		CColorConverter::convert_A1R5G5B5toA8R8G8B8(data, image->getDimension().Height * image->getDimension().Width, tmpImage);
		break;
		// TODO: Error handling in case of unsupported color format
	default:
		os::Printer::log("CImageWriterPNG does not support image format", ColorFormatNames[image->getColorFormat()], ELL_WARNING);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		delete[] tmpImage;
		return false;
	}

	// Create array of pointers to rows in image data

	// Used to point to image rows
	u8 **RowPointers = new png_bytep[image->getDimension().Height];
	if (!RowPointers) {
		os::Printer::log("PNGWriter: Internal PNG create row pointers failure", file->getFileName(), ELL_ERROR);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		delete[] tmpImage;
		return false;
	}

	data = tmpImage;
	// Fill array of pointers to rows in image data
	for (u32 i = 0; i < image->getDimension().Height; ++i) {
		RowPointers[i] = data;
		data += lineWidth;
	}
	// for proper error handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		delete[] RowPointers;
		delete[] tmpImage;
		return false;
	}

	png_set_rows(png_ptr, info_ptr, RowPointers);

	if (image->getColorFormat() == ECF_A8R8G8B8 || image->getColorFormat() == ECF_A1R5G5B5)
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_BGR, NULL);
	else {
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	}

	delete[] RowPointers;
	delete[] tmpImage;
	png_destroy_write_struct(&png_ptr, &info_ptr);
	return true;
}

} // namespace video
} // namespace irr
