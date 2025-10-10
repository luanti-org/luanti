// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "os.h"
#include "irrString.h"
#include "irrMath.h"

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#include <SDL_endian.h>
#define bswap_16(X) SDL_Swap16(X)
#define bswap_32(X) SDL_Swap32(X)
#define bswap_64(X) SDL_Swap64(X)
#elif defined(_IRR_WINDOWS_API_) && defined(_MSC_VER)
#include <cstdlib>
#define bswap_16(X) _byteswap_ushort(X)
#define bswap_32(X) _byteswap_ulong(X)
#define bswap_64(X) _byteswap_uint64(X)
#elif defined(_IRR_OSX_PLATFORM_)
#include <libkern/OSByteOrder.h>
#define bswap_16(X) OSReadSwapInt16(&X, 0)
#define bswap_32(X) OSReadSwapInt32(&X, 0)
#define bswap_64(X) OSReadSwapInt64(&X, 0)
#elif defined(__FreeBSD__)
#include <sys/endian.h>
#define bswap_16(X) bswap16(X)
#define bswap_32(X) bswap32(X)
#define bswap_64(X) bswap64(X)
#elif defined(__OpenBSD__)
#include <endian.h>
#define bswap_16(X) letoh16(X)
#define bswap_32(X) letoh32(X)
#define bswap_64(X) letoh64(X)
#elif !defined(_IRR_SOLARIS_PLATFORM_) && !defined(__PPC__) && !defined(_IRR_WINDOWS_API_)
#include <byteswap.h>
#else
#define bswap_16(X) ((((X) & 0xFF) << 8) | (((X) & 0xFF00) >> 8))
#define bswap_32(X) ((((X) & 0x000000FF) << 24) | (((X) & 0xFF000000) >> 24) | (((X) & 0x0000FF00) << 8) | (((X) & 0x00FF0000) >> 8))
#define bswap_64(X) ((((X) & 0x00000000000000FF) << 56) | (((X) & 0xFF00000000000000) >> 56) | (((X) & 0x000000000000FF00) << 40) | (((X) & 0x00FF000000000000) >> 40) | (((X) & 0x0000000000FF0000) << 24) | (((X) & 0x0000FF0000000000) >> 24) | (((X) & 0x00000000FF000000) << 8) | (((X) & 0x000000FF00000000) >> 8))
#endif

namespace os
{
u16 Byteswap::byteswap(u16 num)
{
	return bswap_16(num);
}
s16 Byteswap::byteswap(s16 num)
{
	return bswap_16(num);
}
u32 Byteswap::byteswap(u32 num)
{
	return bswap_32(num);
}
s32 Byteswap::byteswap(s32 num)
{
	return bswap_32(num);
}
u64 Byteswap::byteswap(u64 num)
{
	return bswap_64(num);
}
s64 Byteswap::byteswap(s64 num)
{
	return bswap_64(num);
}
f32 Byteswap::byteswap(f32 num)
{
	u32 tmp = IR(num);
	tmp = bswap_32(tmp);
	return (FR(tmp));
}
}

#if defined(_IRR_WINDOWS_API_)
// ----------------------------------------------------------------
// Windows specific functions
// ----------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctime>

namespace os
{
//! prints a debuginfo string
void Printer::print(const c8 *message, ELOG_LEVEL ll)
{
	core::stringc tmp(message);
	tmp += "\n";
	OutputDebugStringA(tmp.c_str());
	printf("%s", tmp.c_str());
}

static LARGE_INTEGER HighPerformanceFreq;
static BOOL HighPerformanceTimerSupport = FALSE;

void Timer::initTimer()
{
	HighPerformanceTimerSupport = QueryPerformanceFrequency(&HighPerformanceFreq);
	initVirtualTimer();
}

u32 Timer::getRealTime()
{
	if (HighPerformanceTimerSupport) {
		LARGE_INTEGER nTime;
		BOOL queriedOK = QueryPerformanceCounter(&nTime);

		if (queriedOK)
			return u32((nTime.QuadPart) * 1000 / HighPerformanceFreq.QuadPart);
	}

	return GetTickCount();
}

} // end namespace os

#elif defined(_IRR_ANDROID_PLATFORM_)

// ----------------------------------------------------------------
// Android version
// ----------------------------------------------------------------

#include <android/log.h>

