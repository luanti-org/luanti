// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <atomic>
#include <iostream>
#include "threading/ipc_channel.h"
#include "threading/ipc_child_process.h"
#include "threading/semaphore.h"
#include "threading/thread.h"
// SSCSM headers pull in client/client.h (request exec() bodies call into
// Client). The IPCChildProcess smoke test below speaks the SSCSM protocol,
// so we can only build that test in client-enabled builds.
#if CHECK_CLIENT_BUILD()
#include "script/sscsm/sscsm_events.h"
#include "script/sscsm/sscsm_ievent.h"
#include "script/sscsm/sscsm_irequest.h"
#include "script/sscsm/sscsm_requests.h"
#endif

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif


class TestThreading : public TestBase {
public:
	TestThreading() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestThreading"; }
	void runTests(IGameDef *gamedef);

	void testStartStopWait();
	void testAtomicSemaphoreThread();
	void testTLS();
	void testIPCChannel();
	void testIPCChannelShm();
#if CHECK_CLIENT_BUILD()
	void testIPCChildProcess();
#endif
};

static TestThreading g_test_instance;

void TestThreading::runTests(IGameDef *gamedef)
{
	TEST(testStartStopWait);
	TEST(testAtomicSemaphoreThread);
	TEST(testTLS);
	TEST(testIPCChannel);
	TEST(testIPCChannelShm);
#if CHECK_CLIENT_BUILD()
	TEST(testIPCChildProcess);
#endif
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

void TestThreading::testIPCChannel()
{
	auto [end_a, thread_b] = make_test_ipc_channel([](IPCChannelEnd end_b) {
		// echos back messages. stops if "" is sent
		while (true) {
			UASSERT(end_b.recvWithTimeout(-1));
			UASSERT(end_b.sendWithTimeout(end_b.getRecvData(), end_b.getRecvSize(), -1));
			if (end_b.getRecvSize() == 0)
				break;
		}
	});

	u8 buf1[20000] = {};
	for (int i = sizeof(buf1); i > 0; i -= 100) {
		buf1[i - 1] = 123;
		UASSERT(end_a.exchangeWithTimeout(buf1, i, -1));
		UASSERTEQ(int, end_a.getRecvSize(), i);
		UASSERTEQ(int, reinterpret_cast<const u8 *>(end_a.getRecvData())[i - 1], 123);
	}

	u8 buf2[IPC_CHANNEL_MSG_SIZE * 3 + 10];
	end_a.exchange(buf2, IPC_CHANNEL_MSG_SIZE * 3 + 10);
	end_a.exchange(buf2, IPC_CHANNEL_MSG_SIZE * 3);
	end_a.exchange(buf2, IPC_CHANNEL_MSG_SIZE);
	end_a.exchange(buf2, IPC_CHANNEL_MSG_SIZE * 2);
	end_a.exchange(buf2, IPC_CHANNEL_MSG_SIZE - 1);
	end_a.exchange(buf2, IPC_CHANNEL_MSG_SIZE + 1);
	end_a.exchange(buf2, 1);

	// stop thread_b
	UASSERT(end_a.exchangeWithTimeout(nullptr, 0, -1));
	UASSERTEQ(int, end_a.getRecvSize(), 0);

	thread_b.join();

	// other side dead ==> should time out
	UASSERT(!end_a.exchangeWithTimeout(nullptr, 0, 200));
}

void TestThreading::testIPCChannelShm()
{
#if defined(_WIN32)
	// Single-process round-trip only; we can't easily spawn a child in a
	// unit test on Windows without a lot of scaffolding.
	auto parent_res = IPCChannelResourcesShm::makeFirst("luanti-test");
	UASSERT(parent_res != nullptr);
	std::string name = parent_res->getName();
	UASSERT(!name.empty());

	auto child_res = IPCChannelResourcesShm::makeSecond(name);
	UASSERT(child_res != nullptr);

	auto end_a = IPCChannelEnd::makeA(std::move(parent_res));
	auto end_b = IPCChannelEnd::makeB(std::move(child_res));

	// Simple round-trip across the same process but through real shm
	const char *msg = "hello shm";
	end_a.send(msg, 10);
	end_b.recv();
	UASSERTEQ(size_t, end_b.getRecvSize(), 10);
	UASSERT(memcmp(end_b.getRecvData(), msg, 10) == 0);
#else
	// Fork a child that attaches to the shm and echoes back.
	auto parent_res = IPCChannelResourcesShm::makeFirst("luanti-test");
	UASSERT(parent_res != nullptr);
	std::string name = parent_res->getName();

	pid_t pid = fork();
	UASSERT(pid >= 0);
	if (pid == 0) {
		// Child
		auto child_res = IPCChannelResourcesShm::makeSecond(name);
		if (!child_res)
			_exit(1);
		auto end_b = IPCChannelEnd::makeB(std::move(child_res));

		// Echo loop, stop on empty message
		while (true) {
			if (!end_b.recvWithTimeout(5000))
				_exit(2);
			if (!end_b.sendWithTimeout(end_b.getRecvData(),
					end_b.getRecvSize(), 5000))
				_exit(3);
			if (end_b.getRecvSize() == 0)
				break;
		}
		_exit(0);
	}

	// Parent
	auto end_a = IPCChannelEnd::makeA(std::move(parent_res));

	// Round-trip a few messages of various sizes
	const char *msgs[] = {
		"hi",
		"medium length message for cross-process shm round-trip",
		"",
	};
	// Skip the empty one for now; we use it as the sentinel below
	for (int i = 0; i < 2; ++i) {
		size_t len = strlen(msgs[i]);
		UASSERT(end_a.exchangeWithTimeout(msgs[i], len, 5000));
		UASSERTEQ(size_t, end_a.getRecvSize(), len);
		UASSERT(memcmp(end_a.getRecvData(), msgs[i], len) == 0);
	}

	// Large message spanning multiple chunks
	std::vector<u8> big(IPC_CHANNEL_MSG_SIZE * 3 + 17);
	for (size_t i = 0; i < big.size(); ++i)
		big[i] = static_cast<u8>(i & 0xff);
	UASSERT(end_a.exchangeWithTimeout(big.data(), big.size(), 10000));
	UASSERTEQ(size_t, end_a.getRecvSize(), big.size());
	UASSERT(memcmp(end_a.getRecvData(), big.data(), big.size()) == 0);

	// Empty message to stop the child
	UASSERT(end_a.exchangeWithTimeout(nullptr, 0, 5000));

	// Wait for child
	int status = 0;
	UASSERTEQ(pid_t, waitpid(pid, &status, 0), pid);
	UASSERT(WIFEXITED(status));
	UASSERTEQ(int, WEXITSTATUS(status), 0);
#endif
}

#if CHECK_CLIENT_BUILD()
void TestThreading::testIPCChildProcess()
{
	// Smoke test: spawn the real luanti binary as an SSCSM worker, speak
	// the SSCSM protocol to it, and verify a clean teardown.
	auto [end_a, child] = make_child_ipc_channel();
	UASSERT(child != nullptr);
	UASSERT(child->isValid());

	// The worker's first message is always a PollNextEvent request.
	UASSERT(end_a.recvWithTimeout(5000));
	std::string first_req(
			static_cast<const char *>(end_a.getRecvData()),
			end_a.getRecvSize());
	auto req = deserializeSSCSMRequest(first_req);
	UASSERT(req->getType() == SSCSMRequestType::PollNextEvent);

	// Answer with TearDown so the worker exits cleanly.
	SSCSMRequestPollNextEvent::Answer ans;
	ans.next_event = std::make_unique<SSCSMEventTearDown>();
	auto ans_bytes = serializeSSCSMAnswer(std::move(ans));
	UASSERT(end_a.sendWithTimeout(ans_bytes.data(), ans_bytes.size(), 5000));

	// Wait for clean exit.
	UASSERT(child->waitWithTimeout(5000));
	if (child->getExitCode() != 0) {
		// Surface signal info if the worker died by signal — without
		// this, "exit code -1" tells us nothing about why. Cross-
		// reference signal numbers via `kill -l` (e.g. 11=SEGV,
		// 31=SYS, 9=KILL).
		std::cerr << "testIPCChildProcess: worker exit="
				<< child->getExitCode()
				<< " term_signal=" << child->getTermSignal()
				<< std::endl;
	}
	UASSERTEQ(int, child->getExitCode(), 0);
}
#endif // CHECK_CLIENT_BUILD()
