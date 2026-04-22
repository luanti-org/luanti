// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ipc_channel.h"
#include "ipc_child_process.h"
#include "debug.h"
#include "exceptions.h"
#include "log.h"
#include "porting.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <new>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

// Wait until the buffer's flag becomes 1, then reset it.
// timeout_ms_abs: absolute deadline from porting::getTimeMs(), or 0 for no timeout.
// Returns false on timeout.
//
// Two-phase wait:
//   Phase 1 (iter < SPIN_ITERS): busy spin with yield(). Catches
//     back-to-back sub-requests with sub-microsecond latency.
//   Phase 2 (iter >= SPIN_ITERS): sleep 1ms in the kernel per iteration.
//     Gives ~0% idle CPU at the cost of up to 1ms wake latency, which
//     is ~6% of a 60Hz frame budget — invisible in practice.
static constexpr int SPIN_ITERS = 100;

static bool wait_in(IPCChannelEnd::Dir *dir, u64 timeout_ms_abs) noexcept
{
	int iter = 0;
	while (true) {
		u32 expected = 1;
		if (dir->buf_in->flag.compare_exchange_weak(expected, 0,
				std::memory_order_acquire, std::memory_order_relaxed)) {
			return true;
		}
		if (timeout_ms_abs > 0 && porting::getTimeMs() > timeout_ms_abs)
			return false;

		if (iter < SPIN_ITERS) {
			std::this_thread::yield();
			++iter;
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

// Post: set the buffer's flag to 1.
static void post_out(IPCChannelEnd::Dir *dir) noexcept
{
	dir->buf_out->flag.store(1, std::memory_order_release);
}

template <typename T>
static void write_once(volatile T *var, const T val)
{
	*var = val;
}

template <typename T>
static T read_once(const volatile T *var)
{
	return *var;
}

IPCChannelEnd IPCChannelEnd::makeA(std::unique_ptr<IPCChannelResources> resources)
{
	IPCChannelShared *shared = resources->data.shared;
	return IPCChannelEnd(std::move(resources), Dir{&shared->a, &shared->b});
}

IPCChannelEnd IPCChannelEnd::makeB(std::unique_ptr<IPCChannelResources> resources)
{
	IPCChannelShared *shared = resources->data.shared;
	return IPCChannelEnd(std::move(resources), Dir{&shared->b, &shared->a});
}

void IPCChannelEnd::sendSmall(const void *data, size_t size) noexcept
{
	write_once(&m_dir.buf_out->size, size);

	if (size != 0)
		memcpy(m_dir.buf_out->data, data, size);

	post_out(&m_dir);
}

bool IPCChannelEnd::sendLarge(const void *data, size_t size, int timeout_ms) noexcept
{
	u64 timeout_ms_abs = timeout_ms < 0 ? 0 : porting::getTimeMs() + timeout_ms;

	write_once(&m_dir.buf_out->size, size);

	do {
		memcpy(m_dir.buf_out->data, data, IPC_CHANNEL_MSG_SIZE);
		post_out(&m_dir);

		if (!wait_in(&m_dir, timeout_ms_abs))
			return false;

		size -= IPC_CHANNEL_MSG_SIZE;
		data = reinterpret_cast<const u8 *>(data) + IPC_CHANNEL_MSG_SIZE;
	} while (size > IPC_CHANNEL_MSG_SIZE);

	if (size != 0)
		memcpy(m_dir.buf_out->data, data, size);
	post_out(&m_dir);

	return true;
}

bool IPCChannelEnd::recvWithTimeout(int timeout_ms) noexcept
{
	// Note about memcpy: If the other thread is evil, it might change the contents
	// of the memory while it's memcopied. We're assuming here that memcpy doesn't
	// cause vulnerabilities due to this.

	u64 timeout_ms_abs = timeout_ms < 0 ? 0 : porting::getTimeMs() + timeout_ms;

	if (!wait_in(&m_dir, timeout_ms_abs))
		return false;

	size_t size = read_once(&m_dir.buf_in->size);
	m_recv_size = size;

	if (size <= IPC_CHANNEL_MSG_SIZE) {
		// small msg
		// (m_large_recv.size() is always >= IPC_CHANNEL_MSG_SIZE)
		if (size != 0)
			memcpy(m_large_recv.data(), m_dir.buf_in->data, size);

	} else {
		// large msg
		try {
			m_large_recv.resize(size);
		} catch (...) {
			// it's ok for us if an attacker wants to make us abort
			std::string errmsg = "std::vector::resize failed, size was: "
					+ std::to_string(size);
			FATAL_ERROR(errmsg.c_str());
		}
		u8 *recv_data = m_large_recv.data();
		do {
			memcpy(recv_data, m_dir.buf_in->data, IPC_CHANNEL_MSG_SIZE);
			size -= IPC_CHANNEL_MSG_SIZE;
			recv_data += IPC_CHANNEL_MSG_SIZE;
			post_out(&m_dir);
			if (!wait_in(&m_dir, timeout_ms_abs))
				return false;
		} while (size > IPC_CHANNEL_MSG_SIZE);
		if (size != 0)
			memcpy(recv_data, m_dir.buf_in->data, size);
	}
	return true;
}

std::pair<IPCChannelEnd, std::thread> make_test_ipc_channel(
		const std::function<void(IPCChannelEnd)> &fun)
{
	auto shared = new IPCChannelShared();
	auto resource_data = IPCChannelResources::Data{shared};

	std::thread thread_b([=] {
		auto resources_second = IPCChannelResourcesSingleProcess::makeSecond(resource_data);
		IPCChannelEnd end_b = IPCChannelEnd::makeB(std::move(resources_second));

		fun(std::move(end_b));
	});

	auto resources_first = IPCChannelResourcesSingleProcess::makeFirst(resource_data);
	IPCChannelEnd end_a = IPCChannelEnd::makeA(std::move(resources_first));

	return {std::move(end_a), std::move(thread_b)};
}

////////////////////////////////////////////////////////////////////////////////
// IPCChannelResourcesShm: shared-memory-backed resources for cross-process use

IPCChannelResourcesShm::~IPCChannelResourcesShm()
{
	cleanup();
}

#if defined(_WIN32)

static std::atomic<u32> g_shm_counter{0};

std::unique_ptr<IPCChannelResourcesShm>
IPCChannelResourcesShm::makeFirst(const std::string &name_prefix)
{
	u32 n = g_shm_counter.fetch_add(1);
	std::string name = name_prefix + "-" +
			std::to_string(GetCurrentProcessId()) + "-" + std::to_string(n);

	HANDLE h = CreateFileMappingA(
			INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
			0, sizeof(IPCChannelShared), name.c_str());
	if (!h || GetLastError() == ERROR_ALREADY_EXISTS) {
		if (h) CloseHandle(h);
		errorstream << "IPCChannelResourcesShm: CreateFileMappingA failed for "
				<< name << std::endl;
		return nullptr;
	}
	void *addr = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0,
			sizeof(IPCChannelShared));
	if (!addr) {
		CloseHandle(h);
		errorstream << "IPCChannelResourcesShm: MapViewOfFile failed" << std::endl;
		return nullptr;
	}

	// Initialize the shared struct in place
	auto *shared = new (addr) IPCChannelShared();

	auto ret = std::make_unique<IPCChannelResourcesShm>();
	ret->m_name = std::move(name);
	ret->m_is_creator = true;
	ret->m_mapping_handle = h;
	Data d{};
	d.shared = shared;
	ret->setFirst(d);
	return ret;
}

std::unique_ptr<IPCChannelResourcesShm>
IPCChannelResourcesShm::makeSecond(const std::string &name)
{
	HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
	if (!h) {
		errorstream << "IPCChannelResourcesShm: OpenFileMappingA failed for "
				<< name << std::endl;
		return nullptr;
	}
	void *addr = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0,
			sizeof(IPCChannelShared));
	if (!addr) {
		CloseHandle(h);
		errorstream << "IPCChannelResourcesShm: MapViewOfFile failed" << std::endl;
		return nullptr;
	}

	auto *shared = reinterpret_cast<IPCChannelShared *>(addr);

	auto ret = std::make_unique<IPCChannelResourcesShm>();
	ret->m_name = name;
	ret->m_is_creator = false;
	ret->m_mapping_handle = h;
	Data d{};
	d.shared = shared;
	if (!ret->setSecond(d)) {
		UnmapViewOfFile(addr);
		CloseHandle(h);
		return nullptr;
	}
	return ret;
}

void IPCChannelResourcesShm::cleanupLast() noexcept
{
	if (data.shared) {
		data.shared->~IPCChannelShared();
		UnmapViewOfFile(data.shared);
	}
	if (m_mapping_handle)
		CloseHandle(m_mapping_handle);
}

void IPCChannelResourcesShm::cleanupNotLast() noexcept
{
	if (data.shared)
		UnmapViewOfFile(data.shared);
	if (m_mapping_handle)
		CloseHandle(m_mapping_handle);
}

#else // POSIX

static std::atomic<u32> g_shm_counter{0};

std::unique_ptr<IPCChannelResourcesShm>
IPCChannelResourcesShm::makeFirst(const std::string &name_prefix)
{
	u32 n = g_shm_counter.fetch_add(1);
	// POSIX shm names must start with '/' and have no other '/'.
	std::string name = "/" + name_prefix + "-" +
			std::to_string(getpid()) + "-" + std::to_string(n);

	int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0) {
		errorstream << "IPCChannelResourcesShm: shm_open(create) failed for "
				<< name << ": " << std::strerror(errno) << std::endl;
		return nullptr;
	}
	if (ftruncate(fd, sizeof(IPCChannelShared)) < 0) {
		errorstream << "IPCChannelResourcesShm: ftruncate failed: "
				<< std::strerror(errno) << std::endl;
		close(fd);
		shm_unlink(name.c_str());
		return nullptr;
	}
	void *addr = mmap(nullptr, sizeof(IPCChannelShared),
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		errorstream << "IPCChannelResourcesShm: mmap failed: "
				<< std::strerror(errno) << std::endl;
		shm_unlink(name.c_str());
		return nullptr;
	}

	// Initialize the shared struct in place
	auto *shared = new (addr) IPCChannelShared();

	auto ret = std::make_unique<IPCChannelResourcesShm>();
	ret->m_name = std::move(name);
	ret->m_is_creator = true;
	Data d{};
	d.shared = shared;
	ret->setFirst(d);
	return ret;
}

