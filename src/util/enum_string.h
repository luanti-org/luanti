// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#pragma once

#include <string_view>
#include <type_traits>

struct EnumString
{
	int num;
	const char *str;
};

bool string_to_enum(const EnumString *spec, int &result, std::string_view str);

template <typename T, std::enable_if_t<std::is_enum_v<T>, bool> = true>
bool string_to_enum(const EnumString *spec, T &result, std::string_view str)
{
	int result_int = result;
	bool ret = string_to_enum(spec, result_int, str);
	result = static_cast<T>(result_int);
	return ret;
}

[[nodiscard]] const char *enum_to_string(const EnumString *spec, int num);
