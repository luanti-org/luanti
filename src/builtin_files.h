// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti contributors

#pragma once

#include <string>
#include <unordered_map>

const std::unordered_map<std::string, std::string> &get_builtin_file_sha256_map();

std::string get_builtin_file_sha256(const std::string &rel_path);
