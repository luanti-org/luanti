// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#pragma once

#include <string_view>

struct EnumString
{
	int num;
	const char *str;
};

bool string_to_enum(const EnumString *spec, int &result, std::string_view str);

const char *enum_to_string(const EnumString *spec, int num);
