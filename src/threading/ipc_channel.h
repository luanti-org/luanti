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
#define IPC_CHANNEL_IMPLEMENTATION_WIN32
#elif defined(__linux__)
#define IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX
#else
#define IPC_CHANNEL_IMPLEMENTATION_POSIX
#endif

#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
#include <windows.h>
#elif defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)
#include <atomic>
#elif defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
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

#define IPC_CHANNEL_MSG_SIZE 0x2000U

struct IPCChannelBuffer
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)
	// possible values:
	// 0: futex is not posted. reader will check value before blocking => no
	//    notify needed when posting
	// 1: futex is posted
	// 2: futex is not posted. reader is waiting with futex syscall, and needs
	//    to be notified
	std::atomic<u32> futex{0};

#elif defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	bool posted = false; // protected by mutex
#endif

	// Note: If the other side isn't acting cooperatively, they might write to
	// this at any times. So we must make sure to copy out the data once, and
	// only access that copy.
	size_t size = 0;
	u8 data[IPC_CHANNEL_MSG_SIZE] = {};

	IPCChannelBuffer();
	~IPCChannelBuffer(); // Note: only destruct once, i.e. in one process
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
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
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
	inline const void *getRecvData() const noexcept { return m_large_recv.data(); }
	inline size_t getRecvSize() const noexcept { return m_recv_size; }

private:
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
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
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	HANDLE m_sem_in;
	HANDLE m_sem_out;
#endif
	size_t m_recv_size = 0;
	// we always copy from the shared buffer into this
	// (this buffer only grows)
	std::vector<u8> m_large_recv = std::vector<u8>(IPC_CHANNEL_MSG_SIZE);
};
