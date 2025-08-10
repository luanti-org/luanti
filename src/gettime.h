// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <ctime>
#include <string>
#include <mutex>

inline struct tm mt_localtime()
{
	// initialize the time zone on first invocation
	static std::once_flag tz_init;
	std::call_once(tz_init, [] {
#ifdef _WIN32
		_tzset();
#else
		tzset();
#endif
		});

	struct tm ret{};
	time_t t = time(NULL);
	// TODO we should check if the function returns NULL, which would mean error
#ifdef _WIN32
	localtime_s(&ret, &t);
#else
	localtime_r(&t, &ret);
#endif
	return ret;
}


inline std::string getTimestamp()
{
	const struct tm tm = mt_localtime();
	char cs[20]; // YYYY-MM-DD HH:MM:SS + '\0'
	strftime(cs, 20, "%Y-%m-%d %H:%M:%S", &tm);
	return cs;
}
