// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 cx384

#pragma once

#include "util/enriched_string.h"
#include "fontengine.h"
#include "rect.h"

// Note this is client code, because of the draw function.

// A text consisting of multiple enriched strings which are drawn with different fonts
struct FontEnrichedString
{
	FontEnrichedString(const std::wstring &s,
			video::SColor initial_color = video::SColor(255, 255, 255, 255),
			const FontSpec &initial_font = FontSpec(FONT_SIZE_UNSPECIFIED, FM_Standard,
			false, false));

	void draw(core::rect<s32> position) const;

	core::dimension2d<u32> getDimension() const;

private:
	using Line = std::vector<std::pair<EnrichedString, FontSpec>>;
	std::vector<Line> m_lines;
};
