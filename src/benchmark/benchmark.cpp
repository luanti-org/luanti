// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Luanti Authors

#include "benchmark/benchmark.h"

#include "catch.h"

int run_catch2_benchmarks(int argc, char *argv[])
{
	Catch::Session session;

	int status = session.applyCommandLine(argc, argv);
	if (status != 0)
		return status;

	auto config = session.configData();
	config.skipBenchmarks = false;
	session.useConfigData(config);

	return session.run();
}
