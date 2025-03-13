// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#include "util/enum_string.h"
#include <cassert>

bool string_to_enum(const EnumString *spec, int &result, std::string_view str)
{
	const EnumString *esp = spec;
	while (esp->str) {
		if (str == esp->str) {
			assert(esp->num >= 0);
			result = esp->num;
			return true;
		}
		esp++;
	}
	return false;
}

const char *enum_to_string(const EnumString *spec, int num)
{
	if (num < 0)
		return nullptr;
	// assume array order matches enum order
	auto *p = &spec[num];
	assert(p->num == num);
	assert(p->str);
	return p->str;
}
