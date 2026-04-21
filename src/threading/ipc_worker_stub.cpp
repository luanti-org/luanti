// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ipc_worker_stub.h"

#include "ipc_channel.h"
#include "log.h"
#include <iostream>

// Worker-side idle timeout: if we don't hear from the parent for this many
// milliseconds we give up and exit. Default is 2 minutes.
static constexpr int WORKER_RECV_TIMEOUT_MS = 2 * 60 * 1000;

int run_sscsm_worker(const std::string &shm_name)
{
	auto resources = IPCChannelResourcesShm::makeSecond(shm_name);
	if (!resources) {
		errorstream << "sscsm-worker: failed to attach to shm '"
				<< shm_name << "'" << std::endl;
		return 1;
	}

	auto end_b = IPCChannelEnd::makeB(std::move(resources));

	// Echo loop: receive, send back, stop on empty message.
	while (true) {
		if (!end_b.recvWithTimeout(WORKER_RECV_TIMEOUT_MS)) {
			warningstream << "sscsm-worker: recv timeout, exiting"
					<< std::endl;
			return 2;
		}
		if (!end_b.sendWithTimeout(end_b.getRecvData(),
				end_b.getRecvSize(), WORKER_RECV_TIMEOUT_MS)) {
			warningstream << "sscsm-worker: send timeout, exiting"
					<< std::endl;
			return 3;
		}
		if (end_b.getRecvSize() == 0)
			break;
	}

	return 0;
}