std::unique_ptr<IPCChannelResourcesShm>
IPCChannelResourcesShm::makeSecond(const std::string &name)
{
	int fd = shm_open(name.c_str(), O_RDWR, 0);
	if (fd < 0) {
		errorstream << "IPCChannelResourcesShm: shm_open(attach) failed for "
				<< name << ": " << std::strerror(errno) << std::endl;
		return nullptr;
	}
	void *addr = mmap(nullptr, sizeof(IPCChannelShared),
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		errorstream << "IPCChannelResourcesShm: mmap(attach) failed: "
				<< std::strerror(errno) << std::endl;
		return nullptr;
	}

	auto *shared = reinterpret_cast<IPCChannelShared *>(addr);

	auto ret = std::make_unique<IPCChannelResourcesShm>();
	ret->m_name = name;
	ret->m_is_creator = false;
	Data d{};
	d.shared = shared;
	if (!ret->setSecond(d)) {
		munmap(addr, sizeof(IPCChannelShared));
		return nullptr;
	}
	return ret;
}

void IPCChannelResourcesShm::cleanupLast() noexcept
{
	if (data.shared) {
		data.shared->~IPCChannelShared();
		munmap(data.shared, sizeof(IPCChannelShared));
	}
	if (m_is_creator && !m_name.empty())
		shm_unlink(m_name.c_str());
}

