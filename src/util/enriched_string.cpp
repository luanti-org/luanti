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

#include "enriched_string.h"
#include "util/string.h"
#include "debug.h"
#include "log.h"

using namespace video;

EnrichedString::EnrichedString()
{
	clear();
}

EnrichedString::EnrichedString(std::wstring_view string,
		const std::vector<SColor> &colors)
{
	clear();
	m_string = string;
	m_colors = colors;
}

EnrichedString::EnrichedString(std::wstring_view s, const SColor &color)
{
	clear();
	addAtEnd(translate_string(s), color);
}

void EnrichedString::clear()
{
	m_string.clear();
	m_colors.clear();
	m_has_background = false;
	m_default_length = 0;
	m_default_color = video::SColor(255, 255, 255, 255);
	m_background = video::SColor(0, 0, 0, 0);
}

EnrichedString &EnrichedString::operator=(std::wstring_view str)
{
	clear();
	addAtEnd(translate_string(str), m_default_color);
	return *this;
}

void EnrichedString::addAtEnd(std::wstring_view s, SColor initial_color)
{
	SColor color(initial_color);
	bool use_default = (m_default_length == m_string.size() &&
		color == m_default_color);

	m_string.reserve(m_string.size() + s.size());
	m_colors.reserve(m_colors.size() + s.size());

	size_t i = 0;
	while (i < s.length()) {
		if (s[i] != L'\x1b') {
			m_string += s[i];
			m_colors.push_back(color);
			++i;
			continue;
		}
		++i;
		size_t start_index = i;
		size_t length;
		if (i == s.length()) {
			break;
		}
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
		if (parts[0] == L"c") {
			if (parts.size() < 2) {
				continue;
			}
			parseColorString(wide_to_utf8(parts[1]), color, true);

			// No longer use default color after first escape
			if (use_default) {
				m_default_length = m_string.size();
				use_default = false;
			}
		} else if (parts[0] == L"b") {
			if (parts.size() < 2) {
				continue;
			}
			parseColorString(wide_to_utf8(parts[1]), m_background, true);
			m_has_background = true;
		}
	}

	// Update if no escape character was found
	if (use_default)
		m_default_length = m_string.size();
}

void EnrichedString::addChar(const EnrichedString &source, size_t i)
{
	m_string += source.m_string[i];
	m_colors.push_back(source.m_colors[i]);
}

void EnrichedString::addCharNoColor(wchar_t c)
{
	m_string += c;
	if (m_colors.empty()) {
		m_colors.emplace_back(m_default_color);
	} else {
		m_colors.push_back(m_colors.back());
	}
}

EnrichedString EnrichedString::operator+(const EnrichedString &other) const
{
	EnrichedString result = *this;
	result += other;
	return result;
}

void EnrichedString::operator+=(const EnrichedString &other)
{
	bool update_default_color = m_default_length == m_string.size();

	m_string += other.m_string;
	m_colors.insert(m_colors.end(), other.m_colors.begin(), other.m_colors.end());

	if (update_default_color) {
		m_default_length += other.m_default_length;
		updateDefaultColor();
	}
}

EnrichedString EnrichedString::getNextLine(size_t *pos) const
{
	size_t str_pos = *pos;

	// Split per line
	size_t str_nl = getString().find(L'\n', str_pos);
	if (str_nl == std::wstring::npos)
		str_nl = getString().size();
	EnrichedString line = substr(str_pos, str_nl - str_pos);
	str_pos += line.size() + 1;

	*pos = str_pos;
	return line;
}

EnrichedString EnrichedString::substr(size_t pos, size_t len) const
{
	if (pos >= m_string.length())
		return EnrichedString();

	if (len == std::string::npos || pos + len > m_string.length())
		len = m_string.length() - pos;

	EnrichedString str(
		m_string.substr(pos, len),
		std::vector<SColor>(m_colors.begin() + pos, m_colors.begin() + pos + len)
	);

	str.m_has_background = m_has_background;
	str.m_background = m_background;

	if (pos < m_default_length)
		str.m_default_length = std::min(m_default_length - pos, str.size());
	str.setDefaultColor(m_default_color);
	return str;
}

const std::vector<SColor> &EnrichedString::getColors() const
{
	return m_colors;
}

const std::wstring &EnrichedString::getString() const
{
	return m_string;
}

void EnrichedString::updateDefaultColor()
{
	sanity_check(m_default_length <= m_colors.size());

	for (size_t i = 0; i < m_default_length; ++i)
		m_colors[i] = m_default_color;
}
