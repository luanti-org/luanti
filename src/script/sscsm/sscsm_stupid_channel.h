// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include "sscsm_irequest.h"

// In-process channel between the SSCSMController (main thread) and the
// SSCSMEnvironment thread. To be replaced by an IPCChannel from
// src/threading/ipc_channel.h once SSCSM runs in its own process.
class StupidChannel
{
	std::mutex m_mutex;
	std::condition_variable m_condvar;
	std::optional<SerializedSSCSMRequest> m_request;
	std::optional<SerializedSSCSMAnswer> m_answer;

public:
	void sendA(SerializedSSCSMRequest request)
	{
		{
			auto lock = std::lock_guard(m_mutex);

			m_request = std::move(request);
		}

		m_condvar.notify_one();
	}

	SerializedSSCSMAnswer recvA()
	{
		auto lock = std::unique_lock(m_mutex);

		while (!m_answer.has_value()) {
			m_condvar.wait(lock);
		}

		auto answer = std::move(*m_answer);
		m_answer.reset();

		return answer;
	}

	SerializedSSCSMAnswer exchangeA(SerializedSSCSMRequest request)
	{
		sendA(std::move(request));

		return recvA();
	}

	void sendB(SerializedSSCSMAnswer answer)
	{
		{
			auto lock = std::lock_guard(m_mutex);

			m_answer = std::move(answer);
		}

		m_condvar.notify_one();
	}

	SerializedSSCSMRequest recvB()
	{
		auto lock = std::unique_lock(m_mutex);

		while (!m_request.has_value()) {
			m_condvar.wait(lock);
		}

		auto request = std::move(*m_request);
		m_request.reset();

		return request;
	}

	SerializedSSCSMRequest exchangeB(SerializedSSCSMAnswer answer)
	{
		sendB(std::move(answer));

		return recvB();
	}
};
