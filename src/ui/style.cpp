// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#include "ui/style.h"

#include "debug.h"
#include "log.h"
#include "ui/manager.h"
#include "util/serialize.h"

namespace ui
{
	static LayoutType toLayoutType(u8 type)
	{
		if (type > (u8)LayoutType::MAX) {
			return LayoutType::PLACE;
		}
		return (LayoutType)type;
	}

	static DirFlags toDirFlags(u8 dir)
	{
		if (dir > (u8)DirFlags::MAX) {
			return DirFlags::NONE;
		}
		return (DirFlags)dir;
	}

	static DisplayMode toDisplayMode(u8 mode)
	{
		if (mode > (u8)DisplayMode::MAX) {
			return DisplayMode::VISIBLE;
		}
		return (DisplayMode)mode;
	}

	static IconPlace toIconPlace(u8 place)
	{
		if (place > (u8)IconPlace::MAX) {
			return IconPlace::CENTER;
		}
		return (IconPlace)place;
	}

	void Layout::reset()
	{
		type = LayoutType::PLACE;
		clip = DirFlags::NONE;

		scale = 0.0f;
	}

	void Layout::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			type = toLayoutType(readU8(is));
		if (testShift(set_mask))
			clip = toDirFlags(readU8(is));

		if (testShift(set_mask))
			scale = std::max(readF32(is), 0.0f);
	}

	void Sizing::reset()
	{
		size = SizeF(0.0f, 0.0f);
		span = SizeF(1.0f, 1.0f);

		pos = PosF(0.0f, 0.0f);
		anchor = PosF(0.0f, 0.0f);

		margin = DispF(0.0f, 0.0f, 0.0f, 0.0f);
		padding = DispF(0.0f, 0.0f, 0.0f, 0.0f);
	}

	void Sizing::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			size = readSizeF(is).clip();
		if (testShift(set_mask))
			span = readSizeF(is).clip();

		if (testShift(set_mask))
			pos = readPosF(is);
		if (testShift(set_mask))
			anchor = readPosF(is);

		if (testShift(set_mask))
			margin = readDispF(is);
		if (testShift(set_mask))
			padding = readDispF(is);
	}

	void Layer::reset()
	{
		image = nullptr;
		fill = BLANK;
		tint = WHITE;

		scale = 1.0f;
		source = RectF(0.0f, 0.0f, 1.0f, 1.0f);

		num_frames = 1;
		frame_time = 1000;
	}

	void Layer::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			image = g_manager.getTexture(readNullStr(is));
		if (testShift(set_mask))
			fill = readARGB8(is);
		if (testShift(set_mask))
			tint = readARGB8(is);

		if (testShift(set_mask))
			scale = std::max(readF32(is), 0.0f);
		if (testShift(set_mask))
			source = readRectF(is);

		if (testShift(set_mask))
			num_frames = std::max(readU32(is), 1U);
		if (testShift(set_mask))
			frame_time = std::max(readU32(is), 1U);
	}

	void Style::reset()
	{
		layout.reset();
		sizing.reset();

		display = DisplayMode::VISIBLE;

		box.reset();
		icon.reset();

		box_middle = DispF(0.0f, 0.0f, 0.0f, 0.0f);
		box_tile = DirFlags::NONE;

		icon_place = IconPlace::CENTER;
		icon_gutter = 0.0f;
		icon_overlap = false;
	}

	void Style::read(std::istream &is)
	{
		// No need to read a size prefix; styles are already read in as size-
		// prefixed strings in Window.
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			layout.read(is);
		if (testShift(set_mask))
			sizing.read(is);

		if (testShift(set_mask))
			display = toDisplayMode(readU8(is));

		if (testShift(set_mask))
			box.read(is);
		if (testShift(set_mask))
			icon.read(is);

		if (testShift(set_mask))
			box_middle = readDispF(is).clip();
		if (testShift(set_mask))
			box_tile = toDirFlags(readU8(is));

		if (testShift(set_mask))
			icon_place = toIconPlace(readU8(is));
		if (testShift(set_mask))
			icon_gutter = readF32(is);
		testShiftBool(set_mask, icon_overlap);
	}
}
