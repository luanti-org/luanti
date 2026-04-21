// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_worker_entry.h"

#include "log.h"
#include "sscsm_environment.h"
#include "threading/ipc_channel.h"

int run_sscsm_worker(const std::string &shm_name)
{
	auto resources = IPCChannelResourcesShm::makeSecond(shm_name);
	if (!resources) {
		errorstream << "sscsm-worker: failed to attach to shm '"
				<< shm_name << "'" << std::endl;
		return 1;
	}

	auto end_b = IPCChannelEnd::makeB(std::move(resources));

	try {
		SSCSMEnvironment env(std::move(end_b));
		env.run();
	} catch (std::exception &e) {
		errorstream << "sscsm-worker: unhandled exception: " << e.what()
				<< std::endl;
		return 2;
	}

	return 0;
}
