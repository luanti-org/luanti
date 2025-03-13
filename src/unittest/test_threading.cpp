// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <atomic>
#include <iostream>
#include "threading/semaphore.h"
#include "threading/thread.h"


class TestThreading : public TestBase {
public:
	TestThreading() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestThreading"; }
	void runTests(IGameDef *gamedef);

	void testStartStopWait();
	void testAtomicSemaphoreThread();
	void testTLS();
};

static TestThreading g_test_instance;

void TestThreading::runTests(IGameDef *gamedef)
{
	TEST(testStartStopWait);
	TEST(testAtomicSemaphoreThread);
	TEST(testTLS);
}

class SimpleTestThread : public Thread {
public:
	SimpleTestThread(unsigned int interval) :
		Thread("SimpleTest"),
		m_interval(interval)
	{
	}

private:
	void *run()
	{
		void *retval = this;

		if (isCurrentThread() == false)
			retval = (void *)0xBAD;

		while (!stopRequested())
			sleep_ms(m_interval);

		return retval;
	}

	unsigned int m_interval;
};

void TestThreading::testStartStopWait()
{
	void *thread_retval;
	SimpleTestThread *thread = new SimpleTestThread(25);

	// Try this a couple times, since a Thread should be reusable after waiting
	for (size_t i = 0; i != 5; i++) {
		// Can't wait() on a joined, stopped thread
		UASSERT(thread->wait() == false);

		// start() should work the first time, but not the second.
		UASSERT(thread->start() == true);
		UASSERT(thread->start() == false);

		UASSERT(thread->isRunning() == true);
		UASSERT(thread->isCurrentThread() == false);

		// Let it loop a few times...
		sleep_ms(70);

		// It's still running, so the return value shouldn't be available to us.
		UASSERT(thread->getReturnValue(&thread_retval) == false);

		// stop() should always succeed
		UASSERT(thread->stop() == true);

		// wait() only needs to wait the first time - the other two are no-ops.
		UASSERT(thread->wait() == true);
		UASSERT(thread->wait() == false);
		UASSERT(thread->wait() == false);

		// Now that the thread is stopped, we should be able to get the
		// return value, and it should be the object itself.
		thread_retval = NULL;
		UASSERT(thread->getReturnValue(&thread_retval) == true);
		UASSERT(thread_retval == thread);
	}

	delete thread;
}



class AtomicTestThread : public Thread {
public:
	AtomicTestThread(std::atomic<u32> &v, Semaphore &trigger) :
		Thread("AtomicTest"),
		val(v),
		trigger(trigger)
	{
	}

private:
	void *run()
	{
		trigger.wait();
		for (u32 i = 0; i < 0x10000; ++i)
			++val;
		return NULL;
	}

	std::atomic<u32> &val;
	Semaphore &trigger;
};


void TestThreading::testAtomicSemaphoreThread()
{
	std::atomic<u32> val;
	val = 0;
	Semaphore trigger;
	static const u8 num_threads = 4;

	AtomicTestThread *threads[num_threads];
	for (auto &thread : threads) {
		thread = new AtomicTestThread(val, trigger);
		UASSERT(thread->start());
	}

	trigger.post(num_threads);

	for (AtomicTestThread *thread : threads) {
		thread->wait();
		delete thread;
	}

	UASSERT(val == num_threads * 0x10000);
}



static std::atomic<bool> g_tls_broken;

class TLSTestThread : public Thread {
public:
	TLSTestThread() : Thread("TLSTest")
	{
	}

private:
	struct TestObject {
		TestObject() {
			for (u32 i = 0; i < buffer_size; i++)
				buffer[i] = (i % 2) ? 0xa1 : 0x1a;
		}
		~TestObject() {
			for (u32 i = 0; i < buffer_size; i++) {
				const u8 expect = (i % 2) ? 0xa1 : 0x1a;
				if (buffer[i] != expect) {
					std::cout << "At offset " << i << " expected " << (int)expect
						<< " but found " << (int)buffer[i] << std::endl;
					g_tls_broken = true;
					break;
				}
				// If we're unlucky the loop might actually just crash.
				// probably the user will realize the test failure :)
			}
		}
		// larger objects seem to surface corruption more easily
		static constexpr u32 buffer_size = 576;
		u8 buffer[buffer_size];
	};

	void *run() {
		thread_local TestObject foo;
		while (!stopRequested())
			sleep_ms(1);
		return nullptr;
	}
};

/*
	What are we actually testing here?

	MinGW with gcc has a long-standing unsolved bug where memory belonging to
	thread-local variables is freed *before* the destructors are called.
	Needless to say this leads to unreliable crashes whenever a thread exits.
	It does not affect MSVC or MinGW+clang and as far as we know no other platforms
	are affected either.

	Related reports and information:
	* <https://sourceforge.net/p/mingw-w64/bugs/727/> (2018)
	* <https://github.com/msys2/MINGW-packages/issues/2519> (2017)
	* <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58142> maybe

	Note that this is different from <https://sourceforge.net/p/mingw-w64/bugs/527/>,
	which affected only 32-bit MinGW. It was caused by incorrect calling convention
	and fixed in GCC in 2020.

	Occurrences in Luanti:
	* <https://github.com/luanti-org/luanti/issues/10137> (2020)
	* <https://github.com/luanti-org/luanti/issues/12022> (2022)
	* <https://github.com/luanti-org/luanti/issues/14140> (2023)
*/
void TestThreading::testTLS()
{
	constexpr int num_threads = 10;

	for (int j = 0; j < num_threads; j++) {
		g_tls_broken = false;

		TLSTestThread thread;
		thread.start();
		thread.stop();
		thread.wait();

		if (g_tls_broken) {
			std::cout << "While running test thread " << j << std::endl;
			UASSERT(!g_tls_broken);
		}
	}
}
