// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 sfan5 <sfan5@live.de>

#pragma once

#include <iostream>
#include "irrlichttypes_bloated.h"

enum TileAnimationType : u8
{
	TAT_NONE = 0,
	TAT_VERTICAL_FRAMES = 1,
	TAT_SHEET_2D = 2,
};

struct TileAnimationParams
{
	enum TileAnimationType type = TileAnimationType::TAT_NONE;
	union
	{
		// struct {
		// } none;
		struct
		{
			int aspect_w; // width for aspect ratio
			int aspect_h; // height for aspect ratio
			float length; // seconds
		} vertical_frames;
		struct
		{
			int frames_w;       // number of frames left-to-right
			int frames_h;       // number of frames top-to-bottom
			float frame_length; // seconds
		} sheet_2d;
	};

	void serialize(std::ostream &os, u16 protocol_ver) const;
	void deSerialize(std::istream &is, u16 protocol_ver);
	void determineParams(v2u32 texture_size, int *frame_count, int *frame_length_ms,
			v2u32 *frame_size) const;
	void getTextureModifer(std::ostream &os, v2u32 texture_size, int frame) const;
	v2f getTextureCoords(v2u32 texture_size, int frame) const;
};
