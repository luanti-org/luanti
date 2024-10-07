/*
Minetest
Copyright (C) 2022 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>
Copyright (C) 2024 DS

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
#include "util/basic_macros.h"
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>

#if defined(_WIN32)
#define IPC_CHANNEL_IMPLEMENTATION_WIN32
#elif defined(__linux__)
#define IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX
#else
#define IPC_CHANNEL_IMPLEMENTATION_POSIX
#endif

#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
#include <windows.h>
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

constexpr size_t IPC_CHANNEL_MSG_SIZE = 0x2000;

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
	DISABLE_CLASS_COPY(IPCChannelBuffer)
	~IPCChannelBuffer(); // Note: only destruct once, i.e. in one process
};

// Data in shared memory
struct IPCChannelShared
{
	// Both ends unmap, but last deleter also deletes shared resources.
	std::atomic<u32> refcount{1};

	IPCChannelBuffer a{};
	IPCChannelBuffer b{};
};

// Interface for managing the shared resources.
// Implementors decide whether to use malloc or mmap.
struct IPCChannelResources
{
	// new struct, because the win32 #if is annoying
	struct Data
	{
		IPCChannelShared *shared = nullptr;

#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
		HANDLE sem_a;
		HANDLE sem_b;
#endif
	};

	Data data;

	IPCChannelResources() = default;
	DISABLE_CLASS_COPY(IPCChannelResources)

	// Child should call cleanup().
	// (Parent destructor can not do this, because when it's called the child is
	// already dead.)
	virtual ~IPCChannelResources() = default;

protected:
	// Used for previously unmanaged data_ (move semantics)
	void setFirst(Data data_)
	{
		data = data_;
	}

	// Used for data_ that is already managed by an IPCChannelResources (grab()
	// semantics)
	bool setSecond(Data data_)
	{
		if (data_.shared->refcount.fetch_add(1) == 0) {
			// other end dead, can't use resources
			data_.shared->refcount.fetch_sub(1);
			return false;
		}
		data = data_;
		return true;
	}

	virtual void cleanupLast() noexcept = 0;
	virtual void cleanupNotLast() noexcept = 0;

	void cleanup() noexcept
	{
		if (!data.shared) {
			// No owned resources. Maybe setSecond failed.
			return;
		}
		if (data.shared->refcount.fetch_sub(1) == 1) {
			// We are last, we clean up.
			cleanupLast();
		} else {
			// We are not responsible for cleanup.
			// Note: Shared resources may already be invalid by now.
			cleanupNotLast();
		}
	}
};

class IPCChannelEnd
{
public:
	// Direction. References into IPCChannelResources.
	struct Dir
	{
		IPCChannelBuffer *buf_in = nullptr;
		IPCChannelBuffer *buf_out = nullptr;
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
		HANDLE sem_in;
		HANDLE sem_out;
#endif
	};

	// Unusable empty end
	IPCChannelEnd() = default;

	// Construct end A or end B from resources
	static IPCChannelEnd makeA(std::unique_ptr<IPCChannelResources> resources);
	static IPCChannelEnd makeB(std::unique_ptr<IPCChannelResources> resources);

	// Note: Timeouts may be for receiving any response, not a whole message.
	// Therefore, if a timeout occurs, stop using the channel.

	// Returns false on timeout
	[[nodiscard]]
	bool sendWithTimeout(const void *data, size_t size, int timeout_ms) noexcept
	{
		if (size <= IPC_CHANNEL_MSG_SIZE) {
			sendSmall(data, size);
			return true;
		} else {
			return sendLarge(data, size, timeout_ms);
		}
	}

	// Same as above
	void send(const void *data, size_t size) noexcept
	{
		(void)sendWithTimeout(data, size, -1);
	}

	// Returns false on timeout.
	// Otherwise returns true, and data is available via getRecvData().
	[[nodiscard]]
	bool recvWithTimeout(int timeout_ms) noexcept;

	// Same as above
	void recv() noexcept
	{
		(void)recvWithTimeout(-1);
	}

	// Returns false on timeout
	// Otherwise returns true, and data is available via getRecvData().
	[[nodiscard]]
	bool exchangeWithTimeout(const void *data, size_t size, int timeout_ms) noexcept
	{
		return sendWithTimeout(data, size, timeout_ms)
				&& recvWithTimeout(timeout_ms);
	}

	// Same as above
	void exchange(const void *data, size_t size) noexcept
	{
		(void)exchangeWithTimeout(data, size, -1);
	}

	// Get the content of the last received message
	const void *getRecvData() const noexcept { return m_large_recv.data(); }
	size_t getRecvSize() const noexcept { return m_recv_size; }

private:
	IPCChannelEnd(std::unique_ptr<IPCChannelResources> resources, Dir dir) :
		m_resources(std::move(resources)), m_dir(dir)
	{}

	void sendSmall(const void *data, size_t size) noexcept;

	// returns false on timeout
	bool sendLarge(const void *data, size_t size, int timeout_ms) noexcept;

	std::unique_ptr<IPCChannelResources> m_resources;
	Dir m_dir;
	size_t m_recv_size = 0;
	// we always copy from the shared buffer into this
	// (this buffer only grows)
	std::vector<u8> m_large_recv = std::vector<u8>(IPC_CHANNEL_MSG_SIZE);
};

// For testing purposes
struct IPCChannelResourcesSingleProcess final : public IPCChannelResources
{
	void cleanupLast() noexcept override
	{
		delete data.shared;
#ifdef IPC_CHANNEL_IMPLEMENTATION_WIN32
		CloseHandle(data.sem_b);
		CloseHandle(data.sem_a);
#endif
	}

	void cleanupNotLast() noexcept override
	{
		// nothing to do (i.e. no unmapping needed)
	}

	~IPCChannelResourcesSingleProcess() override { cleanup(); }

	static std::unique_ptr<IPCChannelResourcesSingleProcess> makeFirst(Data data)
	{
		auto ret = std::make_unique<IPCChannelResourcesSingleProcess>();
		ret->setFirst(data);
		return ret;
	}

	static std::unique_ptr<IPCChannelResourcesSingleProcess> makeSecond(Data data)
	{
		auto ret = std::make_unique<IPCChannelResourcesSingleProcess>();
		ret->setSecond(data);
		return ret;
	}
};

// For testing
// Returns one end and a thread holding the other end. The thread will execute
// fun, and pass it the other end.
std::pair<IPCChannelEnd, std::thread> make_test_ipc_channel(
		const std::function<void(IPCChannelEnd)> &fun);
