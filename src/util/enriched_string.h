/*
Copyright (C) 2013 xyz, Ilya Zhuravlev <whatever@xyz.is>
Copyright (C) 2016 Nore, Nathanaëlle Courant <nore@mesecons.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <SColor.h>


class EnrichedString {
public:
	EnrichedString();
	EnrichedString(std::wstring_view s,
		const video::SColor &color = video::SColor(255, 255, 255, 255));
	EnrichedString(std::wstring_view string,
		const std::vector<video::SColor> &colors);
	EnrichedString &operator=(std::wstring_view s);

	void clear();

	void addAtEnd(std::wstring_view s, video::SColor color);

	// Adds the character source[i] at the end.
	// An EnrichedString should always be able to be copied
	// to the end of an existing EnrichedString that way.
	void addChar(const EnrichedString &source, size_t i);

	// Adds a single character at the end, without specifying its
	// color. The color used will be the one from the last character.
	void addCharNoColor(wchar_t c);

	EnrichedString getNextLine(size_t *pos) const;
	EnrichedString substr(size_t pos = 0, size_t len = std::string::npos) const;

	EnrichedString operator+(const EnrichedString &other) const;
	void operator+=(const EnrichedString &other);
	void operator+=(std::wstring_view other)
	{
		*this += EnrichedString(other);
	}

	const wchar_t *c_str() const
	{
		return getString().c_str();
	}
	const std::vector<video::SColor> &getColors() const;
	const std::wstring &getString() const;

	inline void setDefaultColor(video::SColor color)
	{
		m_default_color = color;
		updateDefaultColor();
	}
	void updateDefaultColor();
	inline const video::SColor &getDefaultColor() const
	{
		return m_default_color;
	}

	inline bool operator==(const EnrichedString &other) const
	{
		return (m_string == other.m_string && m_colors == other.m_colors);
	}
	inline bool operator!=(const EnrichedString &other) const
	{
		return !(*this == other);
	}
	inline bool empty() const
	{
		return m_string.empty();
	}
	inline size_t size() const
	{
		return m_string.size();
	}

	inline bool hasBackground() const
	{
		return m_has_background;
	}
	inline video::SColor getBackground() const
	{
		return m_background;
	}
	inline void setBackground(video::SColor color)
	{
		m_background = color;
		m_has_background = true;
	}

private:
	std::wstring m_string;
	std::vector<video::SColor> m_colors;
	bool m_has_background;
	video::SColor m_default_color;
	video::SColor m_background;
	// This variable defines the length of the default-colored text.
	size_t m_default_length = 0;
};
