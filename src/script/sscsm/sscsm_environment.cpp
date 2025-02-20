// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_environment.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "sscsm_stupid_channel.h"


void *SSCSMEnvironment::run()
{
	while (true) {
		auto next_event = requestPollNextEvent();

		if (dynamic_cast<SSCSMEventTearDown *>(next_event.get())) {
			break;
		}

		next_event->exec(this);
	}

	return nullptr;
}

SerializedSSCSMAnswer SSCSMEnvironment::exchange(SerializedSSCSMRequest req)
{
	return m_channel->exchangeA(std::move(req));
}

std::unique_ptr<ISSCSMEvent> SSCSMEnvironment::requestPollNextEvent()
{
	auto request = SSCSMRequestPollNextEvent{};
	auto answer = deserializeSSCSMAnswer<SSCSMRequestPollNextEvent::Answer>(
			exchange(serializeSSCSMRequest(request))
		);
	return std::move(answer.next_event);
}

MapNode SSCSMEnvironment::requestGetNode(v3s16 pos)
{
	auto request = SSCSMRequestGetNode{};
	request.pos = pos;
	auto answer = deserializeSSCSMAnswer<SSCSMRequestGetNode::Answer>(
			exchange(serializeSSCSMRequest(request))
		);
	return answer.node;
}
