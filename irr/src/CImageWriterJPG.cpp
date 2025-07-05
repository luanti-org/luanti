// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CImageWriterJPG.h"

#include "CColorConverter.h"
#include "IWriteFile.h"
#include "coreutil.h"
#include "os.h"

#include <cstdio> // IWYU pragma: keep (required for jpeglib.h)
#include <memory>
#include <jpeglib.h>
#include <jerror.h>

namespace video
{

// The writer uses a 4k buffer and flushes to disk each time it's filled
#define OUTPUT_BUF_SIZE 4096
typedef struct
{
	struct jpeg_destination_mgr pub; /* public fields */

	io::IWriteFile *file;           /* target file */
	JOCTET buffer[OUTPUT_BUF_SIZE]; /* image buffer */
} mem_destination_mgr;

typedef mem_destination_mgr *mem_dest_ptr;

// init
static void jpeg_init_destination(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest;
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

// flush to disk and reset buffer
static boolean jpeg_empty_output_buffer(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest;

	// for now just exit upon file error
	if (dest->file->write(dest->buffer, OUTPUT_BUF_SIZE) != OUTPUT_BUF_SIZE)
		ERREXIT(cinfo, JERR_FILE_WRITE);

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return TRUE;
}

static void jpeg_term_destination(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest;
	const size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;
	// for now just exit upon file error
	if (dest->file->write(dest->buffer, datacount) != datacount)
		ERREXIT(cinfo, JERR_FILE_WRITE);
}

// set up buffer data
static void jpeg_file_dest(j_compress_ptr cinfo, io::IWriteFile *file)
{
	if (cinfo->dest == NULL) { /* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo,
				JPOOL_PERMANENT,
				sizeof(mem_destination_mgr));
	}

	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest; /* for casting */

	/* Initialize method pointers */
	dest->pub.init_destination = jpeg_init_destination;
	dest->pub.empty_output_buffer = jpeg_empty_output_buffer;
	dest->pub.term_destination = jpeg_term_destination;

	/* Initialize private member */
	dest->file = file;
}

/* write_JPEG_memory: store JPEG compressed image into memory.
 */
static bool writeJPEGFile(io::IWriteFile *file, IImage *image, u32 quality)
{
	void (*format)(const void *, s32, void *) = 0;
	switch (image->getColorFormat()) {
	case ECF_R8G8B8:
		format = CColorConverter::convert_R8G8B8toR8G8B8;
		break;
	case ECF_A8R8G8B8:
		format = CColorConverter::convert_A8R8G8B8toR8G8B8;
		break;
	case ECF_A1R5G5B5:
		format = CColorConverter::convert_A1R5G5B5toB8G8R8;
		break;
	case ECF_R5G6B5:
		format = CColorConverter::convert_R5G6B5toR8G8B8;
		break;
	default:
		os::Printer::log("writeJPEGFile does not support image format", ColorFormatNames[image->getColorFormat()], ELL_WARNING);
		break;
	}

	// couldn't find a color converter
	if (0 == format)
		return false;

	const core::dimension2du dim = image->getDimension();

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);
	jpeg_file_dest(&cinfo, file);
	cinfo.image_width = dim.Width;
	cinfo.image_height = dim.Height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);

	if (0 == quality)
		quality = 75;

	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);

	std::unique_ptr<u8[]> dest{new u8[dim.Width * 3]};

	const u32 pitch = image->getPitch();
	JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
	row_pointer[0] = dest.get();

	u8 *src = (u8 *)image->getData();

	while (cinfo.next_scanline < cinfo.image_height) {
		// convert next line
		format(src, dim.Width, dest.get());
		src += pitch;
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	/* Step 6: Finish compression */
	jpeg_finish_compress(&cinfo);

	/* Step 7: Destroy */
	jpeg_destroy_compress(&cinfo);

	return true;
}

} // namespace video

namespace video
{

IImageWriter *createImageWriterJPG()
{
	return new CImageWriterJPG;
}

CImageWriterJPG::CImageWriterJPG()
{}

bool CImageWriterJPG::isAWriteableFileExtension(const io::path &filename) const
{
	return core::hasFileExtension(filename, "jpg", "jpeg");
}

bool CImageWriterJPG::writeImage(io::IWriteFile *file, IImage *image, u32 quality) const
{
	return writeJPEGFile(file, image, quality);
}

} // namespace video
