/*
Minetest
Copyright (C) 2022 Desour <vorunbekannt75@web.de>
Copyright (C) 2022 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "irrlichttypes.h"
#include <string>
#include <type_traits>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <atomic>
#else
#include <pthread.h>
#endif

/*
	An IPC channel is used for synchronous communication between two processes.
	Sending two messages in succession from one end is not allowed; messages
	must alternate back and forth.

	IPCChannelShared is situated in shared memory and is used by both ends of
	the channel.
*/

#define IPC_CHANNEL_MSG_SIZE 8192U

struct IPCChannelBuffer
{
#if !defined(_WIN32)
#if defined(__linux__)
	std::atomic<u32> futex = ATOMIC_VAR_INIT(0U);
#else
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	// TODO: use atomic?
	bool posted = false;
#endif
#endif // !defined(_WIN32)
	size_t size;
	u8 data[IPC_CHANNEL_MSG_SIZE];

	IPCChannelBuffer();
	~IPCChannelBuffer();
};

struct IPCChannelShared
{
	IPCChannelBuffer a;
	IPCChannelBuffer b;
};

class IPCChannelEnd
{
public:
	IPCChannelEnd() = default;

#if defined(_WIN32)
	static IPCChannelEnd makeA(IPCChannelShared *shared, HANDLE sem_a, HANDLE sem_b)
	{
		return IPCChannelEnd(&shared->a, &shared->b, sem_a, sem_b);
	}

	static IPCChannelEnd makeB(IPCChannelShared *shared, HANDLE sem_a, HANDLE sem_b)
	{
		return IPCChannelEnd(&shared->b, &shared->a, sem_b, sem_a);
	}
#else
	static IPCChannelEnd makeA(IPCChannelShared *shared)
	{
		return IPCChannelEnd(&shared->a, &shared->b);
	}

	static IPCChannelEnd makeB(IPCChannelShared *shared)
	{
		return IPCChannelEnd(&shared->b, &shared->a);
	}
#endif // !defined(_WIN32)

	// If send, recv, or exchange return false, stop using the channel.
	// Note: timeouts may be for receiving any response, not a whole message.

	bool send(const void *data, size_t size, int timeout_ms = -1) noexcept
	{
		if (size <= IPC_CHANNEL_MSG_SIZE) {
			return sendSmall(data, size);
		} else {
			return sendLarge(data, size, timeout_ms);
		}
	}

	bool recv(int timeout_ms = -1) noexcept;

	bool exchange(const void *data, size_t size, int timeout_ms = -1) noexcept
	{
		return send(data, size, timeout_ms) && recv(timeout_ms);
	}

	// Get information about the last received message
	inline const void *getRecvData() const noexcept { return m_recv_data; }
	inline size_t getRecvSize() const noexcept { return m_recv_size; }

private:
#if defined(_WIN32)
	IPCChannelEnd(IPCChannelBuffer *in, IPCChannelBuffer *out, HANDLE sem_in, HANDLE sem_out):
			m_in(in), m_out(out), m_sem_in(sem_in), m_sem_out(sem_out)
	{}
#else
	IPCChannelEnd(IPCChannelBuffer *in, IPCChannelBuffer *out): m_in(in), m_out(out) {}
#endif

	bool sendSmall(const void *data, size_t size) noexcept;

	bool sendLarge(const void *data, size_t size, int timeout_ms) noexcept;

	IPCChannelBuffer *m_in = nullptr;
	IPCChannelBuffer *m_out = nullptr;
#if defined(_WIN32)
	HANDLE m_sem_in;
	HANDLE m_sem_out;
#endif
	const void *m_recv_data;
	size_t m_recv_size;
	std::vector<u8> m_large_recv;
};
