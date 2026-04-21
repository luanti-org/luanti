// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "util/basic_macros.h"
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>

/*
	An IPC channel is used for synchronous communication between two processes.
	Sending two messages in succession from one end is not allowed; messages
	must alternate back and forth.

	IPCChannelShared is situated in shared memory and is used by both ends of
	the channel.

	Synchronisation uses a portable spin-lock on std::atomic<u32>.
*/

constexpr size_t IPC_CHANNEL_MSG_SIZE = 0x2000;

struct IPCChannelBuffer
{
	// 0 = not posted, 1 = posted.
	std::atomic<u32> flag{0};

	// Note: If the other side isn't acting cooperatively, they might write to
	// this at any times. So we must make sure to copy out the data once, and
	// only access that copy.
	size_t size = 0;
	u8 data[IPC_CHANNEL_MSG_SIZE] = {};

	IPCChannelBuffer() = default;
	DISABLE_CLASS_COPY(IPCChannelBuffer)
	~IPCChannelBuffer() = default;
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
	struct Data
	{
		IPCChannelShared *shared = nullptr;
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

class IPCChildProcess;

// For testing
// Returns one end and a thread holding the other end. The thread will execute
// fun, and pass it the other end.
std::pair<IPCChannelEnd, std::thread> make_test_ipc_channel(
		const std::function<void(IPCChannelEnd)> &fun);

// Create a cross-process IPC channel: allocates shared memory with a
// generated name and spawns a worker process via IPCChildProcess to act
// as end B. The current process becomes end A. Returns end A plus the
// handle to the child process (so the caller can monitor it).
//
// The worker is launched via `<exec_path> --sscsm-worker <shmname>`.
// If exec_path is empty, the current executable is used.
//
// Returns { empty_end, invalid_process } on failure.
std::pair<IPCChannelEnd, std::unique_ptr<IPCChildProcess>>
make_child_ipc_channel(const std::string &exec_path = {});

// Shared-memory-backed resources for use across a process boundary.
// POSIX: uses shm_open + mmap, with shm_unlink after both ends attach.
// Win32: uses CreateFileMapping + MapViewOfFile.
//
// The parent creates with makeFirst(name_prefix) — this allocates shared
// memory with a generated unique name and stores the name internally.
// Callers retrieve the name via getName() and are responsible for passing
// it to the child process (via env var, fd passing, or other means).
// The child attaches with makeSecond(name).
struct IPCChannelResourcesShm final : public IPCChannelResources
{
	~IPCChannelResourcesShm() override;

	// Parent side: create a new shm region with a generated name.
	// name_prefix should be a short string used in the generated name
	// for debugging (e.g. "luanti-sscsm").
	static std::unique_ptr<IPCChannelResourcesShm>
			makeFirst(const std::string &name_prefix);

	// Child side: attach to an existing shm region by name.
	// Returns nullptr on failure (e.g. name does not exist, other end dead).
	static std::unique_ptr<IPCChannelResourcesShm>
			makeSecond(const std::string &name);

	// The shm object name. Only meaningful for the parent after makeFirst.
	// The child must receive this name through some other channel.
	const std::string &getName() const noexcept { return m_name; }

	void cleanupLast() noexcept override;
	void cleanupNotLast() noexcept override;

private:
	std::string m_name;
	// POSIX: true if we created the shm (so we should shm_unlink on cleanup).
	// Win32: true if we created the mapping (for CloseHandle ordering).
	bool m_is_creator = false;
#if defined(_WIN32)
	void *m_mapping_handle = nullptr;
#endif
};
