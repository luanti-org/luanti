// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 hecks

#include "png.h"
#include <string>
#include <optional>
#include <sstream>
#include <zlib.h>
#include <cassert>
#include "util/serialize.h"
#include "serialization.h"
#include "irrlichttypes.h"

enum {
	COLOR_GRAY = 0,
	COLOR_RGB = 2,
	COLOR_RGBA = 6,
};

static void writeChunk(std::string &target, const std::string &chunk_str)
{
	assert(chunk_str.size() >= 4);
	assert(chunk_str.size() - 4 < U32_MAX);
	u8 tmp[4];
	target.reserve(target.size() + 4 + chunk_str.size() + 4);

	writeU32(tmp, chunk_str.size() - 4); // Length minus the identifier
	target.append(reinterpret_cast<char*>(tmp), 4);
	target.append(chunk_str); // Data
	const u32 csum = crc32(0, reinterpret_cast<const u8*>(chunk_str.data()),
			chunk_str.size());
	writeU32(tmp, csum); // CRC32 checksum
	target.append(reinterpret_cast<char*>(tmp), 4);
}

static std::optional<u8> reduceColor(const u8 *data, u32 width, u32 height, std::string &new_data)
{
	const u32 npixels = width * height;
	// check if the alpha channel is all opaque
	for (u32 i = 0; i < npixels; i++) {
		if (data[4*i + 3] != 255)
			return std::nullopt;
	}

	// check if RGB components are identical
	bool gray = true;
	for (u32 i = 0; i < npixels; i++) {
		const u8 *pixel = &data[4*i];
		if (pixel[0] != pixel[1] || pixel[1] != pixel[2]) {
			gray = false;
			break;
		}
	}

	if (gray) {
		// convert to grayscale
		new_data.resize(width * height);
		u8 *dst = reinterpret_cast<u8*>(new_data.data());
		for (u32 i = 0; i < npixels; i++)
			dst[i] = data[4*i];
		return COLOR_GRAY;
	} else {
		// convert to RGB
		new_data.resize(width * 3 * height);
		u8 *dst = reinterpret_cast<u8*>(new_data.data());
		for (u32 i = 0; i < npixels; i++)
			memcpy(&dst[3*i], &data[4*i], 3);
		return COLOR_RGB;
	}
}

std::string encodePNG(const u8 *data, u32 width, u32 height, s32 compression)
{
	u8 color_type = COLOR_RGBA;
	std::string new_data;
	if (compression == Z_DEFAULT_COMPRESSION || compression >= 2) {
		// try to reduce the image data to grayscale or RGB
		if (auto ret = reduceColor(data, width, height, new_data); ret.has_value()) {
			color_type = ret.value();
			assert(!new_data.empty());
			data = reinterpret_cast<u8*>(new_data.data());
		}
	}

	std::string file;
	file.append("\x89PNG\r\n\x1a\n");

	{
		std::ostringstream header(std::ios::binary);
		header << "IHDR";
		writeU32(header, width);
		writeU32(header, height);
		writeU8(header, 8); // bpp
		writeU8(header, color_type);
		header.write("\x00\x00\x00", 3);
		writeChunk(file, header.str());
	}

	{
		std::ostringstream IDAT(std::ios::binary);
		IDAT << "IDAT";
		const u32 ps = color_type == COLOR_GRAY ? 1 :
				(color_type == COLOR_RGB ? 3 : 4);
		std::string scanlines;
		scanlines.reserve(width * ps * height + height);
		for(u32 i = 0; i < height; i++) {
			scanlines.append(1, 0); // Null predictor
			scanlines.append(reinterpret_cast<const char*>(data + width * ps * i),
					width * ps);
		}
		compressZlib(scanlines, IDAT, compression);
		writeChunk(file, IDAT.str());
	}

	file.append("\x00\x00\x00\x00IEND\xae\x42\x60\x82", 12);

	return file;
}
