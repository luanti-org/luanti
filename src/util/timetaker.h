// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include "irrlichttypes.h"

enum TimePrecision : s8
{
	PRECISION_SECONDS,
	PRECISION_MILLI,
	PRECISION_MICRO,
	PRECISION_NANO,
};

constexpr const char *TimePrecision_units[] = {
	"s"  /* PRECISION_SECONDS */,
	"ms" /* PRECISION_MILLI */,
	"us" /* PRECISION_MICRO */,
	"ns" /* PRECISION_NANO */,
};

/*
	TimeTaker
*/

// Note: this class should be kept lightweight

class TimeTaker
{
public:
	TimeTaker(const std::string &name, u64 *result = nullptr,
		TimePrecision prec = PRECISION_MILLI)
	{
		if (result)
			m_result = result;
		else
			m_name = name;
		m_precision = prec;
		start();
	}

	~TimeTaker()
	{
		stop();
	}

	u64 stop(bool quiet=false);

	u64 getTimerTime();

private:
	void start();

	std::string m_name;
	u64 *m_result = nullptr;
	u64 m_time1;
	bool m_running = true;
	TimePrecision m_precision;
};
