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

	static ClipMode toClipMode(u8 mode)
	{
		if (mode > (u8)ClipMode::MAX) {
			return ClipMode::NORMAL;
		}
		return (ClipMode)mode;
	}

	static ObjectFit toObjectFit(u8 fit)
	{
		if (fit > (u8)ObjectFit::MAX) {
			return ObjectFit::FIXED;
		}
		return (ObjectFit)fit;
	}

	static TextAlign toTextAlign(u8 align)
	{
		if (align > (u8)TextAlign::MAX) {
			return TextAlign::CENTER;
		}
		return (TextAlign)align;
	}

	void LayoutProps::reset()
	{
		type = LayoutType::PLACE;
		truncate = DirFlags::NONE;

		scale = 0.0f;
	}

	void LayoutProps::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			type = toLayoutType(readU8(is));
		if (testShift(set_mask))
			truncate = toDirFlags(readU8(is));

		if (testShift(set_mask))
			scale = std::max(readF32(is), 0.0f);
	}

	void SizingProps::reset()
	{
		min = SizeF(0.0f, 0.0f);

		margin = DispF(0.0f, 0.0f, 0.0f, 0.0f);
		padding = DispF(0.0f, 0.0f, 0.0f, 0.0f);

		pos = PosF(0.0f, 0.0f);
		size = SizeF(1.0f, 1.0f);
		anchor = PosF(0.0f, 0.0f);
	}

	void SizingProps::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			min = readSizeF(is).clip();

		if (testShift(set_mask))
			margin = readDispF(is);
		if (testShift(set_mask))
			padding = readDispF(is);

		if (testShift(set_mask))
			pos = readPosF(is);
		if (testShift(set_mask))
			size = readSizeF(is).clip();
		if (testShift(set_mask))
			anchor = readPosF(is).clamp(PosF(), PosF(1.0f, 1.0f));
	}

	void VisualProps::reset()
	{
		clip = ClipMode::NORMAL;
		hidden = false;

		fill = BLANK;
	}

	void VisualProps::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			clip = toClipMode(readU8(is));
		testShiftBool(set_mask, hidden);

		if (testShift(set_mask))
			fill = readARGB8(is);
	}

	void ImageProps::reset()
	{
		pane = nullptr;
		overlay = nullptr;

		tint = WHITE;
		slice = RectF(0.0f, 0.0f, 1.0f, 1.0f);

		frames = 1;
		frame_time = 1000;

		border = DispF(0.0f, 0.0f, 0.0f, 0.0f);
		tile = DirFlags::NONE;

		align = PosF(0.5f, 0.5f);
		scale = 1.0f;
	}

	void ImageProps::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			pane = g_manager.getTexture(readNullStr(is));
		if (testShift(set_mask))
			overlay = g_manager.getTexture(readNullStr(is));

		if (testShift(set_mask))
			tint = readARGB8(is);
		if (testShift(set_mask))
			slice = readRectF(is);

		if (testShift(set_mask))
			frames = std::max(readU32(is), 1U);
		if (testShift(set_mask))
			frame_time = std::max(readU32(is), 1U);

		if (testShift(set_mask))
			border = readDispF(is).clip();
		if (testShift(set_mask))
			tile = toDirFlags(readU8(is));

		if (testShift(set_mask))
			align = readPosF(is).clamp(PosF(), PosF(1.0f, 1.0f));
		if (testShift(set_mask))
			scale = std::max(readF32(is), 0.0f);
	}

	void TextProps::reset()
	{
		prepend = "";
		append = "";

		color = WHITE;
		mark = BLANK;
		size = 16;

		mono = false;
		italic = false;
		bold = false;

		align = TextAlign::CENTER;
		valign = TextAlign::CENTER;
	}

	void TextProps::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			prepend = readStr16(is);
		if (testShift(set_mask))
			append = readStr16(is);

		if (testShift(set_mask))
			color = readARGB8(is);
		if (testShift(set_mask))
			mark = readARGB8(is);
		if (testShift(set_mask))
			size = std::clamp(readU32(is), 1U, 999U);

		testShiftBool(set_mask, mono);
		testShiftBool(set_mask, italic);
		testShiftBool(set_mask, bold);

		if (testShift(set_mask))
			align = toTextAlign(readU8(is));
		if (testShift(set_mask))
			valign = toTextAlign(readU8(is));
	}

	void StyleProps::reset()
	{
		layout.reset();
		sizing.reset();
		visual.reset();
		img.reset();
		text.reset();
	}

	void StyleProps::read(std::istream &is)
	{
		layout.read(is);
		sizing.read(is);
		visual.read(is);
		img.read(is);
		text.read(is);
	}
}
