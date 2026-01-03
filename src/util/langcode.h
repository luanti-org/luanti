// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <vector>
#include <string>

std::vector<std::wstring> parse_language_list(const std::wstring &lang);
std::wstring expand_language_list(const std::wstring &lang);
