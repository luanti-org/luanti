// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ipc_channel.h"
#include "debug.h"
#include "exceptions.h"
#include "porting.h"
#include <cstring>

// Spin-wait until the buffer's flag becomes 1, then reset it.
// timeout_ms_abs: absolute deadline from porting::getTimeMs(), or 0 for no timeout.
// Returns false on timeout.
static bool wait_in(IPCChannelEnd::Dir *dir, u64 timeout_ms_abs) noexcept
{
	while (true) {
		u32 expected = 1;
		if (dir->buf_in->flag.compare_exchange_weak(expected, 0,
				std::memory_order_acquire, std::memory_order_relaxed)) {
			return true;
		}
		if (timeout_ms_abs > 0 && porting::getTimeMs() > timeout_ms_abs)
			return false;
		std::this_thread::yield();
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
