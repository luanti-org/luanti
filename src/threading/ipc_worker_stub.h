// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

/*
	Stub worker entry point used by `luanti --sscsm-worker <shmname>`.

	Attaches to the shared-memory channel with the given name and runs
	a simple echo loop until the parent sends an empty message.

	This is a placeholder — when SSCSM is wired to use the real IPC
	channel (item 3 of the roadmap), this will be replaced by the
	actual SSCSM environment entry point.

	Returns a process exit code.
*/
int run_sscsm_worker(const std::string &shm_name);
