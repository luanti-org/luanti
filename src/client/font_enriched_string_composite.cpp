// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 cx384

#include "font_enriched_string_composite.h"
#include "irrlicht_changes/CGUITTFont.h"
#include "util/string.h"
#include <utility>

static bool parseFontModifier(std::string_view s, FontModifier &modifier) {
	if (s == "mono")
		modifier = FontModifier::Mono;
	else if (s == "unmono")
		modifier = FontModifier::Unmono;
	else if (s == "bold")
		modifier = FontModifier::Bold;
	else if (s == "unbold")
		modifier = FontModifier::Unbold;
	else if (s == "italic")
		modifier = FontModifier::Italic;
	else if (s == "unitalic")
		modifier = FontModifier::Unitalic;
	else
		return false;
	return true;
};

FontEnrichedStringComposite::FontEnrichedStringComposite(const std::wstring &s,
		const video::SColor &initial_color, const FontSpec &initial_font) {

	FontSpec font = initial_font;
	video::SColor color = initial_color;

	// Handle font escape sequence like in EnrichedString and translate_string
	Line line;
	size_t fragmen_start = 0;
	size_t i = 0;
	while (i < s.length()) {
		if (s[i] == L'\n') {
			// Split lines

			EnrichedString fragment(std::wstring(s, fragmen_start, i-fragmen_start), color);
			line.push_back(std::make_pair(fragment, font));

			auto colors = fragment.getColors();
			if (!colors.empty())
				color =	colors.back();

			if (!line.empty()) {
				m_lines.emplace_back(std::move(line));
				line = Line();
			}
			i++;
			fragmen_start = i;
			continue;
		} else if (s[i] != L'\x1b') {
			i++;
			continue;
		}
		i++;
		size_t start_index = i;
		size_t length;
		if (i == s.length())
			break;
		if (s[i] == L'(') {
			++i;
			++start_index;
			while (i < s.length() && s[i] != L')') {
				if (s[i] == L'\\') {
					++i;
				}
				++i;
			}
			length = i - start_index;
			++i;
		} else {
			++i;
			length = 1;
		}
		std::wstring escape_sequence(s, start_index, length);
		std::vector<std::wstring> parts = split(escape_sequence, L'@');
		if (parts[0] == L"f") {
			if (parts.size() < 2) {
				continue;
			}
			FontModifier modifier;
			if (parseFontModifier(wide_to_utf8(parts[1]), modifier)) {
				EnrichedString fragment(std::wstring(s, fragmen_start, start_index-fragmen_start), color);
				line.push_back(std::make_pair(fragment, font));

				fragmen_start = start_index + length + 1;
				font.applyFontModifier(modifier);

				auto colors = fragment.getColors();
				if (!colors.empty())
					color =	colors.back();
			}
		}
	}
	if (fragmen_start < s.length()) {
		EnrichedString fragment(std::wstring(s, fragmen_start), color);
		line.push_back(std::make_pair(fragment, font));
	}

	if (!line.empty())
		m_lines.push_back(line);

};

void FontEnrichedStringComposite::draw(core::rect<s32> position) const {
	u32 start_pos_x = position.UpperLeftCorner.X;
	for (auto line : m_lines) {
		position.UpperLeftCorner.X = start_pos_x;
		u32 max_h = 0;
		for (auto [es, spec] : line) {
			gui::IGUIFont *font = g_fontengine->getFont(spec);
			assert(font);
			gui::CGUITTFont *ttfont = dynamic_cast<gui::CGUITTFont*>(font);
			if (ttfont) { // Don't draw other fonts
				ttfont->draw(es, position);

				auto frag_dim = ttfont->getDimension(es.c_str());
				position.UpperLeftCorner.X += frag_dim.Width;
				if (frag_dim.Height > max_h)
					max_h = frag_dim.Height;
			}
		}
		position.UpperLeftCorner.Y += max_h;
	}
};

core::dimension2d<u32> FontEnrichedStringComposite::getDimension() const {
	core::dimension2d<u32> dim(0, 0);
	for (auto line : m_lines) {
		u32 max_h = 0;
		u32 sum_w = 0;
		for (auto [es, spec] : line) {
			gui::IGUIFont *font = g_fontengine->getFont(spec);
			assert(font);
			gui::CGUITTFont *ttfont = dynamic_cast<gui::CGUITTFont*>(font);
			if (ttfont) {
				auto frag_dim = ttfont->getDimension(es.c_str());
				sum_w += frag_dim.Width;
				if (frag_dim.Height > max_h)
					max_h = frag_dim.Height;
			}
		}
		dim.Height += max_h;
		if (dim.Width < sum_w)
			dim.Width = sum_w;
	}
	return dim;
};
