// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_controller.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "log.h"
#include "threading/ipc_child_process.h"

// Timeout for channel operations on the main-process side. No legitimate
// event should take more than a few seconds; a stuck worker past this is
// either crashed or wedged in a Lua infinite loop — either way, nothing
// useful is coming back. A longer timeout just means a longer visible
// stall on the loading screen.
static constexpr int SSCSM_CHANNEL_TIMEOUT_MS = 5 * 1000;

// Timeout for the initial handshake. Same reasoning as above.
static constexpr int SSCSM_HANDSHAKE_TIMEOUT_MS = 5 * 1000;

// Timeout for sending the TearDown message in ~SSCSMController. If the
// worker is dead this would hang shutdown; we want a quick give-up.
static constexpr int SSCSM_TEARDOWN_TIMEOUT_MS = 1 * 1000;

static SerializedSSCSMAnswer receive_one(IPCChannelEnd &ch)
{
	return SerializedSSCSMAnswer(
			static_cast<const char *>(ch.getRecvData()),
			ch.getRecvSize());
}

std::unique_ptr<SSCSMController> SSCSMController::create()
{
	auto [end_a, process] = make_child_ipc_channel();
	if (!process || !process->isValid()) {
		errorstream << "SSCSM: failed to spawn worker process" << std::endl;
		return nullptr;
	}

	// Wait for the worker to send its initial PollNextEvent request.
	if (!end_a.recvWithTimeout(SSCSM_HANDSHAKE_TIMEOUT_MS)) {
		errorstream << "SSCSM: worker did not respond within "
				<< SSCSM_HANDSHAKE_TIMEOUT_MS << "ms; giving up" << std::endl;
		process->terminate();
		return nullptr;
	}
	auto req0 = deserializeSSCSMRequest(receive_one(end_a));
	if (req0->getType() != SSCSMRequestType::PollNextEvent) {
		errorstream << "SSCSM: worker first request must be PollNextEvent"
				<< std::endl;
		process->terminate();
		return nullptr;
	}

	return std::make_unique<SSCSMController>(std::move(end_a), std::move(process));
}

SSCSMController::SSCSMController(IPCChannelEnd channel,
		std::unique_ptr<IPCChildProcess> process) :
	m_channel(std::move(channel)), m_process(std::move(process))
{
}

SSCSMController::~SSCSMController()
{
	// Skip teardown send if the worker is already gone — otherwise we'd
	// block for SSCSM_TEARDOWN_TIMEOUT_MS on every shutdown after a
	// worker crash.
	if (!m_dead && !checkWorkerDead()) {
		// Send tear-down event as the answer to the worker's pending PollNextEvent.
		auto answer0 = SSCSMRequestPollNextEvent::Answer{};
		answer0.next_event = std::make_unique<SSCSMEventTearDown>();
		auto answer = serializeSSCSMAnswer(std::move(answer0));
		(void)m_channel.sendWithTimeout(answer.data(), answer.size(),
				SSCSM_TEARDOWN_TIMEOUT_MS);
	}

	// Wait for the worker to exit cleanly; m_process's destructor
	// will escalate to termination if it doesn't.
}

bool SSCSMController::checkWorkerDead()
{
	if (m_dead)
		return true;
	if (!m_process || !m_process->isValid()) {
		m_dead = true;
		return true;
	}
	// Non-blocking poll: waitpid(WNOHANG) via a tiny timeout. If the
	// worker has already exited, this returns true immediately.
	if (m_process->waitWithTimeout(0)) {
		warningstream << "SSCSM: worker has exited (code "
				<< m_process->getExitCode() << ")" << std::endl;
		m_dead = true;
		return true;
	}
	return false;
}

SerializedSSCSMAnswer SSCSMController::handleRequest(Client *client, ISSCSMRequest *req)
{
	return req->exec(client);
}

void SSCSMController::runEvent(Client *client, std::unique_ptr<ISSCSMEvent> event)
{
	// Fast-path: worker is already known dead; don't bother serializing
	// and waiting SSCSM_CHANNEL_TIMEOUT_MS to rediscover that.
	if (checkWorkerDead())
		return;

	auto answer0 = SSCSMRequestPollNextEvent::Answer{};
	answer0.next_event = std::move(event);
	auto answer = serializeSSCSMAnswer(std::move(answer0));

	while (true) {
		if (!m_channel.exchangeWithTimeout(answer.data(), answer.size(),
				SSCSM_CHANNEL_TIMEOUT_MS)) {
			// Timeout. Distinguish "worker exited silently" (common,
			// expected after a crash) from "worker wedged" (rare).
			if (checkWorkerDead()) {
				errorstream << "SSCSM: worker died during runEvent; "
						"clientmods now disabled" << std::endl;
			} else {
				errorstream << "SSCSM: worker channel timed out after "
						<< SSCSM_CHANNEL_TIMEOUT_MS
						<< "ms with worker still alive; disabling SSCSM"
						<< std::endl;
				m_dead = true;
			}
			return;
		}
		auto request = deserializeSSCSMRequest(receive_one(m_channel));

		// SSCSMRequestPollNextEvent means `event` is finished and we need to
		// answer with the next event (that will be passed in a subsequent runEvent()
		// call).
		if (request->getType() == SSCSMRequestType::PollNextEvent) {
			break;
		}

		answer = handleRequest(client, request.get());
	}
}
