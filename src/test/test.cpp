// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti Authors

#include "test/test.h"

#include "catch.h"

int run_catch2_tests(int argc, char *argv[])
{
	Catch::Session session;

	int status = session.applyCommandLine(argc, argv);
	if (status != 0)
		return status;

	auto config = session.configData();
	config.skipBenchmarks = true;
	session.useConfigData(config);

	return session.run();
}