void IPCChannelResourcesShm::cleanupNotLast() noexcept
{
	if (data.shared)
		munmap(data.shared, sizeof(IPCChannelShared));
	// If we're the creator but not the last to go, still unlink the name
	// so new openers can't attach. The inode stays alive for the other end.
	if (m_is_creator && !m_name.empty())
		shm_unlink(m_name.c_str());
}

#endif

////////////////////////////////////////////////////////////////////////////////
// make_child_ipc_channel: spawn a worker process and wire it to the channel.

std::pair<IPCChannelEnd, std::unique_ptr<IPCChildProcess>>
make_child_ipc_channel(const std::string &exec_path)
{
	auto parent_res = IPCChannelResourcesShm::makeFirst("luanti-sscsm");
	if (!parent_res) {
		return {IPCChannelEnd{}, nullptr};
	}
	std::string shm_name = parent_res->getName();

	std::vector<std::string> args = {"--sscsm-worker", shm_name};
	auto child = exec_path.empty()
			? std::make_unique<IPCChildProcess>(args)
			: std::make_unique<IPCChildProcess>(exec_path, args);

	if (!child->isValid()) {
		// Teardown: parent_res destructor unlinks the shm name.
		return {IPCChannelEnd{}, nullptr};
	}

	auto end_a = IPCChannelEnd::makeA(std::move(parent_res));
	return {std::move(end_a), std::move(child)};
}
