/*
Minetest
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

#include "catch.h"
#include "threading/ipc_channel.h"
#include <thread>

TEST_CASE("benchmark_ipc_channel")
{
	// same as in test_threading.cpp (TODO: remove duplication)
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
	};

	auto resource_data = [] {
		auto shared = new IPCChannelShared();

#ifdef IPC_CHANNEL_IMPLEMENTATION_WIN32
		HANDLE sem_a = CreateSemaphoreA(nullptr, 0, 1, nullptr);
		REQUIRE(sem_a != INVALID_HANDLE_VALUE);

		HANDLE sem_b = CreateSemaphoreA(nullptr, 0, 1, nullptr);
		REQUIRE(sem_b != INVALID_HANDLE_VALUE);

		return IPCChannelResources::Data{shared, sem_a, sem_b};
#else
		return IPCChannelResources::Data{shared};
#endif
	}();

	auto resources_first = std::make_unique<IPCChannelResourcesSingleProcess>();
	resources_first->setFirst(resource_data);

	IPCChannelEnd end_a = IPCChannelEnd::makeA(std::move(resources_first));

	// echos back messages. stops if "" is sent
	std::thread thread_b([=] {
		auto resources_second = std::make_unique<IPCChannelResourcesSingleProcess>();
		resources_second->setSecond(resource_data);
		IPCChannelEnd end_b = IPCChannelEnd::makeB(std::move(resources_second));

		for (;;) {
			end_b.recv();
			end_b.send(end_b.getRecvData(), end_b.getRecvSize());
			if (end_b.getRecvSize() == 0)
				break;
		}
	});

	BENCHMARK("simple_call_1", i) {
		char buf[16] = {};
		buf[i & 0xf] = i;
		end_a.exchange(buf, 16);
		return reinterpret_cast<const u8 *>(end_a.getRecvData())[i & 0xf];
	};

	BENCHMARK("simple_call_1000", i) {
		char buf[16] = {};
		buf[i & 0xf] = i;
		for (int k = 0; k < 1000; ++k) {
			buf[0] = k & 0xff;
			end_a.exchange(buf, 16);
		}
		return reinterpret_cast<const u8 *>(end_a.getRecvData())[i & 0xf];
	};

	// stop thread_b
	end_a.exchange(nullptr, 0);
	REQUIRE(end_a.getRecvSize() == 0);

	thread_b.join();
}
