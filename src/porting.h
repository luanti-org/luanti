// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

/*
	Random portability stuff
*/

#pragma once

#if (defined(__linux__) || defined(__GNU__)) && !defined(_GNU_SOURCE)
	#define _GNU_SOURCE
#endif

// Be mindful of what you include here!
#include <csignal>
#include <string>
#include "config.h"
#include "irrlichttypes.h" // u64
#include "debug.h"
#include "constants.h"
#include "util/timetaker.h" // TimePrecision

#ifdef _MSC_VER
	#define SWPRINTF_CHARSTRING L"%S"
#else
	#define SWPRINTF_CHARSTRING L"%s"
#endif

#ifdef _WIN32
	#include <windows.h>

	#define sleep_ms(x) Sleep(x)
	#define sleep_us(x) Sleep((x)/1000)

	#define SLEEP_ACCURACY_US 2000

	#define setenv(n,v,o) _putenv_s(n,v)
	#define unsetenv(n) _putenv_s(n,"")
#else
	#include <unistd.h>
	#include <cstdlib> // setenv

	#define SLEEP_ACCURACY_US 200

	#define sleep_ms(x) usleep((x)*1000)
	#define sleep_us(x) usleep(x)
#endif

#ifdef _MSC_VER
	#define strtok_r(x, y, z) strtok_s(x, y, z)
	#define strtof(x, y) (float)strtod(x, y)
	#define strtoll(x, y, z) _strtoi64(x, y, z)
	#define strtoull(x, y, z) _strtoui64(x, y, z)
	#define strcasecmp(x, y) stricmp(x, y)
	#define strncasecmp(x, y, n) strnicmp(x, y, n)
#endif

#if !HAVE_STRLCPY
	#define strlcpy(d, s, n) mystrlcpy(d, s, n)
#endif

#ifndef _WIN32 // POSIX
	#include <sys/time.h>
	#include <ctime>
	#if defined(__MACH__) && defined(__APPLE__)
		#include <TargetConditionals.h>
	#endif
#endif

namespace porting
{

/*
	Signal handler (grabs Ctrl-C on POSIX systems)
*/

void signal_handler_init();
// Returns a pointer to a bool.
// When the bool is true, program should quit.
[[nodiscard]] volatile std::sig_atomic_t *signal_handler_killstatus();

/*
	Path of static data directory.
*/
extern std::string path_share;

/*
	Directory for storing user data. Examples:
	Windows: "C:\Documents and Settings\user\Application Data\<PROJECT_NAME>"
	Linux: "~/.<PROJECT_NAME>"
	Mac: "~/Library/Application Support/<PROJECT_NAME>"
*/
extern std::string path_user;

/*
	Path to gettext locale files
*/
extern std::string path_locale;

/*
	Path to directory for storing caches.
*/
extern std::string path_cache;

/*
	Gets the path of our executable.
*/
[[nodiscard]]
bool getCurrentExecPath(char *buf, size_t len);

/*
	Concatenate subpath to path_share.
*/
[[nodiscard]]
std::string getDataPath(const char *subpath);

/*
	Initialize path_*.
*/
void initializePaths();

/*
	Return system information
	e.g. "Linux/3.12.7 x86_64"
*/
const std::string &get_sysinfo();


// Monotonic timer

#ifdef _WIN32 // Windows

extern double perf_freq;

inline u64 os_get_time(double mult)
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return static_cast<double>(t.QuadPart) / (perf_freq / mult);
}

// Resolution is <1us.
inline u64 getTimeS() { return os_get_time(1); }
inline u64 getTimeMs() { return os_get_time(1000); }
inline u64 getTimeUs() { return os_get_time(1000*1000); }
inline u64 getTimeNs() { return os_get_time(1000*1000*1000); }

#else // POSIX

inline void os_get_clock(struct timespec *ts)
{
#if defined(CLOCK_MONOTONIC_RAW)
	clock_gettime(CLOCK_MONOTONIC_RAW, ts);
#elif defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK > 0
	clock_gettime(CLOCK_MONOTONIC, ts);
#else
# if defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK == 0
	// zero means it might be supported at runtime
	if (clock_gettime(CLOCK_MONOTONIC, ts) == 0)
		return;
# endif
	struct timeval tv;
	gettimeofday(&tv, NULL);
	TIMEVAL_TO_TIMESPEC(&tv, ts);
#endif
}

inline u64 getTimeS()
{
	struct timespec ts;
	os_get_clock(&ts);
	return ts.tv_sec;
}

inline u64 getTimeMs()
{
	struct timespec ts;
	os_get_clock(&ts);
	return ((u64) ts.tv_sec) * 1000LL + ((u64) ts.tv_nsec) / 1000000LL;
}

