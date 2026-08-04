// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021-2025 sfan5

#include "irrlichttypes.h"
#include <vector>
#include <algorithm>
#include <cassert>

/**
 * Rudimentary header-only 2D bitmap class.
 * @warning not thread-safe
 */
class Bitmap {
	u32 linesize, lines;
	std::vector<u8> data;

	// NOTE: do not rely on the value of unused padding bits in `data`

	static inline u32 bytepos(u32 index) { return index >> 3; }
	static inline u8 bitpos(u32 index) { return index & 7; }

	template<bool set, bool toggle, bool clear>
	bool modify_(u32 x, u32 y)
	{
		u32 index = y * linesize + x;
		u8 mask = 1 << bitpos(index);
		u8 byte = data[bytepos(index)];
		if constexpr (set)
			byte |= mask;
		else if constexpr (toggle)
			byte ^= mask;
		else if constexpr (clear)
			byte &= ~mask;
		data[bytepos(index)] = byte;
		return byte & mask;
	}

public:
	/// @brief Create an empty bitmap
	Bitmap() : linesize(0), lines(0) {}

	/// @brief Create a new zero-filled bitmap
	Bitmap(u32 width, u32 height)
	{
		resize(width, height);
	}

	inline u32 width() const { return linesize; }
	inline u32 height() const { return lines; }

	inline void resize(u32 width, u32 height, bool initial_value=false)
	{
		assert(width <= 65534 && height <= 65534); // index would overflow
		linesize = width;
		lines = height;
		data.clear(); // make sure to discard all data
		if (width && height)
			data.resize(bytepos(width * height) + 1, static_cast<u8>(initial_value ? 0xff : 0));
	}

	inline void reset(bool value)
	{
		std::fill(data.begin(), data.end(), value ? 0xff : 0);
	}

	inline bool get(u32 x, u32 y) const
	{
		u32 index = y * linesize + x;
		return data[bytepos(index)] & (1 << bitpos(index));
	}

	inline void set(u32 x, u32 y)   { modify_<1, 0, 0>(x, y); }
	inline void unset(u32 x, u32 y) { modify_<0, 0, 1>(x, y); }
	inline bool toggle(u32 x, u32 y) { return modify_<0, 1, 0>(x, y); }

	/// @brief Returns true if all bits in the bitmap are set
	inline bool all() const
	{
		if (!linesize || !lines)
			return (assert(0), true);
		for (u32 i = 0; i < data.size() - 1; i++) {
			if (data[i] != 0xff)
				return false;
		}
		u8 last_byte = data.back(); // contains padding
		u8 leftover_bits = bitpos(linesize * lines); // [0, 7]
		u8 leftover_mask = (1 << leftover_bits) - 1; // 0, 1, 3, 7, ... or 127
		return (last_byte & leftover_mask) == leftover_mask;
	}

	/// @brief Returns true if no bits in the bitmap are set
	inline bool none() const
	{
		if (!linesize || !lines)
			return (assert(0), true);
		for (u32 i = 0; i < data.size() - 1; i++) {
			if (data[i] != 0)
				return false;
		}
		u8 last_byte = data.back();
		u8 leftover_bits = bitpos(linesize * lines);
		u8 leftover_mask = (1 << leftover_bits) - 1;
		return (last_byte & leftover_mask) == 0;
	}
};

// Note: a 3D class could be written based on the 2D one. Or maybe the other way?
