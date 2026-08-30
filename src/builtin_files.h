// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti contributors

#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

// Sha256 sums of all relevant files in builtin, generated on build
// relative path (with /) -> hex(sha256(filecontent))
extern const std::unordered_map<std::string_view, std::string_view> g_builtin_file_sha256_map;

// Files that should be loaded as part of SSCSM client-builtin
// relative paths (with /)
extern const std::vector<std::string_view> g_builtin_sscsm_client_files;
