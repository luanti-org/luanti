// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <chrono>

namespace sound {

// The owner serializes access when multiple sound managers share a device.
class DeviceReconnect
{
public:
	using Clock = std::chrono::steady_clock;
	enum class Result { Connected, Lost, Waiting, RetryFailed, Recovered };

	template <typename Reopen>
	Result poll(Clock::time_point now, bool connected, Reopen &&reopen)
	{
		if (connected) {
			const bool was_disconnected = m_disconnected;
			m_disconnected = false;
			return was_disconnected ? Result::Recovered : Result::Connected;
		}
		if (!m_disconnected) {
			m_disconnected = true;
			m_next_retry = now;
			return Result::Lost;
		}
		if (now < m_next_retry)
			return Result::Waiting;

		if (reopen()) {
			m_disconnected = false;
			return Result::Recovered;
		}
		m_next_retry = now + std::chrono::seconds(1);
		return Result::RetryFailed;
	}

private:
	bool m_disconnected = false;
	Clock::time_point m_next_retry{};
};

} // namespace sound