namespace os
{

//! prints a debuginfo string
void Printer::print(const c8 *message, ELOG_LEVEL ll)
{
	android_LogPriority LogLevel = ANDROID_LOG_UNKNOWN;

	switch (ll) {
	case ELL_DEBUG:
		LogLevel = ANDROID_LOG_DEBUG;
		break;
	case ELL_INFORMATION:
		LogLevel = ANDROID_LOG_INFO;
		break;
	case ELL_WARNING:
		LogLevel = ANDROID_LOG_WARN;
		break;
	case ELL_ERROR:
		LogLevel = ANDROID_LOG_ERROR;
		break;
	default: // ELL_NONE
		LogLevel = ANDROID_LOG_VERBOSE;
		break;
	}

	// Android logcat restricts log-output and cuts the rest of the message away. But we want it all.
	// On my device max-len is 1023 (+ 0 byte). Some websites claim a limit of 4096 so maybe different numbers on different devices.
	constexpr size_t maxLogLen = 1023;
	size_t msgLen = strlen(message);
	size_t start = 0;
	while (msgLen - start > maxLogLen) {
		__android_log_print(LogLevel, "Irrlicht", "%.*s\n",
				static_cast<int>(maxLogLen), &message[start]);
		start += maxLogLen;
	}
	__android_log_print(LogLevel, "Irrlicht", "%s\n", &message[start]);
}

void Timer::initTimer()
{
	initVirtualTimer();
}

u32 Timer::getRealTime()
{
	timeval tv;
	gettimeofday(&tv, 0);
	return (u32)(tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}
} // end namespace os

#elif defined(_IRR_EMSCRIPTEN_PLATFORM_)

// ----------------------------------------------------------------
// emscripten version
// ----------------------------------------------------------------

#include <emscripten.h>
#include <ctime>
#include <sys/time.h>

namespace os
{

//! prints a debuginfo string
void Printer::print(const c8 *message, ELOG_LEVEL ll)
{
	int log_level;
	switch (ll) {
	case ELL_DEBUG:
		log_level = 0;
		break;
	case ELL_INFORMATION:
		log_level = 0;
		break;
	case ELL_WARNING:
		log_level = EM_LOG_WARN;
		break;
	case ELL_ERROR:
		log_level = EM_LOG_ERROR;
		break;
	default: // ELL_NONE
		log_level = 0;
		break;
	}
	emscripten_log(log_level, "%s", message); // Note: not adding \n as emscripten_log seems to do that already.
}

void Timer::initTimer()
{
	initVirtualTimer();
}

u32 Timer::getRealTime()
{
	double time = emscripten_get_now();
	return (u32)(time);
}
} // end namespace os

#else

// ----------------------------------------------------------------
// linux/ansi version
// ----------------------------------------------------------------

#include <cstdio>
#include <ctime>
#include <sys/time.h>

namespace os
{

//! prints a debuginfo string
void Printer::print(const c8 *message, ELOG_LEVEL ll)
{
	printf("%s\n", message);
}

void Timer::initTimer()
{
	initVirtualTimer();
}

u32 Timer::getRealTime()
{
	timeval tv;
	gettimeofday(&tv, 0);
	return (u32)(tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}
} // end namespace os

#endif // end linux / emscripten / android / windows

namespace os
{
// The platform independent implementation of the printer
ILogger *Printer::Logger = 0;

void Printer::log(const c8 *message, ELOG_LEVEL ll)
{
	if (Logger)
		Logger->log(message, ll);
}

void Printer::log(const c8 *message, const c8 *hint, ELOG_LEVEL ll)
{
	if (Logger)
		Logger->log(message, hint, ll);
}

void Printer::log(const c8 *message, const io::path &hint, ELOG_LEVEL ll)
{
	if (Logger)
		Logger->log(message, hint.c_str(), ll);
}

// ------------------------------------------------------
// virtual timer implementation

f32 Timer::VirtualTimerSpeed = 1.0f;
s32 Timer::VirtualTimerStopCounter = 0;
u32 Timer::LastVirtualTime = 0;
u32 Timer::StartRealTime = 0;
u32 Timer::StaticTime = 0;

//! returns current virtual time
u32 Timer::getTime()
{
	if (isStopped())
		return LastVirtualTime;

	return LastVirtualTime + (u32)((StaticTime - StartRealTime) * VirtualTimerSpeed);
}

//! ticks, advances the virtual timer
void Timer::tick()
{
	StaticTime = getRealTime();
}

//! sets the current virtual time
void Timer::setTime(u32 time)
{
	StaticTime = getRealTime();
	LastVirtualTime = time;
	StartRealTime = StaticTime;
}

//! stops the virtual timer
void Timer::stopTimer()
{
	if (!isStopped()) {
		// stop the virtual timer
		LastVirtualTime = getTime();
	}

	--VirtualTimerStopCounter;
}

//! starts the virtual timer
void Timer::startTimer()
{
	++VirtualTimerStopCounter;

	if (!isStopped()) {
		// restart virtual timer
		setTime(LastVirtualTime);
	}
}

//! sets the speed of the virtual timer
void Timer::setSpeed(f32 speed)
{
	setTime(getTime());

	VirtualTimerSpeed = speed;
	if (VirtualTimerSpeed < 0.0f)
		VirtualTimerSpeed = 0.0f;
}

//! gets the speed of the virtual timer
f32 Timer::getSpeed()
{
	return VirtualTimerSpeed;
}

//! returns if the timer currently is stopped
bool Timer::isStopped()
{
	return VirtualTimerStopCounter < 0;
}

void Timer::initVirtualTimer()
{
	StaticTime = getRealTime();
	StartRealTime = StaticTime;
}

} // end namespace os
