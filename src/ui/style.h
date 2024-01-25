// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#pragma once

#include "ui/helpers.h"

#include <iostream>
#include <string>

namespace ui
{
	// Serialized enum; do not change order of entries.
	enum class LayoutType : u8
	{
		PLACE,

		MAX = PLACE,
	};

	// Serialized enum; do not change order of entries.
	enum class DirFlags : u8
	{
		NONE,
		X,
		Y,
		BOTH,

		MAX = BOTH,
	};

	// Serialized enum; do not change order of entries.
	enum class DisplayMode : u8
	{
		VISIBLE,
		OVERFLOW,
		HIDDEN,
		CLIPPED,

		MAX = CLIPPED,
	};

	// Serialized enum; do not change order of entries.
	enum class IconPlace : u8
	{
		CENTER,
		LEFT,
		TOP,
		RIGHT,
		BOTTOM,

		MAX = BOTTOM,
	};

	struct Layout
	{
		LayoutType type;
		DirFlags clip;

		float scale;

		Layout() { reset(); }

		void reset();
		void read(std::istream &is);
	};

	struct Sizing
	{
		SizeF size;
		SizeF span;

		PosF pos;
		PosF anchor;

		DispF margin;
		DispF padding;

		Sizing() { reset(); }

		void reset();
		void read(std::istream &is);
	};

	struct Layer
	{
		video::ITexture *image;
		video::SColor fill;
		video::SColor tint;

		float scale;
		RectF source;

		u32 num_frames;
		u32 frame_time;

		Layer() { reset(); }

		void reset();
		void read(std::istream &is);
	};

	struct Style
	{
		Layout layout;
		Sizing sizing;

		DisplayMode display;

		Layer box;
		Layer icon;

		DispF box_middle;
		DirFlags box_tile;

		IconPlace icon_place;
		float icon_gutter;
		bool icon_overlap;

		Style() { reset(); }

		void reset();
		void read(std::istream &is);
	};
}
