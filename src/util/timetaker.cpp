// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "timetaker.h"

#include "porting.h"
#include "log.h"
#include <ostream>

void TimeTaker::start()
{
	m_time1 = porting::getTime(m_precision);
}

u64 TimeTaker::stop(bool quiet)
{
	if (m_running) {
		u64 dtime = porting::getTime(m_precision) - m_time1;
		if (m_result != nullptr) {
			(*m_result) += dtime;
		} else {
			if (!quiet && !m_name.empty()) {
				infostream << m_name << " took "
					<< dtime << TimePrecision_units[m_precision] << std::endl;
			}
		}
		m_running = false;
		return dtime;
	}
	return 0;
}

u64 TimeTaker::getTimerTime()
{
	return porting::getTime(m_precision) - m_time1;
}

