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

#include "ipc_channel.h"
#include "debug.h"
#include "exceptions.h"
#include "porting.h"
#include <cerrno>
#include <utility>
#include <cstring>

#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#endif

#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX) && (defined(__i386__) || defined(__x86_64__))
#include <immintrin.h>

#define HAVE_BUSY_WAIT 1

[[maybe_unused]]
static void busy_wait(int n) noexcept
{
	for (int i = 0; i < n; i++)
		_mm_pause();
}

#else
#define HAVE_BUSY_WAIT 0
#endif

#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)

// returns false on timeout
static bool wait(HANDLE sem, DWORD timeout)
{
	return WaitForSingleObject(sem, timeout) == WAIT_OBJECT_0;
}

static void post(HANDLE sem)
{
	if (!ReleaseSemaphore(sem, 1, nullptr))
		FATAL_ERROR("ReleaseSemaphore failed unexpectedly");
}

#elif defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)

static int futex(std::atomic<u32> *uaddr, int futex_op, u32 val,
		const struct timespec *timeout, u32 *uaddr2, u32 val3) noexcept
{
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

// timeout is relative
// returns false on timeout
static bool wait(IPCChannelBuffer *buf, const struct timespec *timeout) noexcept
{
	// try busy waiting
	for (int i = 0; i < 100; i++) {
		// posted?
		if (buf->futex.load(std::memory_order_acquire) == 1) {
			// yes
			// reset it. (relaxed ordering is sufficient, because the other thread
			// does not need to see the side effects we did before unposting)
			buf->futex.store(0, std::memory_order_relaxed);
			return true;
		}
#if HAVE_BUSY_WAIT
		busy_wait(40);
#else
		break;
#endif
	}
	// wait with futex
	while (true) {
		// write 2 to show that we're futexing
		if (buf->futex.exchange(2, std::memory_order_acquire) == 1) {
			// it was posted in the meantime
			buf->futex.store(0, std::memory_order_relaxed);
			return true;
		}
		int s = futex(&buf->futex, FUTEX_WAIT, 2, timeout, nullptr, 0);
		if (s == -1) {
			if (errno == ETIMEDOUT) {
				return false;
			} else if (errno != EAGAIN && errno != EINTR) {
				std::string errmsg = std::string("FUTEX_WAIT failed unexpectedly: ")
						+ std::strerror(errno);
				FATAL_ERROR(errmsg.c_str());
			}
		}
	}
}

static void post(IPCChannelBuffer *buf) noexcept
{
	if (buf->futex.exchange(1, std::memory_order_release) == 2) {
		// 2 means reader needs to be notified
		int s = futex(&buf->futex, FUTEX_WAKE, 1, nullptr, nullptr, 0);
		if (s == -1) {
			std::string errmsg = std::string("FUTEX_WAKE failed unexpectedly: ")
					+ std::strerror(errno);
			FATAL_ERROR(errmsg.c_str());
		}
	}
}

#elif defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)

// timeout is absolute (using cond_clockid)
// returns false on timeout
static bool wait(IPCChannelBuffer *buf, const struct timespec *timeout) noexcept
{
	bool timed_out = false;
	pthread_mutex_lock(&buf->mutex);
	while (!buf->posted) {
		if (timeout) {
			auto err = pthread_cond_timedwait(&buf->cond, &buf->mutex, timeout);
			if (err == ETIMEDOUT) {
				timed_out = true;
				break;
			}
			FATAL_ERROR_IF(err != 0 && err != EINTR, "pthread_cond_timedwait failed");
		} else {
			pthread_cond_wait(&buf->cond, &buf->mutex);
		}
	}
	buf->posted = false;
	pthread_mutex_unlock(&buf->mutex);
	return !timed_out;
}

static void post(IPCChannelBuffer *buf) noexcept
{
	pthread_mutex_lock(&buf->mutex);
	buf->posted = true;
	pthread_mutex_unlock(&buf->mutex);
	pthread_cond_broadcast(&buf->cond);
}

#endif

// timeout_ms_abs: absolute timeout (using porting::getTimeMs()), or 0 for no timeout
// returns false on timeout
static bool wait_in(IPCChannelEnd::Dir *dir, u64 timeout_ms_abs)
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	// Relative time
	DWORD timeout = INFINITE;
	if (timeout_ms_abs > 0) {
		u64 tnow = porting::getTimeMs();
		if (tnow > timeout_ms_abs)
			return false;
		timeout = (DWORD)(timeout_ms_abs - tnow);
	}
	return wait(dir->sem_in, timeout);

#else
	struct timespec timeout;
	struct timespec *timeoutp = nullptr;
	if (timeout_ms_abs > 0) {
		u64 tnow = porting::getTimeMs();
		if (tnow > timeout_ms_abs)
			return false;
		u64 timeout_ms_rel = timeout_ms_abs - tnow;
#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)
		// Relative time
		timeout.tv_sec = 0;
		timeout.tv_nsec = 0;
#elif defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
		// Absolute time
		FATAL_ERROR_IF(clock_gettime(CLOCK_REALTIME, &timeout) < 0,
				"clock_gettime failed");
#endif
		timeout.tv_sec += timeout_ms_rel / 1000;
		timeout.tv_nsec += timeout_ms_rel % 1000 * 1000'000L;
		// tv_nsec must be smaller than 1 sec, or else pthread_cond_timedwait fails
		if (timeout.tv_nsec >= 1000'000'000L) {
			timeout.tv_nsec -= 1000'000'000L;
			timeout.tv_sec += 1;
		}
		timeoutp = &timeout;
	}

	return wait(dir->buf_in, timeoutp);
#endif
}

