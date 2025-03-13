// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 Jonathan Neuschäfer <j.neuschaefer@gmx.net>

#pragma once

#include <string>
#include <string_view>

static const char hex_chars[] = "0123456789abcdef";

static inline std::string hex_encode(const char *data, unsigned int data_size)
{
	std::string ret;
	ret.reserve(data_size * 2);

	char buf2[3];
	buf2[2] = '\0';

	for (unsigned int i = 0; i < data_size; i++) {
		unsigned char c = (unsigned char)data[i];
		buf2[0] = hex_chars[(c & 0xf0) >> 4];
		buf2[1] = hex_chars[c & 0x0f];
		ret.append(buf2);
	}

	return ret;
}

static inline std::string hex_encode(std::string_view data)
{
	return hex_encode(data.data(), data.size());
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
