// SPDX-FileCopyrightText: 2024 Luanti Contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>
#include <string_view>

namespace hashing
{

// Size of raw digest in bytes
constexpr size_t SHA1_DIGEST_SIZE = 20;
constexpr size_t SHA256_DIGEST_SIZE = 32;

// Returns the digest of data
std::string sha1(std::string_view data);
std::string sha256(std::string_view data);

}
