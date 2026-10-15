// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"
#include "client/sound/device_reconnect.h"

class TestDeviceReconnect : public TestBase
{
public:
	TestDeviceReconnect() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestDeviceReconnect"; }
	void runTests(IGameDef *) { TEST(testRecovery); }
	void testRecovery();
};

static TestDeviceReconnect g_test_instance;

void TestDeviceReconnect::testRecovery()
{
	using Recovery = sound::DeviceReconnect;
	using Result = Recovery::Result;
	Recovery recovery;
	const auto start = Recovery::Clock::time_point{};
	int attempts = 0;
	bool available = false;
	auto reopen = [&] { ++attempts; return available; };
	auto poll = [&](int ms, bool connected) {
		return recovery.poll(start + std::chrono::milliseconds(ms), connected, reopen);
	};
	UASSERT(poll(0, true) == Result::Connected);
	UASSERT(attempts == 0);
	UASSERT(poll(10, false) == Result::Lost);
	UASSERT(poll(20, false) == Result::RetryFailed);
	UASSERT(poll(1019, false) == Result::Waiting);
	UASSERT(attempts == 1);
	UASSERT(poll(1020, false) == Result::RetryFailed);
	available = true;
	UASSERT(poll(2019, false) == Result::Waiting);
	UASSERT(poll(2020, false) == Result::Recovered);
	UASSERT(attempts == 3);
	UASSERT(poll(2030, true) == Result::Connected);
	UASSERT(attempts == 3);
	// Another disconnect must recover without inheriting the old retry deadline.
	UASSERT(poll(2040, false) == Result::Lost);
	UASSERT(poll(2050, false) == Result::Recovered);
	UASSERT(attempts == 4);
	// Also accept recovery performed by the backend itself.
	UASSERT(poll(2060, false) == Result::Lost);
	UASSERT(poll(2070, true) == Result::Recovered);
	UASSERT(attempts == 4);
}
