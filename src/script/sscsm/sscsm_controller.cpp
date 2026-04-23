// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_controller.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "log.h"
#include "porting.h"
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

// Per-event time budgets. A single event-handler invocation in the
// worker must not be allowed to monopolize the parent's main thread —
// this is the parent's only line of defense against a malicious or
// buggy clientmod that, e.g., sits in a tight
// `core.display_chat_message` loop. Budget exceeded => disable SSCSM,
// kill the worker.
//
// Time-based (not request-count-based) so the protection holds
// equivalently on a fast desktop and a slow phone: the bound is on
// perceived freeze time, not on request volume.
//
// Two budgets:
//   * OnStep runs every frame and *is* the frame budget; we cap it
//     tight to keep stutters visible-but-survivable (~6 dropped
//     frames at 60 Hz).
//   * The other events (UpdateVFSFiles, LoadMods, UpdateContentDefs)
//     run during loading where some delay is expected and clientmod
//     init may legitimately take longer; we're more generous.
static constexpr u64 SSCSM_BUDGET_ONSTEP_MS = 100;
static constexpr u64 SSCSM_BUDGET_OTHER_MS = 2000;

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
		// Distinguish "worker crashed on startup" (exit code visible)
		// from "worker is still alive but unresponsive".
		if (process->waitWithTimeout(0)) {
			errorstream << "SSCSM: worker exited during handshake (code "
					<< process->getExitCode() << ")" << std::endl;
		} else {
			errorstream << "SSCSM: worker did not respond within "
					<< SSCSM_HANDSHAKE_TIMEOUT_MS << "ms; giving up"
					<< std::endl;
		}
		process->terminate();
		return nullptr;
	}

	std::unique_ptr<ISSCSMRequest> req0;
	try {
		req0 = deserializeSSCSMRequest(receive_one(end_a));
	} catch (std::exception &e) {
		errorstream << "SSCSM: worker sent malformed first request ("
				<< e.what() << "); giving up" << std::endl;
		process->terminate();
		return nullptr;
	}
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
	// A healthy worker exits in milliseconds after TearDown; a sick
	// worker gets SIGTERM from our own code below. Either way there's
	// no reason to let the IPCChildProcess default (2 min) gate
	// shutdown.
	if (m_process)
		m_process->setDestructorTimeoutMs(2000);
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

	const SSCSMEventType event_type = event->getType();

	auto answer0 = SSCSMRequestPollNextEvent::Answer{};
	answer0.next_event = std::move(event);
	auto answer = serializeSSCSMAnswer(std::move(answer0));

	const u64 budget_ms = (event_type == SSCSMEventType::OnStep)
			? SSCSM_BUDGET_ONSTEP_MS
			: SSCSM_BUDGET_OTHER_MS;
	const u64 deadline = porting::getTimeMs() + budget_ms;
	int request_count = 0;

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
				// Worker is alive but not responding — kill it so we
				// don't stall shutdown on an unresponsive orphan.
				if (m_process && m_process->isValid())
					m_process->terminate();
			}
			return;
		}
		std::unique_ptr<ISSCSMRequest> request;
		try {
			request = deserializeSSCSMRequest(receive_one(m_channel));
		} catch (std::exception &e) {
			errorstream << "SSCSM: malformed request from worker ("
					<< e.what() << "); disabling SSCSM" << std::endl;
			m_dead = true;
			// Worker is wedged waiting for our reply to its bad request.
			// Kill it now so shutdown doesn't stall on the orphan.
			if (m_process && m_process->isValid())
				m_process->terminate();
			return;
		}

		// SSCSMRequestPollNextEvent means `event` is finished and we need to
		// answer with the next event (that will be passed in a subsequent runEvent()
		// call).
		if (request->getType() == SSCSMRequestType::PollNextEvent) {
			break;
		}

		// Enforce per-event time budget. A clientmod that issues an
		// unbounded number of requests in one handler would otherwise
		// freeze the main game thread for as long as it keeps sending
		// — confirmed in pen-testing: 10M chat messages = 28 s of
		// frozen UI before any limit was in place.
		++request_count;
		if (porting::getTimeMs() > deadline) {
			errorstream << "SSCSM: worker exceeded " << budget_ms
					<< "ms budget in a single event ("
					<< request_count << " requests so far); disabling SSCSM"
					<< std::endl;
			m_dead = true;
			if (m_process && m_process->isValid())
				m_process->terminate();
			return;
		}

		// SetFatalError is the worker's "I'm broken, give up on me" signal —
		// raised on Lua/Mod errors in event execution. Policy is to keep
		// the player connected (matches the worker-crash recovery path)
		// and just disable further SSCSM activity. Killing the worker
		// here also unblocks shutdown since it's wedged waiting for us.
		if (request->getType() == SSCSMRequestType::SetFatalError) {
			auto *fe = static_cast<SSCSMRequestSetFatalError *>(request.get());
			errorstream << "SSCSM: worker reported fatal error: "
					<< fe->reason << "; disabling SSCSM" << std::endl;
			m_dead = true;
			if (m_process && m_process->isValid())
				m_process->terminate();
			return;
		}

		answer = handleRequest(client, request.get());
	}
}
