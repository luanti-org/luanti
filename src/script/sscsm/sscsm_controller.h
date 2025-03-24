// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include "irrlichttypes.h"
#include "sscsm_irequest.h"
#include "sscsm_ievent.h"
#include "util/basic_macros.h"

class SSCSMEnvironment;
class StupidChannel;

class SSCSMController
{
	std::unique_ptr<SSCSMEnvironment> m_thread;
	std::shared_ptr<StupidChannel> m_channel;

	SerializedSSCSMAnswer handleRequest(Client *client, ISSCSMRequest *req);

public:
	static std::unique_ptr<SSCSMController> create();

	SSCSMController(std::unique_ptr<SSCSMEnvironment> thread,
			std::shared_ptr<StupidChannel> channel);

	~SSCSMController();

	DISABLE_CLASS_COPY(SSCSMController);

	// Handles requests until the next event is polled
	void runEvent(Client *client, std::unique_ptr<ISSCSMEvent> event);
};
