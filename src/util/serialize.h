// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "exceptions.h" // for SerializationError
#include "ieee_float.h"

#include "config.h"
#include <cstring> // for memcpy
#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

/* make sure BYTE_ORDER macros are available */
#ifdef _WIN32
	#define BYTE_ORDER 1234
#elif defined(__MACH__) && defined(__APPLE__)
	#include <machine/endian.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
	#include <sys/endian.h>
#elif HAVE_ENDIAN_H
	#include <endian.h>
#else
	#error "Can't detect endian (missing header)"
#endif
#ifndef LITTLE_ENDIAN
	#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
	#define BIG_ENDIAN 4321
#endif
#if !defined(BYTE_ORDER) && defined(_BYTE_ORDER)
	#define BYTE_ORDER _BYTE_ORDER
#elif !defined(BYTE_ORDER) && defined(__BYTE_ORDER)
	#define BYTE_ORDER __BYTE_ORDER
#endif

#define FIXEDPOINT_FACTOR 1000.0f

// 0x7FFFFFFF / 1000.0f is not serializable.
// The limited float precision at this magnitude may cause the result to round
// to a greater value than can be represented by a 32 bit integer when increased
// by a factor of FIXEDPOINT_FACTOR.  As a result, [F1000_MIN..F1000_MAX] does
// not represent the full range, but rather the largest safe range, of values on
// all supported architectures.  Note: This definition makes assumptions on
// platform float-to-int conversion behavior.
static constexpr float F1000_MIN = (s32)((float)(S32_MIN) / FIXEDPOINT_FACTOR);
static constexpr float F1000_MAX = (s32)((float)(S32_MAX) / FIXEDPOINT_FACTOR);

#define STRING_MAX_LEN 0xFFFF
#define WIDE_STRING_MAX_LEN 0xFFFF
// 64 MB ought to be enough for anybody - Billy G.
#define LONG_STRING_MAX_LEN (64 * 1024 * 1024)


extern FloatType g_serialize_f32_type;

#if HAVE_ENDIAN_H
// use machine native byte swapping routines
// Note: memcpy below is optimized out by modern compilers

inline u16 readU16(const u8 *data)
{
	u16 val;
	memcpy(&val, data, 2);
	return be16toh(val);
}

inline u32 readU32(const u8 *data)
{
	u32 val;
	memcpy(&val, data, 4);
	return be32toh(val);
}

inline u64 readU64(const u8 *data)
{
	u64 val;
	memcpy(&val, data, 8);
	return be64toh(val);
}

inline void writeU16(u8 *data, u16 i)
{
	u16 val = htobe16(i);
	memcpy(data, &val, 2);
}

inline void writeU32(u8 *data, u32 i)
{
	u32 val = htobe32(i);
	memcpy(data, &val, 4);
}

inline void writeU64(u8 *data, u64 i)
{
	u64 val = htobe64(i);
	memcpy(data, &val, 8);
}

#else
// generic byte-swapping implementation

inline u16 readU16(const u8 *data)
{
	return
		((u16)data[0] << 8) | ((u16)data[1] << 0);
}

inline u32 readU32(const u8 *data)
{
	return
		((u32)data[0] << 24) | ((u32)data[1] << 16) |
		((u32)data[2] <<  8) | ((u32)data[3] <<  0);
}

inline u64 readU64(const u8 *data)
{
	return
		((u64)data[0] << 56) | ((u64)data[1] << 48) |
		((u64)data[2] << 40) | ((u64)data[3] << 32) |
		((u64)data[4] << 24) | ((u64)data[5] << 16) |
		((u64)data[6] <<  8) | ((u64)data[7] << 0);
}

inline void writeU16(u8 *data, u16 i)
{
	data[0] = (i >> 8) & 0xFF;
	data[1] = (i >> 0) & 0xFF;
}

inline void writeU32(u8 *data, u32 i)
{
	data[0] = (i >> 24) & 0xFF;
	data[1] = (i >> 16) & 0xFF;
	data[2] = (i >>  8) & 0xFF;
	data[3] = (i >>  0) & 0xFF;
}

inline void writeU64(u8 *data, u64 i)
{
	data[0] = (i >> 56) & 0xFF;
	data[1] = (i >> 48) & 0xFF;
	data[2] = (i >> 40) & 0xFF;
	data[3] = (i >> 32) & 0xFF;
	data[4] = (i >> 24) & 0xFF;
	data[5] = (i >> 16) & 0xFF;
	data[6] = (i >>  8) & 0xFF;
	data[7] = (i >>  0) & 0xFF;
}

#endif // HAVE_ENDIAN_H

//////////////// read routines ////////////////

inline u8 readU8(const u8 *data)
{
	return data[0];
}

inline s8 readS8(const u8 *data)
{
	return (s8)readU8(data);
}

inline s16 readS16(const u8 *data)
{
	return (s16)readU16(data);
}

