// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_environment.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "sscsm_stupid_channel.h"
#include "client/mod_vfs.h"
#include "common/c_types.h" // LuaError


SSCSMEnvironment::SSCSMEnvironment(std::shared_ptr<StupidChannel> channel) :
	Thread("SSCSMEnvironment-thread"),
	m_channel(std::move(channel)),
	m_script(std::make_unique<SSCSMScripting>(this)),
	m_vfs(std::make_unique<ModVFS>())
{
}

SSCSMEnvironment::~SSCSMEnvironment() = default;

void *SSCSMEnvironment::run()
{
	while (true) {
		auto next_event = [&]{
			auto request = SSCSMRequestPollNextEvent{};
			auto answer = doRequest(std::move(request));
			return std::move(answer.next_event);
		}();

		if (dynamic_cast<SSCSMEventTearDown *>(next_event.get())) {
			break;
		}

		try {
			next_event->exec(this);
		} catch (LuaError &e) {
			setFatalError(std::string("Lua error: ") + e.what());
		} catch (ModError &e) {
			setFatalError(std::string("Mod error: ") + e.what());
		}
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
		m_vfs->m_vfs.emplace(std::move(p.first), std::move(p.second));
	}
}

std::optional<std::string_view> SSCSMEnvironment::readVFSFile(const std::string &path)
{
	auto it = m_vfs->m_vfs.find(path);
	if (it == m_vfs->m_vfs.end())
		return std::nullopt;
	else
		return it->second;
}

void SSCSMEnvironment::setFatalError(const std::string &reason)
{
	auto request = SSCSMRequestSetFatalError{};
	request.reason = reason;
	doRequest(std::move(request));
}
