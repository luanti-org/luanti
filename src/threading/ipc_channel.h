/*
Minetest
Copyright (C) 2022 DS
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
#include <memory>
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

	There are currently 3 implementations for synchronisation:
	* win32: uses win32 semaphore
	* linux: uses futex, and does busy waiting if on x86/x86_64
	* other posix: uses posix mutex and condition variable
*/

#define IPC_CHANNEL_MSG_SIZE 8192U

struct IPCChannelBuffer
{
#if !defined(_WIN32)
#if defined(__linux__)
	// possible values:
	// 0: futex is not posted. reader will check value before blocking => no
	//    notify needed when posting
	// 1: futex is posted
	// 2: futex is not posted. reader is waiting with futex syscall, and needs
	//    to be notified
	std::atomic<u32> futex{0};
#else
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	bool posted = false; // protected by mutex
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

// opaque owner for the shared mem and stuff
// users have to implement this
struct IPCChannelStuff
{
	virtual ~IPCChannelStuff() = default;
	virtual IPCChannelShared *getShared() = 0;
#ifdef _WIN32
	virtual HANDLE getSemA() = 0;
	virtual HANDLE getSemB() = 0;
#endif
};

class IPCChannelEnd
{
public:
	IPCChannelEnd() = default;

	static IPCChannelEnd makeA(std::unique_ptr<IPCChannelStuff> stuff);
	static IPCChannelEnd makeB(std::unique_ptr<IPCChannelStuff> stuff);

	// If send, recv, or exchange return false (=timeout), stop using the channel.
	// Note: timeouts may be for receiving any response, not a whole message.

	bool send(const void *data, size_t size, int timeout_ms = -1) noexcept
	{
		if (size <= IPC_CHANNEL_MSG_SIZE) {
			sendSmall(data, size);
			return true;
		} else {
			return sendLarge(data, size, timeout_ms);
		}
	}

	bool recv(int timeout_ms = -1) noexcept;

	bool exchange(const void *data, size_t size, int timeout_ms = -1) noexcept
	{
		return send(data, size, timeout_ms) && recv(timeout_ms);
	}

	// Get the content of the last received message
	inline const void *getRecvData() const noexcept { return m_recv_data; }
	inline size_t getRecvSize() const noexcept { return m_recv_size; }

private:
#if defined(_WIN32)
	IPCChannelEnd(
			std::unique_ptr<IPCChannelStuff> stuff,
			IPCChannelBuffer *in, IPCChannelBuffer *out,
			HANDLE sem_in, HANDLE sem_out) :
		m_stuff(std::move(stuff)),
		m_in(in), m_out(out),
		m_sem_in(sem_in), m_sem_out(sem_out)
	{}
#else
	IPCChannelEnd(
			std::unique_ptr<IPCChannelStuff> stuff,
			IPCChannelBuffer *in, IPCChannelBuffer *out) :
		m_stuff(std::move(stuff)),
		m_in(in), m_out(out)
	{}
#endif

	void sendSmall(const void *data, size_t size) noexcept;

	// returns false on timeout
	bool sendLarge(const void *data, size_t size, int timeout_ms) noexcept;

	std::unique_ptr<IPCChannelStuff> m_stuff;
	IPCChannelBuffer *m_in = nullptr;
	IPCChannelBuffer *m_out = nullptr;
#if defined(_WIN32)
	HANDLE m_sem_in;
	HANDLE m_sem_out;
#endif
	const void *m_recv_data = nullptr;
	size_t m_recv_size = 0;
	std::vector<u8> m_large_recv;
};