inline s32 readS32(const u8 *data)
{
	return (s32)readU32(data);
}

inline s64 readS64(const u8 *data)
{
	return (s64)readU64(data);
}

inline f32 readF1000(const u8 *data)
{
	return (f32)readS32(data) / FIXEDPOINT_FACTOR;
}

inline f32 readF32(const u8 *data)
{
	u32 u = readU32(data);

	switch (g_serialize_f32_type) {
	case FLOATTYPE_SYSTEM: {
			f32 f;
			memcpy(&f, &u, 4);
			return f;
		}
	case FLOATTYPE_SLOW:
		return u32Tof32Slow(u);
	case FLOATTYPE_UNKNOWN: // First initialization
		g_serialize_f32_type = getFloatSerializationType();
		return readF32(data);
	}
	throw SerializationError("readF32: Unreachable code");
}

inline video::SColor readARGB8(const u8 *data)
{
	video::SColor p(readU32(data));
	return p;
}

inline v2s16 readV2S16(const u8 *data)
{
	v2s16 p;
	p.X = readS16(&data[0]);
	p.Y = readS16(&data[2]);
	return p;
}

inline v3s16 readV3S16(const u8 *data)
{
	v3s16 p;
	p.X = readS16(&data[0]);
	p.Y = readS16(&data[2]);
	p.Z = readS16(&data[4]);
	return p;
}

inline v2s32 readV2S32(const u8 *data)
{
	v2s32 p;
	p.X = readS32(&data[0]);
	p.Y = readS32(&data[4]);
	return p;
}

inline v3s32 readV3S32(const u8 *data)
{
	v3s32 p;
	p.X = readS32(&data[0]);
	p.Y = readS32(&data[4]);
	p.Z = readS32(&data[8]);
	return p;
}

inline v3f readV3F1000(const u8 *data)
{
	v3f p;
	p.X = readF1000(&data[0]);
	p.Y = readF1000(&data[4]);
	p.Z = readF1000(&data[8]);
	return p;
}

inline v2f readV2F32(const u8 *data)
{
	v2f p;
	p.X = readF32(&data[0]);
	p.Y = readF32(&data[4]);
	return p;
}

inline v3f readV3F32(const u8 *data)
{
	v3f p;
	p.X = readF32(&data[0]);
	p.Y = readF32(&data[4]);
	p.Z = readF32(&data[8]);
	return p;
}

/////////////// write routines ////////////////

inline void writeU8(u8 *data, u8 i)
{
	data[0] = i;
}

inline void writeS8(u8 *data, s8 i)
{
	writeU8(data, (u8)i);
}

inline void writeS16(u8 *data, s16 i)
{
	writeU16(data, (u16)i);
}

inline void writeS32(u8 *data, s32 i)
{
	writeU32(data, (u32)i);
}

inline void writeS64(u8 *data, s64 i)
{
	writeU64(data, (u64)i);
}

inline void writeF1000(u8 *data, f32 i)
{
	assert(i >= F1000_MIN && i <= F1000_MAX);
	writeS32(data, i * FIXEDPOINT_FACTOR);
}

inline void writeF32(u8 *data, f32 i)
{
	switch (g_serialize_f32_type) {
	case FLOATTYPE_SYSTEM: {
			u32 u;
			memcpy(&u, &i, 4);
			return writeU32(data, u);
		}
	case FLOATTYPE_SLOW:
		return writeU32(data, f32Tou32Slow(i));
	case FLOATTYPE_UNKNOWN: // First initialization
		g_serialize_f32_type = getFloatSerializationType();
		return writeF32(data, i);
	}
	throw SerializationError("writeF32: Unreachable code");
}

inline void writeARGB8(u8 *data, video::SColor p)
{
	writeU32(data, p.color);
}

inline void writeV2S16(u8 *data, v2s16 p)
{
	writeS16(&data[0], p.X);
	writeS16(&data[2], p.Y);
}

inline void writeV3S16(u8 *data, v3s16 p)
{
	writeS16(&data[0], p.X);
	writeS16(&data[2], p.Y);
	writeS16(&data[4], p.Z);
}

inline void writeV2S32(u8 *data, v2s32 p)
{
	writeS32(&data[0], p.X);
	writeS32(&data[4], p.Y);
}

inline void writeV3S32(u8 *data, v3s32 p)
{
	writeS32(&data[0], p.X);
	writeS32(&data[4], p.Y);
	writeS32(&data[8], p.Z);
}

inline void writeV3F1000(u8 *data, v3f p)
{
	writeF1000(&data[0], p.X);
	writeF1000(&data[4], p.Y);
	writeF1000(&data[8], p.Z);
}

inline void writeV2F32(u8 *data, v2f p)
{
	writeF32(&data[0], p.X);
	writeF32(&data[4], p.Y);
}

inline void writeV3F32(u8 *data, v3f p)
{
	writeF32(&data[0], p.X);
	writeF32(&data[4], p.Y);
	writeF32(&data[8], p.Z);
}

