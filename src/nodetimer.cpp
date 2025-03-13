// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "nodetimer.h"
#include "log.h"
#include "serialization.h"
#include "util/serialize.h"
#include "constants.h" // MAP_BLOCKSIZE

/*
	NodeTimer
*/

void NodeTimer::serialize(std::ostream &os) const
{
	writeF1000(os, timeout);
	writeF1000(os, elapsed);
}

void NodeTimer::deSerialize(std::istream &is)
{
	timeout = readF1000(is);
	elapsed = readF1000(is);
}

/*
	NodeTimerList
*/

void NodeTimerList::serialize(std::ostream &os, u8 map_format_version) const
{
	if (map_format_version == 24) {
		// Version 0 is a placeholder for "nothing to see here; go away."
		if (m_timers.empty()) {
			writeU8(os, 0); // version
			return;
		}
		writeU8(os, 1); // version
		writeU16(os, m_timers.size());
	}

	if (map_format_version >= 25) {
		writeU8(os, 2 + 4 + 4); // length of the data for a single timer
		writeU16(os, m_timers.size());
	}

	for (const auto &timer : m_timers) {
		NodeTimer t = timer.second;
		NodeTimer nt = NodeTimer(t.timeout,
			t.timeout - (f32)(timer.first - m_time), t.position);
		v3s16 p = t.position;

		u16 p16 = p.Z * MAP_BLOCKSIZE * MAP_BLOCKSIZE + p.Y * MAP_BLOCKSIZE + p.X;
		writeU16(os, p16);
		nt.serialize(os);
	}
}

void NodeTimerList::deSerialize(std::istream &is, u8 map_format_version)
{
	clear();

	if (map_format_version == 24) {
		u8 timer_version = readU8(is);
		if(timer_version == 0)
			return;
		if(timer_version != 1)
			throw SerializationError("unsupported NodeTimerList version");
	}

	if (map_format_version >= 25) {
		u8 timer_data_len = readU8(is);
		if(timer_data_len != 2+4+4)
			throw SerializationError("unsupported NodeTimer data length");
	}

	u16 count = readU16(is);

	for (u16 i = 0; i < count; i++) {
		u16 p16 = readU16(is);

		v3s16 p;
		p.Z = p16 / MAP_BLOCKSIZE / MAP_BLOCKSIZE;
		p16 &= MAP_BLOCKSIZE * MAP_BLOCKSIZE - 1;
		p.Y = p16 / MAP_BLOCKSIZE;
		p16 &= MAP_BLOCKSIZE - 1;
		p.X = p16;

		NodeTimer t(p);
		t.deSerialize(is);

		if (t.timeout <= 0) {
			warningstream<<"NodeTimerList::deSerialize(): "
					<<"invalid data at position"
					<<"("<<p.X<<","<<p.Y<<","<<p.Z<<"): Ignoring."
					<<std::endl;
			continue;
		}

		if (m_iterators.find(p) != m_iterators.end()) {
			warningstream<<"NodeTimerList::deSerialize(): "
					<<"already set data at position"
					<<"("<<p.X<<","<<p.Y<<","<<p.Z<<"): Ignoring."
					<<std::endl;
			continue;
		}

		insert(t);
	}
}

std::vector<NodeTimer> NodeTimerList::step(float dtime)
{
	std::vector<NodeTimer> elapsed_timers;
	m_time += dtime;
	if (m_next_trigger_time == -1. || m_time < m_next_trigger_time) {
		return elapsed_timers;
	}
	auto i = m_timers.begin();
	// Process timers
	for (; i != m_timers.end() && i->first <= m_time; ++i) {
		NodeTimer t = i->second;
		t.elapsed = t.timeout + (f32)(m_time - i->first);
		elapsed_timers.push_back(t);
		m_iterators.erase(t.position);
	}
	// Delete elapsed timers
	m_timers.erase(m_timers.begin(), i);
	if (m_timers.empty())
		m_next_trigger_time = -1.;
	else
		m_next_trigger_time = m_timers.begin()->first;
	return elapsed_timers;
}