static void post_out(IPCChannelEnd::Dir *dir)
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	post(dir->sem_out);
#else
	post(dir->buf_out);
#endif
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

IPCChannelBuffer::IPCChannelBuffer()
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
	pthread_condattr_t condattr;
	pthread_mutexattr_t mutexattr;
	if (pthread_condattr_init(&condattr) != 0)
		goto error_condattr_init;
	if (pthread_mutexattr_init(&mutexattr) != 0)
		goto error_mutexattr_init;
	if (pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED) != 0)
		goto error_condattr_setpshared;
	if (pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) != 0)
		goto error_mutexattr_setpshared;
	if (pthread_cond_init(&cond, &condattr) != 0)
		goto error_cond_init;
	if (pthread_mutex_init(&mutex, &mutexattr) != 0)
		goto error_mutex_init;
	pthread_mutexattr_destroy(&mutexattr);
	pthread_condattr_destroy(&condattr);
	return;

error_mutex_init:
	pthread_cond_destroy(&cond);
error_cond_init:
error_mutexattr_setpshared:
error_condattr_setpshared:
	pthread_mutexattr_destroy(&mutexattr);
error_mutexattr_init:
	pthread_condattr_destroy(&condattr);
error_condattr_init:
	throw BaseException("Unable to initialize IPCChannelBuffer");
#endif // defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
}

IPCChannelBuffer::~IPCChannelBuffer()
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
#endif // defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
}

IPCChannelEnd IPCChannelEnd::makeA(std::unique_ptr<IPCChannelResources> resources)
{
	IPCChannelShared *shared = resources->data.shared;
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	HANDLE sem_a = resources->data.sem_a;
	HANDLE sem_b = resources->data.sem_b;
	return IPCChannelEnd(std::move(resources), Dir{&shared->a, &shared->b, sem_a, sem_b});
#else
	return IPCChannelEnd(std::move(resources), Dir{&shared->a, &shared->b});
#endif // !defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
}

IPCChannelEnd IPCChannelEnd::makeB(std::unique_ptr<IPCChannelResources> resources)
{
	IPCChannelShared *shared = resources->data.shared;
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	HANDLE sem_a = resources->data.sem_a;
	HANDLE sem_b = resources->data.sem_b;
	return IPCChannelEnd(std::move(resources), Dir{&shared->b, &shared->a, sem_b, sem_a});
#else
	return IPCChannelEnd(std::move(resources), Dir{&shared->b, &shared->a});
#endif // !defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
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
	auto resource_data = [] {
		auto shared = new IPCChannelShared();

#ifdef IPC_CHANNEL_IMPLEMENTATION_WIN32
		HANDLE sem_a = CreateSemaphoreA(nullptr, 0, 1, nullptr);
		HANDLE sem_b = CreateSemaphoreA(nullptr, 0, 1, nullptr);
		FATAL_ERROR_IF(!sem_a || !sem_b, "CreateSemaphoreA failed");

		return IPCChannelResources::Data{shared, sem_a, sem_b};
#else
		return IPCChannelResources::Data{shared};
#endif
	}();

	std::thread thread_b([=] {
		auto resources_second = IPCChannelResourcesSingleProcess::makeSecond(resource_data);
		IPCChannelEnd end_b = IPCChannelEnd::makeB(std::move(resources_second));

		fun(std::move(end_b));
	});

	auto resources_first = IPCChannelResourcesSingleProcess::makeFirst(resource_data);
	IPCChannelEnd end_a = IPCChannelEnd::makeA(std::move(resources_first));

	return {std::move(end_a), std::move(thread_b)};
}