////
//// Iostream wrapper for data read/write
////

#define MAKE_STREAM_READ_FXN(T, N, S)    \
	inline T read ## N(std::istream &is) \
	{                                    \
		char buf[S] = {0};               \
		is.read(buf, sizeof(buf));       \
		return read ## N((u8 *)buf);     \
	}

#define MAKE_STREAM_WRITE_FXN(T, N, S)              \
	inline void write ## N(std::ostream &os, T val) \
	{                                               \
		char buf[S];                                \
		write ## N((u8 *)buf, val);                 \
		os.write(buf, sizeof(buf));                 \
	}

MAKE_STREAM_READ_FXN(u8,    U8,       1);
MAKE_STREAM_READ_FXN(u16,   U16,      2);
MAKE_STREAM_READ_FXN(u32,   U32,      4);
MAKE_STREAM_READ_FXN(u64,   U64,      8);
MAKE_STREAM_READ_FXN(s8,    S8,       1);
MAKE_STREAM_READ_FXN(s16,   S16,      2);
MAKE_STREAM_READ_FXN(s32,   S32,      4);
MAKE_STREAM_READ_FXN(s64,   S64,      8);
MAKE_STREAM_READ_FXN(f32,   F1000,    4);
MAKE_STREAM_READ_FXN(f32,   F32,      4);
MAKE_STREAM_READ_FXN(v2s16, V2S16,    4);
MAKE_STREAM_READ_FXN(v3s16, V3S16,    6);
MAKE_STREAM_READ_FXN(v2s32, V2S32,    8);
MAKE_STREAM_READ_FXN(v3s32, V3S32,   12);
MAKE_STREAM_READ_FXN(v3f,   V3F1000, 12);
MAKE_STREAM_READ_FXN(v2f,   V2F32,    8);
MAKE_STREAM_READ_FXN(v3f,   V3F32,   12);
MAKE_STREAM_READ_FXN(video::SColor, ARGB8, 4);

MAKE_STREAM_WRITE_FXN(u8,    U8,       1);
MAKE_STREAM_WRITE_FXN(u16,   U16,      2);
MAKE_STREAM_WRITE_FXN(u32,   U32,      4);
MAKE_STREAM_WRITE_FXN(u64,   U64,      8);
MAKE_STREAM_WRITE_FXN(s8,    S8,       1);
MAKE_STREAM_WRITE_FXN(s16,   S16,      2);
MAKE_STREAM_WRITE_FXN(s32,   S32,      4);
MAKE_STREAM_WRITE_FXN(s64,   S64,      8);
MAKE_STREAM_WRITE_FXN(f32,   F1000,    4);
MAKE_STREAM_WRITE_FXN(f32,   F32,      4);
MAKE_STREAM_WRITE_FXN(v2s16, V2S16,    4);
MAKE_STREAM_WRITE_FXN(v3s16, V3S16,    6);
MAKE_STREAM_WRITE_FXN(v2s32, V2S32,    8);
MAKE_STREAM_WRITE_FXN(v3s32, V3S32,   12);
MAKE_STREAM_WRITE_FXN(v3f,   V3F1000, 12);
MAKE_STREAM_WRITE_FXN(v2f,   V2F32,    8);
MAKE_STREAM_WRITE_FXN(v3f,   V3F32,   12);
MAKE_STREAM_WRITE_FXN(video::SColor, ARGB8, 4);

////
//// More serialization stuff
////

[[nodiscard]] inline float clampToF1000(float v)
{
	return core::clamp(v, F1000_MIN, F1000_MAX);
}

[[nodiscard]] inline v3f clampToF1000(v3f v)
{
	return {clampToF1000(v.X), clampToF1000(v.Y), clampToF1000(v.Z)};
}

// Creates a string with the length as the first two bytes
std::string serializeString16(std::string_view plain);

// Reads a string with the length as the first two bytes
std::string deSerializeString16(std::istream &is);

// Creates a string with the length as the first four bytes
std::string serializeString32(std::string_view plain);

// Reads a string with the length as the first four bytes
std::string deSerializeString32(std::istream &is);

// Creates a string encoded in JSON format (almost equivalent to a C string literal)
std::string serializeJsonString(std::string_view plain);

// Reads a string encoded in JSON format
std::string deSerializeJsonString(std::istream &is);

// If the string contains spaces, quotes or control characters, encodes as JSON.
// Else returns the string unmodified.
std::string serializeJsonStringIfNeeded(std::string_view s);

// Parses a string serialized by serializeJsonStringIfNeeded.
std::string deSerializeJsonStringIfNeeded(std::istream &is);

// Serializes an array of strings (max 2^16 chars each)
// Output is well suited for compression :)
std::string serializeString16Array(const std::vector<std::string> &array);

// Deserializes a string array
std::vector<std::string> deserializeString16Array(std::istream &is);