inline u64 getTimeUs()
{
	struct timespec ts;
	os_get_clock(&ts);
	return ((u64) ts.tv_sec) * 1000000LL + ((u64) ts.tv_nsec) / 1000LL;
}

inline u64 getTimeNs()
{
	struct timespec ts;
	os_get_clock(&ts);
	return ((u64) ts.tv_sec) * 1000000000LL + ((u64) ts.tv_nsec);
}

#endif

inline u64 getTime(TimePrecision prec)
{
	switch (prec) {
	case PRECISION_SECONDS: return getTimeS();
	case PRECISION_MILLI:   return getTimeMs();
	case PRECISION_MICRO:   return getTimeUs();
	case PRECISION_NANO:    return getTimeNs();
	}
	FATAL_ERROR("Called getTime with invalid time precision");
}

/**
 * Delta calculation function arguments.
 * @param old_time_ms old time for delta calculation
 * @param new_time_ms new time for delta calculation
 * @return positive delta value
 */
inline u64 getDeltaMs(u64 old_time_ms, u64 new_time_ms)
{
	if (new_time_ms >= old_time_ms) {
		return (new_time_ms - old_time_ms);
	}

	return (old_time_ms - new_time_ms);
}

inline void preciseSleepUs(u64 sleep_time)
{
	if (sleep_time > 0)
	{
		u64 target_time = porting::getTimeUs() + sleep_time;
		if (sleep_time > SLEEP_ACCURACY_US)
			sleep_us(sleep_time - SLEEP_ACCURACY_US);

		// Busy-wait the remaining time to adjust for sleep inaccuracies
		// The target - now > 0 construct will handle overflow gracefully (even though it should
		// never happen)
		while ((s64)(target_time - porting::getTimeUs()) > 0) {}
	}
}

inline const char *getPlatformName()
{
	return
#if defined(ANDROID)
	"Android"
#elif defined(__linux__)
	"Linux"
#elif defined(_WIN32) || defined(_WIN64)
	"Windows"
#elif defined(__DragonFly__) || defined(__FreeBSD__) || \
		defined(__NetBSD__) || defined(__OpenBSD__)
	"BSD"
#elif defined(__APPLE__) && defined(__MACH__)
	#if TARGET_OS_MAC
		"OSX"
	#elif TARGET_OS_IPHONE
		"iOS"
	#else
		"Apple"
	#endif
#elif defined(_AIX)
	"AIX"
#elif defined(__hpux)
	"HP-UX"
#elif defined(__sun) || defined(sun)
	#if defined(__SVR4)
		"Solaris"
	#else
		"SunOS"
	#endif
#elif defined(__HAIKU__)
	"Haiku"
#elif defined(__CYGWIN__)
	"Cygwin"
#elif defined(__unix__) || defined(__unix)
	#if defined(_POSIX_VERSION)
		"POSIX"
	#else
		"Unix"
	#endif
#else
	"?"
#endif
	;
}

// Securely fills buffer with bytes from system's random source
[[nodiscard]] bool secure_rand_fill_buf(void *buf, size_t len);

// Call once near beginning of main function.
void osSpecificInit();

// This attaches to the parents process console, or creates a new one if it doesnt exist.
void attachOrCreateConsole();

#if HAVE_MALLOC_TRIM
/**
 * Call this after freeing bigger blocks of memory. Used on some platforms to
 * properly give memory back to the OS.
 * @param amount Number of bytes freed
*/
void TrackFreedMemory(size_t amount);

/**
 * Call this regularly from background threads. This performs the actual trimming
 * and is potentially slow.
 */
void TriggerMemoryTrim();
#else
static inline void TrackFreedMemory(size_t amount) { (void)amount; }
static inline void TriggerMemoryTrim() { (void)0; }
#endif

#ifdef _WIN32
// Quotes an argument for use in a CreateProcess() commandline (not cmd.exe!!)
[[nodiscard]] std::string QuoteArgv(const std::string &arg);

// Convert an error code (e.g. from GetLastError()) into a string.
[[nodiscard]] std::string ConvertError(DWORD error_code);
#endif

// snprintf wrapper
int mt_snprintf(char *buf, const size_t buf_size, const char *fmt, ...);

/**
 * Opens URL in default web browser
 *
 * Must begin with http:// or https://, and not contain any new lines
 *
 * @param url The URL
 * @return true on success, false on failure
 */
bool open_url(const std::string &url);

/**
 * Opens a directory in the default file manager
 *
 * The directory must exist.
 *
 * @param path Path to directory
 * @return true on success, false on failure
 */
bool open_directory(const std::string &path);

} // namespace porting

#ifdef __ANDROID__
#include "porting_android.h"
#endif
