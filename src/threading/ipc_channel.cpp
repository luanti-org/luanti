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

#include "ipc_channel.h"
#include "debug.h"
#include "exceptions.h"
#include "porting.h"
#include <cerrno>
#include <utility>

#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)
#include <linux/futex.h>
#include <cstring>
#include <sys/syscall.h>
#include <sys/wait.h>
#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif
#endif

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

#else

#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)

#if defined(__i386__) || defined(__x86_64__)
static void busy_wait(int n) noexcept
{
	for (int i = 0; i < n; i++)
		_mm_pause();
}
#endif // defined(__i386__) || defined(__x86_64__)

static int futex(std::atomic<u32> *uaddr, int futex_op, u32 val,
		const struct timespec *timeout, u32 *uaddr2, u32 val3) noexcept
{
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

#endif // defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)

// timeout: relative on linux, and absolute on other posix
// returns false on timeout
static bool wait(IPCChannelBuffer *buf, const struct timespec *timeout) noexcept
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)
	// try busy waiting
	for (int i = 0; i < 100; i++) {
		// posted?
		if (buf->futex.exchange(0) == 1)
			return true; // yes
#if defined(__i386__) || defined(__x86_64__)
		busy_wait(40);
#else
		break; // Busy wait not implemented
#endif
	}
	// wait with futex
	while (true) {
		// write 2 to show that we're futexing
		if (buf->futex.exchange(2) == 1) {
			// futex was posted => change 2 to 0 (or 1 to 1)
			buf->futex.fetch_and(1);
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
#elif defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
	bool timed_out = false;
	pthread_mutex_lock(&buf->mutex);
	if (!buf->posted) {
		if (timeout)
			timed_out = pthread_cond_timedwait(&buf->cond, &buf->mutex, timeout) == ETIMEDOUT;
		else
			pthread_cond_wait(&buf->cond, &buf->mutex);
	}
	buf->posted = false;
	pthread_mutex_unlock(&buf->mutex);
	return !timed_out;
#endif
}

static void post(IPCChannelBuffer *buf) noexcept
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX)
	if (buf->futex.exchange(1) == 2) {
		// 2 means reader needs to be notified
		int s = futex(&buf->futex, FUTEX_WAKE, 1, nullptr, nullptr, 0);
		if (s == -1) {
			std::string errmsg = std::string("FUTEX_WAKE failed unexpectedly: ")
					+ std::strerror(errno);
			FATAL_ERROR(errmsg.c_str());
		}
	}
#elif defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
	pthread_mutex_lock(&buf->mutex);
	buf->posted = true;
	pthread_cond_broadcast(&buf->cond);
	pthread_mutex_unlock(&buf->mutex);
#endif
}

#endif // !defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)

#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
static DWORD get_timeout(int timeout_ms)
{
	return timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
}
#elif defined(IPC_CHANNEL_IMPLEMENTATION_LINUX_FUTEX) || defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
static struct timespec *set_timespec(struct timespec *ts, int ms)
{
	if (ms < 0)
		return nullptr;
	u64 msu = ms;
#if defined(IPC_CHANNEL_IMPLEMENTATION_POSIX)
	msu += porting::getTimeMs(); // Absolute time
#endif
	ts->tv_sec = msu / 1000;
	ts->tv_nsec = msu % 1000 * 1000000UL;
	return ts;
}
#endif

template <typename T>
static inline void write_once(volatile T *var, const T val)
{
	*var = val;
}

template <typename T>
static inline T read_once(const volatile T *var)
{
	return *var;
}

IPCChannelEnd IPCChannelEnd::makeA(std::unique_ptr<IPCChannelResources> resources)
{
	IPCChannelShared *shared = resources->data.shared;
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	HANDLE sem_a = resources->data.sem_a;
	HANDLE sem_b = resources->data.sem_b;
	return IPCChannelEnd(std::move(resources), &shared->a, &shared->b, sem_a, sem_b);
#else
	return IPCChannelEnd(std::move(resources), &shared->a, &shared->b);
#endif // !defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
}

IPCChannelEnd IPCChannelEnd::makeB(std::unique_ptr<IPCChannelResources> resources)
{
	IPCChannelShared *shared = resources->data.shared;
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	HANDLE sem_a = resources->data.sem_a;
	HANDLE sem_b = resources->data.sem_b;
	return IPCChannelEnd(std::move(resources), &shared->b, &shared->a, sem_b, sem_a);
#else
	return IPCChannelEnd(std::move(resources), &shared->b, &shared->a);
#endif // !defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
}

void IPCChannelEnd::sendSmall(const void *data, size_t size) noexcept
{
	write_once(&m_out->size, size);
	memcpy(m_out->data, data, size);
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	post(m_sem_out);
#else
	post(m_out);
#endif
}

bool IPCChannelEnd::sendLarge(const void *data, size_t size, int timeout_ms) noexcept
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	DWORD timeout = get_timeout(timeout_ms);
#else
	struct timespec timeout;
	struct timespec *timeoutp = set_timespec(&timeout, timeout_ms);
#endif
	write_once(&m_out->size, size);
	do {
		memcpy(m_out->data, data, IPC_CHANNEL_MSG_SIZE);
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
		post(m_sem_out);
#else
		post(m_out);
#endif
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
		if (!wait(m_sem_in, timeout))
#else
		if (!wait(m_in, timeoutp))
#endif
			return false;
		size -= IPC_CHANNEL_MSG_SIZE;
		data = (u8 *)data + IPC_CHANNEL_MSG_SIZE;
	} while (size > IPC_CHANNEL_MSG_SIZE);
	memcpy(m_out->data, data, size);
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	post(m_sem_out);
#else
	post(m_out);
#endif
	return true;
}

bool IPCChannelEnd::recv(int timeout_ms) noexcept
{
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	DWORD timeout = get_timeout(timeout_ms);
#else
	struct timespec timeout;
	struct timespec *timeoutp = set_timespec(&timeout, timeout_ms);
#endif
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
	if (!wait(m_sem_in, timeout))
#else
	if (!wait(m_in, timeoutp))
#endif
		return false;
	size_t size = read_once(&m_in->size);
	m_recv_size = size;
	if (size <= IPC_CHANNEL_MSG_SIZE) {
		// m_large_recv.size() is always >= IPC_CHANNEL_MSG_SIZE
		memcpy(m_large_recv.data(), m_in->data, size);
	} else {
		try {
			m_large_recv.resize(size);
		} catch (...) {
			// it's ok for us if an attacker wants to make us abort
			std::string errmsg = std::string("std::vector::resize failed, size was: ")
					+ std::to_string(size);
			FATAL_ERROR(errmsg.c_str());
		}
		u8 *recv_data = m_large_recv.data();
		do {
			memcpy(recv_data, m_in->data, IPC_CHANNEL_MSG_SIZE);
			size -= IPC_CHANNEL_MSG_SIZE;
			recv_data += IPC_CHANNEL_MSG_SIZE;
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
			post(m_sem_out);
#else
			post(m_out);
#endif
#if defined(IPC_CHANNEL_IMPLEMENTATION_WIN32)
			if (!wait(m_sem_in, timeout))
#else
			if (!wait(m_in, timeoutp))
#endif
				return false;
		} while (size > IPC_CHANNEL_MSG_SIZE);
		memcpy(recv_data, m_in->data, size);
	}
	return true;
}
