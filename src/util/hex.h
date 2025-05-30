// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 Jonathan Neuschäfer <j.neuschaefer@gmx.net>

#pragma once

#include <string>
#include <string_view>

static const char hex_chars[] = "0123456789abcdef";

[[nodiscard]]
static inline std::string hex_encode(std::string_view data)
{
	std::string ret;
	ret.reserve(data.size() * 2);
	for (unsigned char c : data) {
		ret.push_back(hex_chars[(c & 0xf0) >> 4]);
		ret.push_back(hex_chars[c & 0x0f]);
	}
	return ret;
}

[[nodiscard]]
static inline std::string hex_encode(const char *data, size_t data_size)
{
	if (!data_size)
		return "";
	return hex_encode(std::string_view(data, data_size));
}

static inline bool hex_digit_decode(char hexdigit, unsigned char &value)
{
	if (hexdigit >= '0' && hexdigit <= '9')
		value = hexdigit - '0';
	else if (hexdigit >= 'A' && hexdigit <= 'F')
		value = hexdigit - 'A' + 10;
	else if (hexdigit >= 'a' && hexdigit <= 'f')
		value = hexdigit - 'a' + 10;
	else
		return false;
	return true;
}
