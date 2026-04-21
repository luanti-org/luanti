// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

/*
	Worker process entry point used by `luanti --sscsm-worker <shmname>`.

	Attaches to the shared-memory IPC channel with the given name, constructs
	an SSCSMEnvironment, and runs its event loop until teardown.

	Returns a process exit code:
	  0: clean teardown
	  1: failed to attach to shm
	  2: unhandled exception in environment
*/
int run_sscsm_worker(const std::string &shm_name);
