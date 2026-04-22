// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_environment.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "client/mod_vfs.h"
#include "common/c_types.h" // LuaError
#include "log.h"
#include "porting.h"

#include <algorithm>
#include <cstdlib>
#include <string>

// Timeout for channel operations in the worker. If the main process is
// unresponsive for this long, we give up and exit. Matches the worker stub.
static constexpr int SSCSM_CHANNEL_TIMEOUT_MS = 2 * 60 * 1000;


SSCSMEnvironment::SSCSMEnvironment(IPCChannelEnd channel) :
	m_channel(std::move(channel)),
	m_script(std::make_unique<SSCSMScripting>(this)),
	m_vfs(std::make_unique<ModVFS>())
{
}

SSCSMEnvironment::~SSCSMEnvironment() = default;

void SSCSMEnvironment::run()
{
	// Fault-injection: LUANTI_SSCSM_DIE_ON_EVENT=N crashes the worker while
	// processing the Nth event (1-based). Tests the mid-session death path.
	int die_on_event = 0;
	int event_count = 0;
	if (const char *s = std::getenv("LUANTI_SSCSM_DIE_ON_EVENT"))
		die_on_event = std::atoi(s);

	// Fault-injection: LUANTI_SSCSM_FLOOD_REQUESTS=N spams N
	// DisplayChatMessage requests during the first event's execution.
	// Tests the parent's throughput and any DoS vulnerability via a
	// runaway clientmod (e.g. tight core.display_chat_message loop).
	int flood_n = 0;
	bool flood_done = false;
	if (const char *s = std::getenv("LUANTI_SSCSM_FLOOD_REQUESTS"))
		flood_n = std::atoi(s);

	while (true) {
		auto next_event = [&]{
			auto request = SSCSMRequestPollNextEvent{};
			auto answer = doRequest(std::move(request));
			return std::move(answer.next_event);
		}();

		if (next_event->getType() == SSCSMEventType::TearDown) {
			break;
		}

		if (die_on_event > 0 && ++event_count == die_on_event) {
			warningstream << "sscsm-worker: DIE_ON_EVENT=" << die_on_event
					<< " — calling _Exit(12)" << std::endl;
			std::_Exit(12);
		}

		// Fault-injection: LUANTI_SSCSM_FATAL_ERROR=1 triggers the
		// SetFatalError request path on the first event, exercising
		// the controller's "worker says it's broken" handling.
		if (std::getenv("LUANTI_SSCSM_FATAL_ERROR")) {
			warningstream << "sscsm-worker: FATAL_ERROR — sending "
					"SSCSMRequestSetFatalError" << std::endl;
			setFatalError("LUANTI_SSCSM_FATAL_ERROR injected");
			std::_Exit(13);
		}

		try {
			next_event->exec(this);
		} catch (LuaError &e) {
			setFatalError(std::string("Lua error: ") + e.what());
		} catch (ModError &e) {
			setFatalError(std::string("Mod error: ") + e.what());
		}

		// Flood fires on the first in-game OnStep so we exercise the
		// per-event budget under realistic frame-loop conditions, not
		// during loading-screen init where extra latency is masked.
		if (!flood_done && flood_n > 0 &&
				next_event->getType() == SSCSMEventType::OnStep) {
			flood_done = true;
			warningstream << "sscsm-worker: FLOOD_REQUESTS=" << flood_n
					<< " — spamming chat messages on first OnStep"
					<< std::endl;
			u64 t0 = porting::getTimeMs();
			for (int i = 0; i < flood_n; ++i) {
				auto req = SSCSMRequestDisplayChatMessage{};
				req.text = "flood " + std::to_string(i);
				doRequest(std::move(req));
			}
			u64 t1 = porting::getTimeMs();
			warningstream << "sscsm-worker: FLOOD_REQUESTS done in "
					<< (t1 - t0) << " ms ("
					<< (flood_n * 1000 / std::max<u64>(1, t1 - t0))
					<< " req/s)" << std::endl;
		}
	}
}

SerializedSSCSMAnswer SSCSMEnvironment::exchange(SerializedSSCSMRequest req)
{
	// Fault-injection: LUANTI_SSCSM_GARBAGE_REQUEST=N replaces the Nth
	// outgoing request (1-based) with a single 0xFF byte — an unknown
	// request type tag — to exercise the parent's deserializer.
	static int garbage_at = []{
		if (const char *s = std::getenv("LUANTI_SSCSM_GARBAGE_REQUEST"))
			return std::atoi(s);
		return 0;
	}();
	static int request_count = 0;
	++request_count;
	if (garbage_at > 0 && request_count == garbage_at) {
		warningstream << "sscsm-worker: GARBAGE_REQUEST=" << garbage_at
				<< " — sending 1 byte of 0xFF instead of request #"
				<< request_count << std::endl;
		char garbage = '\xff';
		if (!m_channel.exchangeWithTimeout(&garbage, 1,
				SSCSM_CHANNEL_TIMEOUT_MS)) {
			std::exit(4);
		}
		return SerializedSSCSMAnswer(
				static_cast<const char *>(m_channel.getRecvData()),
				m_channel.getRecvSize());
	}

	if (!m_channel.exchangeWithTimeout(req.data(), req.size(),
			SSCSM_CHANNEL_TIMEOUT_MS)) {
		// Main process unresponsive — nothing we can do. Exit worker.
		std::exit(4);
	}
	return SerializedSSCSMAnswer(
			static_cast<const char *>(m_channel.getRecvData()),
			m_channel.getRecvSize());
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
