// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier <sapier AT gmx DOT net>

#pragma once

#if defined(_WIN32)
#include <windows.h>
#elif defined(__MACH__) && defined(__APPLE__)
#include <mach/semaphore.h>
#else
#include <semaphore.h>
#endif

#include "util/basic_macros.h"

class Semaphore
{
public:
	Semaphore(int val = 0);
	~Semaphore();

	DISABLE_CLASS_COPY(Semaphore);

	void post(unsigned int num = 1);
	void wait();
	bool wait(unsigned int time_ms);

private:
#if defined(WIN32)
	HANDLE semaphore;
#elif defined(__MACH__) && defined(__APPLE__)
	semaphore_t semaphore;
#else
	sem_t semaphore;
#endif
};
