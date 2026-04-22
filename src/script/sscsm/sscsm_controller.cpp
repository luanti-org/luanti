// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_controller.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "log.h"
#include "threading/ipc_child_process.h"

// Timeout for channel operations on the main-process side. If the worker
// doesn't respond within this long, we treat the channel as broken.
static constexpr int SSCSM_CHANNEL_TIMEOUT_MS = 2 * 60 * 1000;

// Shorter timeout for the initial handshake. A worker that can't send its
// first PollNextEvent within a few seconds is broken (crashed on startup,
// sandboxed away, wrong binary); no point blocking client init for minutes.
static constexpr int SSCSM_HANDSHAKE_TIMEOUT_MS = 5 * 1000;

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
	// Send tear-down event as the answer to the worker's pending PollNextEvent.
	auto answer0 = SSCSMRequestPollNextEvent::Answer{};
	answer0.next_event = std::make_unique<SSCSMEventTearDown>();
	auto answer = serializeSSCSMAnswer(std::move(answer0));
	(void)m_channel.sendWithTimeout(answer.data(), answer.size(),
			SSCSM_CHANNEL_TIMEOUT_MS);

	// Wait for the worker to exit cleanly; m_process's destructor
	// will escalate to termination if it doesn't.
}

SerializedSSCSMAnswer SSCSMController::handleRequest(Client *client, ISSCSMRequest *req)
{
	return req->exec(client);
}

void SSCSMController::runEvent(Client *client, std::unique_ptr<ISSCSMEvent> event)
{
	auto answer0 = SSCSMRequestPollNextEvent::Answer{};
	answer0.next_event = std::move(event);
	auto answer = serializeSSCSMAnswer(std::move(answer0));

	while (true) {
		if (!m_channel.exchangeWithTimeout(answer.data(), answer.size(),
				SSCSM_CHANNEL_TIMEOUT_MS)) {
			errorstream << "SSCSM: worker channel timed out, assuming dead"
					<< std::endl;
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
