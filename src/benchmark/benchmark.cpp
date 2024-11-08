// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest Authors

#include "benchmark/benchmark.h"

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include "catch.h"

bool run_benchmarks(const char *arg)
{
	const char *const argv[] = {
		"MinetestBenchmark", arg, nullptr
	};
	const int argc = arg ? 2 : 1;
	int errCount = Catch::Session().run(argc, argv);
	return errCount == 0;
}
