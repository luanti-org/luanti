// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include "client/client.h"
#include "threading/thread.h"
#include "sscsm_controller.h"
#include "sscsm_irequest.h"

// The thread that runs SSCSM code.
// Meant to be replaced by a sandboxed process.
class SSCSMEnvironment : public Thread
{
	std::shared_ptr<StupidChannel> m_channel;

	void *run() override;

	SerializedSSCSMAnswer exchange(SerializedSSCSMRequest req);

public:
	SSCSMEnvironment(std::shared_ptr<StupidChannel> channel) :
		Thread("SSCSMEnvironment-thread"),
		m_channel(std::move(channel))
	{
	}

	std::unique_ptr<ISSCSMEvent> requestPollNextEvent();
	MapNode requestGetNode(v3s16 pos);
};
