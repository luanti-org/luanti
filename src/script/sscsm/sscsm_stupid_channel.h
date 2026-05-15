// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>
#include "sscsm_irequest.h"

// FIXME: replace this with an ipc channel
class StupidChannel
{
	std::mutex m_mutex;
	std::condition_variable m_condvar;
	SerializedSSCSMRequest m_request;
	SerializedSSCSMAnswer m_answer;

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

		while (!m_answer) {
			m_condvar.wait(lock);
		}

		auto answer = std::move(m_answer);
		m_answer = nullptr;

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

		while (!m_request) {
			m_condvar.wait(lock);
		}

		auto request = std::move(m_request);
		m_request = nullptr;

		return request;
	}

	SerializedSSCSMRequest exchangeB(SerializedSSCSMAnswer answer)
	{
		sendB(std::move(answer));

		return recvB();
	}
};
