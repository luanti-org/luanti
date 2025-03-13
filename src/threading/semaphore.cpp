// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier <sapier AT gmx DOT net>

#include "threading/semaphore.h"

#include <iostream>
#include <cstdlib>
#include <cassert>

#define UNUSED(expr) do { (void)(expr); } while (0)

#ifdef _WIN32
	#include <climits>
	#define MAX_SEMAPHORE_COUNT LONG_MAX - 1
#else
	#include <cerrno>
	#include <sys/time.h>
	#include <pthread.h>
	#if defined(__MACH__) && defined(__APPLE__)
		#include <mach/mach.h>
		#include <mach/task.h>
		#include <mach/semaphore.h>
		#include <sys/semaphore.h>
		#include <unistd.h>

		#undef sem_t
		#undef sem_init
		#undef sem_wait
		#undef sem_post
		#undef sem_destroy
		#define sem_t             semaphore_t
		#define sem_init(s, p, c) semaphore_create(mach_task_self(), (s), 0, (c))
		#define sem_wait(s)       semaphore_wait(*(s))
		#define sem_post(s)       semaphore_signal(*(s))
		#define sem_destroy(s)    semaphore_destroy(mach_task_self(), *(s))
	#endif
#endif


Semaphore::Semaphore(int val)
{
#ifdef _WIN32
	semaphore = CreateSemaphore(NULL, val, MAX_SEMAPHORE_COUNT, NULL);
#else
	int ret = sem_init(&semaphore, 0, val);
	assert(!ret);
	UNUSED(ret);
#endif
}


Semaphore::~Semaphore()
{
#ifdef _WIN32
	CloseHandle(semaphore);
#else
	int ret = sem_destroy(&semaphore);
#ifdef __ANDROID__
	// Workaround for broken bionic semaphore implementation!
	assert(!ret || errno == EBUSY);
#else
	assert(!ret);
#endif
	UNUSED(ret);
#endif
}


void Semaphore::post(unsigned int num)
{
	assert(num > 0);
#ifdef _WIN32
	ReleaseSemaphore(semaphore, num, NULL);
#else
	for (unsigned i = 0; i < num; i++) {
		int ret = sem_post(&semaphore);
		assert(!ret);
		UNUSED(ret);
	}
#endif
}


void Semaphore::wait()
{
#ifdef _WIN32
	WaitForSingleObject(semaphore, INFINITE);
#else
	int ret = sem_wait(&semaphore);
	assert(!ret);
	UNUSED(ret);
#endif
}


bool Semaphore::wait(unsigned int time_ms)
{
#ifdef _WIN32
	unsigned int ret = WaitForSingleObject(semaphore, time_ms);

	if (ret == WAIT_OBJECT_0) {
		return true;
	} else {
		assert(ret == WAIT_TIMEOUT);
		return false;
	}
#else
# if defined(__MACH__) && defined(__APPLE__)
	mach_timespec_t wait_time;
	wait_time.tv_sec = time_ms / 1000;
	wait_time.tv_nsec = 1000000 * (time_ms % 1000);

	errno = 0;
	int ret = semaphore_timedwait(semaphore, wait_time);
	switch (ret) {
	case KERN_OPERATION_TIMED_OUT:
		errno = ETIMEDOUT;
		break;
	case KERN_ABORTED:
		errno = EINTR;
		break;
	default:
		if (ret)
			errno = EINVAL;
	}
# else
	int ret;
	if (time_ms > 0) {
		struct timespec wait_time;
		struct timeval now;

		if (gettimeofday(&now, NULL) == -1) {
			std::cerr << "Semaphore::wait(ms): Unable to get time with gettimeofday!" << std::endl;
			abort();
		}

		wait_time.tv_nsec = ((time_ms % 1000) * 1000 * 1000) + (now.tv_usec * 1000);
		wait_time.tv_sec  = (time_ms / 1000) + (wait_time.tv_nsec / (1000 * 1000 * 1000)) + now.tv_sec;
		wait_time.tv_nsec %= 1000 * 1000 * 1000;

		ret = sem_timedwait(&semaphore, &wait_time);
	} else {
		ret = sem_trywait(&semaphore);
	}
# endif

	assert(!ret || (errno == ETIMEDOUT || errno == EINTR || errno == EAGAIN));
	return !ret;
#endif
}

