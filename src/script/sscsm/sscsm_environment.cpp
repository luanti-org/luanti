// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_environment.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "sscsm_stupid_channel.h"


SSCSMEnvironment::SSCSMEnvironment(std::shared_ptr<StupidChannel> channel) :
	Thread("SSCSMEnvironment-thread"),
	m_channel(std::move(channel)),
	m_script(std::make_unique<SSCSMScripting>(this))
{
}

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

void SSCSMEnvironment::updateVFSFiles(std::vector<std::pair<std::string, std::string>> &&files)
{
	for (auto &&p : files) {
		m_vfs.emplace(std::move(p.first), std::move(p.second));
	}
}

void SSCSMEnvironment::setFatalError(const std::string &reason)
{
	//TODO
	// what to do on error?
	// probably send a request
	errorstream << "SSCSMEnvironment::setFatalError() reason: " << reason << std::endl;
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
