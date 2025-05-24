/*
This file is a part of the JThread package, which contains some object-
oriented thread wrappers for different thread implementations.

Copyright (c) 2000-2006  Jori Liesenborgs (jori.liesenborgs@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "threading/thread.h"
#include "threading/mutex_auto_lock.h"
#include "log_internal.h"
#include "porting.h"

// for setName
#if defined(__linux__)
	#include <sys/prctl.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
	#include <pthread_np.h>
#elif defined(__NetBSD__)
	#include <sched.h>
#elif defined(_MSC_VER)
	struct THREADNAME_INFO {
		DWORD dwType;     // Must be 0x1000
		LPCSTR szName;    // Pointer to name (in user addr space)
		DWORD dwThreadID; // Thread ID (-1=caller thread)
		DWORD dwFlags;    // Reserved for future use, must be zero
	};
#endif

// for bindToProcessor
#if __FreeBSD_version >= 702106
	typedef cpuset_t cpu_set_t;
#elif defined(__sun) || defined(sun)
	#include <sys/types.h>
	#include <sys/processor.h>
	#include <sys/procset.h>
#elif defined(_AIX)
	#include <sys/processor.h>
	#include <sys/thread.h>
#elif defined(__APPLE__)
	#include <mach/mach_init.h>
	#include <mach/thread_act.h>
#endif

// See https://msdn.microsoft.com/en-us/library/hh920601.aspx#thread__native_handle_method
#define win32_native_handle() ((HANDLE) getThreadHandle())

thread_local Thread *current_thread = nullptr;


Thread::Thread(const std::string &name) :
	m_name(name),
	m_request_stop(false),
	m_running(false)
{
#ifdef _AIX
	m_kernel_thread_id = -1;
#endif
}


Thread::~Thread()
{
	// kill the thread if running
	if (!m_running) {
		wait();
	} else {

		m_running = false;

#if defined(_WIN32)
		TerminateThread(win32_native_handle(), 0);
		CloseHandle(win32_native_handle());
#else
		// We need to pthread_kill instead on Android since NDKv5's pthread
		// implementation is incomplete.
# ifdef __ANDROID__
		pthread_kill(getThreadHandle(), SIGKILL);
# else
		pthread_cancel(getThreadHandle());
# endif
		wait();
#endif
	}

	// Make sure start finished mutex is unlocked before it's destroyed
	if (m_start_finished_mutex.try_lock())
		m_start_finished_mutex.unlock();
}


bool Thread::start()
{
	MutexAutoLock lock(m_mutex);

	if (m_running)
		return false;

	m_request_stop = false;

	// The mutex may already be locked if the thread is being restarted
	// FIXME: what if this fails, or if already locked by same thread?
	std::unique_lock sf_lock(m_start_finished_mutex, std::try_to_lock);

	try {
		m_thread_obj = new std::thread(threadProc, this);
	} catch (const std::system_error &e) {
		return false;
	}

	while (!m_running)
		sleep_ms(1);

	// Allow spawned thread to continue
	sf_lock.unlock();

	m_joinable = true;

	return true;
}


bool Thread::stop()
{
	m_request_stop = true;
	return true;
}


bool Thread::wait()
{
	MutexAutoLock lock(m_mutex);

	if (!m_joinable)
		return false;


	m_thread_obj->join();

	delete m_thread_obj;
	m_thread_obj = nullptr;

	assert(m_running == false);
	m_joinable = false;
	return true;
}



bool Thread::getReturnValue(void **ret) const
{
	if (m_running)
		return false;

	*ret = m_retval;
	return true;
}


void Thread::threadProc(Thread *thr)
{
#ifdef _AIX
	thr->m_kernel_thread_id = thread_self();
#endif

	current_thread = thr;

	thr->setName(thr->m_name);

	g_logger.registerThread(thr->m_name);
	thr->m_running = true;

	// Wait for the thread that started this one to finish initializing the
	// thread handle so that getThreadId/getThreadHandle will work.
	std::unique_lock sf_lock(thr->m_start_finished_mutex);

	thr->m_retval = thr->run();

	thr->m_running = false;
	// Unlock m_start_finished_mutex to prevent data race condition on Windows.
	// On Windows with VS2017 build TerminateThread is called and this mutex is not
	// released. We try to unlock it from caller thread and it's refused by system.
	sf_lock.unlock();
	g_logger.deregisterThread();
}


Thread *Thread::getCurrentThread()
{
	return current_thread;
}


void Thread::setName(const std::string &name)
{
#if defined(__linux__)

	// It would be cleaner to do this with pthread_setname_np,
	// which was added to glibc in version 2.12, but some major
	// distributions are still runing 2.11 and previous versions.
	prctl(PR_SET_NAME, name.c_str());

#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)

	pthread_set_name_np(pthread_self(), name.c_str());

#elif defined(__NetBSD__)

	pthread_setname_np(pthread_self(), "%s", const_cast<char*>(name.c_str()));

#elif defined(__APPLE__)

	pthread_setname_np(name.c_str());

#elif defined(__HAIKU__)

	rename_thread(find_thread(NULL), name.c_str());

#elif defined(_MSC_VER)

	// Windows itself doesn't support thread names,
	// but the MSVC debugger does...
	THREADNAME_INFO info;

	info.dwType = 0x1000;
	info.szName = name.c_str();
	info.dwThreadID = -1;
	info.dwFlags = 0;

	__try {
		RaiseException(0x406D1388, 0,
			sizeof(info) / sizeof(DWORD), (ULONG_PTR *)&info);
	} __except (EXCEPTION_CONTINUE_EXECUTION) {
	}

#elif defined(_WIN32) || defined(__GNU__)

	// These platforms are known to not support thread names.
	// Silently ignore the request.

#else
	#warning "Unrecognized platform, thread names will not be available."
#endif
}


unsigned int Thread::getNumberOfProcessors()
{
	return std::thread::hardware_concurrency();
}


bool Thread::bindToProcessor(unsigned int proc_number)
{
#if defined(__ANDROID__)

	return false;

#elif defined(_WIN32)

	return SetThreadAffinityMask(win32_native_handle(), 1 << proc_number);

#elif __FreeBSD_version >= 702106 || defined(__linux__) || defined(__DragonFly__)

	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(proc_number, &cpuset);

	return pthread_setaffinity_np(getThreadHandle(), sizeof(cpuset), &cpuset) == 0;
#elif defined(__NetBSD__)

	cpuset_t *cpuset = cpuset_create();
	if (cpuset == NULL)
		return false;
	int r = pthread_setaffinity_np(getThreadHandle(), cpuset_size(cpuset), cpuset);
	cpuset_destroy(cpuset);
	return r == 0;
#elif defined(__sun) || defined(sun)

	return processor_bind(P_LWPID, P_MYID, proc_number, NULL) == 0

#elif defined(_AIX)

	return bindprocessor(BINDTHREAD, m_kernel_thread_id, proc_number) == 0;

#elif defined(__hpux) || defined(hpux)

	pthread_spu_t answer;

	return pthread_processor_bind_np(PTHREAD_BIND_ADVISORY_NP,
			&answer, proc_number, getThreadHandle()) == 0;

#elif defined(__APPLE__)

	struct thread_affinity_policy tapol;

	thread_port_t threadport = pthread_mach_thread_np(getThreadHandle());
	tapol.affinity_tag = proc_number + 1;
	return thread_policy_set(threadport, THREAD_AFFINITY_POLICY,
			(thread_policy_t)&tapol,
			THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;

#else

	return false;

#endif
}


bool Thread::setPriority(int prio)
{
#ifdef _WIN32

	return SetThreadPriority(win32_native_handle(), prio);

#else

	struct sched_param sparam;
	int policy;

	if (pthread_getschedparam(getThreadHandle(), &policy, &sparam) != 0)
		return false;

	int min = sched_get_priority_min(policy);
	int max = sched_get_priority_max(policy);

	sparam.sched_priority = min + prio * (max - min) / THREAD_PRIORITY_HIGHEST;
	return pthread_setschedparam(getThreadHandle(), policy, &sparam) == 0;

#endif
}

