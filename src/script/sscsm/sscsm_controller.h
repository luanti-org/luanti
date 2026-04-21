// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include "irrlichttypes.h"
#include "sscsm_irequest.h"
#include "sscsm_ievent.h"
#include "threading/ipc_channel.h"
#include "util/basic_macros.h"

class Client;
class IPCChildProcess;

/**
 * The purpose of this class is to:
 * * Be the RAII owner of the SSCSM worker process.
 * * Send events to SSCSM process, and process requests. (`runEvent`)
 * * Hide details (e.g. that it is a separate process, or that it has to do IPC calls).
 *
 * See also SSCSMEnvironment for the other side (runs in the worker process).
 */
class SSCSMController
{
	IPCChannelEnd m_channel;
	std::unique_ptr<IPCChildProcess> m_process;

	SerializedSSCSMAnswer handleRequest(Client *client, ISSCSMRequest *req);

public:
	// Spawns the worker process and waits for its initial PollNextEvent
	// request. Returns nullptr on failure (e.g. child exited before ready).
	static std::unique_ptr<SSCSMController> create();

	SSCSMController(IPCChannelEnd channel,
			std::unique_ptr<IPCChildProcess> process);

	~SSCSMController();

	DISABLE_CLASS_COPY(SSCSMController);

	// Handles requests until the next event is polled
	void runEvent(Client *client, std::unique_ptr<ISSCSMEvent> event);
};
