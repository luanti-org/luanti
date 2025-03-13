// SPDX-FileCopyrightText: 2024 Luanti Contributors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"
#include "util/hashing.h"
#include <string>

TEST_CASE("benchmark_sha")
{
	std::string input;
	input.resize(100000);

	BENCHMARK("sha1_input100000", i) {
		input[0] = (char)i;
		return hashing::sha1(input);
	};

	BENCHMARK("sha256_input100000", i) {
		input[0] = (char)i;
		return hashing::sha256(input);
	};
}
