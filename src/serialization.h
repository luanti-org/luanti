// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "exceptions.h"
#include <iostream>
#include <string_view>

/*
	Map format serialization version
	--------------------------------

	For map data (blocks, nodes, sectors).

	NOTE: The goal is to increment this so that saved maps will be
	      loadable by any version. Other compatibility is not
		  maintained.

	0: original networked test with 1-byte nodes
	1: update with 2-byte nodes
	2: lighting is transmitted in param
	3: optional fetching of far blocks
	4: block compression
	5: sector objects NOTE: block compression was left accidentally out
	6: failed attempt at switching block compression on again
	7: block compression switched on again
	8: server-initiated block transfers and all kinds of stuff
	9: block objects
	10: water pressure
	11: zlib'd blocks, block flags
	12: UnlimitedHeightmap now uses interpolated areas
	13: Mapgen v2
	14: NodeMetadata
	15: StaticObjects
	16: larger maximum size of node metadata, and compression
	17: MapBlocks contain timestamp
	18: new generator (not really necessary, but it's there)
	19: new content type handling
	20: many existing content types translated to extended ones
	21: dynamic content type allocation
	22: minerals removed, facedir & wallmounted changed
	23: new node metadata format
	24: 16-bit node ids and node timers (never released as stable)
	25: Improved node timer format
	26: Never written; read the same as 25
	27: Added light spreading flags to blocks
	28: Added "private" flag to NodeMetadata
	29: Switched compression to zstd, a bit of reorganization
*/

// This represents an uninitialized or invalid format
constexpr u8 SER_FMT_VER_INVALID = 255;
// Highest supported serialization version
constexpr u8 SER_FMT_VER_HIGHEST_READ = 29;
// Saved on disk version
constexpr u8 SER_FMT_VER_HIGHEST_WRITE = 29;
// Lowest supported serialization version
constexpr u8 SER_FMT_VER_LOWEST_READ = 0;
// Lowest serialization version for writing
// Can't do < 24 anymore; we have 16-bit dynamically allocated node IDs
// in memory; conversion just won't work in this direction.
constexpr u8 SER_FMT_VER_LOWEST_WRITE = 24;

inline bool ser_ver_supported_read(s32 v)
{
	return v >= SER_FMT_VER_LOWEST_READ && v <= SER_FMT_VER_HIGHEST_READ;
}

inline bool ser_ver_supported_write(s32 v)
{
	return v >= SER_FMT_VER_LOWEST_WRITE && v <= SER_FMT_VER_HIGHEST_WRITE;
}

/*
	Compression functions
*/

void compressZlib(const u8 *data, size_t data_size, std::ostream &os, int level = -1);
inline void compressZlib(std::string_view data, std::ostream &os, int level = -1)
{
	compressZlib(reinterpret_cast<const u8*>(data.data()), data.size(), os, level);
}
void decompressZlib(std::istream &is, std::ostream &os, size_t limit = 0);

void compressZstd(const u8 *data, size_t data_size, std::ostream &os, int level = 0);
inline void compressZstd(std::string_view data, std::ostream &os, int level = 0)
{
	compressZstd(reinterpret_cast<const u8*>(data.data()), data.size(), os, level);
}
void decompressZstd(std::istream &is, std::ostream &os);

// These choose between zstd, zlib and a self-made one according to version
void compress(const u8 *data, u32 size, std::ostream &os, u8 version, int level = -1);
inline void compress(std::string_view data, std::ostream &os, u8 version, int level = -1)
{
	compress(reinterpret_cast<const u8*>(data.data()), data.size(), os, version, level);
}
void decompress(std::istream &is, std::ostream &os, u8 version);
