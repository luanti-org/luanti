// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_controller.h"
#include "sscsm_environment.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "sscsm_stupid_channel.h"

std::unique_ptr<SSCSMController> SSCSMController::create()
{
	auto channel = std::make_shared<StupidChannel>();
	auto thread = std::make_unique<SSCSMEnvironment>(channel);
	thread->start();

	// Wait for thread to finish initializing.
	auto req0 = deserializeSSCSMRequest(channel->recvB());
	FATAL_ERROR_IF(!dynamic_cast<SSCSMRequestPollNextEvent *>(req0.get()),
			"First request must be pollEvent.");

	return std::make_unique<SSCSMController>(std::move(thread), channel);
}

SSCSMController::SSCSMController(std::unique_ptr<SSCSMEnvironment> thread,
		std::shared_ptr<StupidChannel> channel) :
	m_thread(std::move(thread)), m_channel(std::move(channel))
{
}

SSCSMController::~SSCSMController()
{
	// send tear-down
	auto answer = SSCSMRequestPollNextEvent::Answer{};
	answer.next_event = std::make_unique<SSCSMEventTearDown>();
	m_channel->sendB(serializeSSCSMAnswer(std::move(answer)));
	// wait for death
	m_thread->stop();
	m_thread->wait();
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
		auto request = deserializeSSCSMRequest(m_channel->exchangeB(std::move(answer)));

		// SSCSMRequestPollNextEvent means `event` is finished and we need to
		// answer with the next event (that will be passed in a subsequent runEvent()
		// call)
		if (dynamic_cast<SSCSMRequestPollNextEvent *>(request.get()) != nullptr) {
			break;
		}

		answer = handleRequest(client, request.get());
	}
}
