// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <cassert>
#include <string>
#include <map>
#include <ostream>

#include "threading/mutex_auto_lock.h"
#include "util/timetaker.h"
#include "util/numeric.h"      // paging()
/* FIXME: ^ move this to the .cpp file, it's not needed here */
#include "util/basic_macros.h"

// Global profiler
class Profiler;
extern Profiler *g_profiler;

/*
	Time profiler
*/

class Profiler
{
public:
	Profiler();

	void add(const std::string &name, float value);
	void avg(const std::string &name, float value);
	void max(const std::string &name, float value);
	void clear();

	float getValue(const std::string &name) const;
	int getAvgCount(const std::string &name) const;
	u64 getElapsedMs() const;

	typedef std::map<std::string, float> GraphValues;

	// Returns the line count
	int print(std::ostream &o, u32 page = 1, u32 pagecount = 1);
	void getPage(GraphValues &o, u32 page, u32 pagecount);


	void graphSet(const std::string &id, float value)
	{
		MutexAutoLock lock(m_mutex);
		m_graphvalues[id] = value;
	}
	void graphAdd(const std::string &id, float value)
	{
		MutexAutoLock lock(m_mutex);
		auto it = m_graphvalues.find(id);
		if (it == m_graphvalues.end())
			m_graphvalues.emplace(id, value);
		else
			it->second += value;
	}
	void graphPop(GraphValues &result)
	{
		MutexAutoLock lock(m_mutex);
		assert(result.empty());
		std::swap(result, m_graphvalues);
	}

	void remove(const std::string& name)
	{
		MutexAutoLock lock(m_mutex);
		m_data.erase(name);
	}

private:
	struct DataPair {
		float value = 0;
		int avgcount = 0;

		inline void reset() {
			value = 0;
			// negative values are used for type checking, so leave them alone
			if (avgcount >= 1)
				avgcount = 0;
		}
		inline float getValue() const {
			return avgcount >= 1 ? (value / avgcount) : value;
		}
	};

	std::mutex m_mutex;
	std::map<std::string, DataPair> m_data;
	std::map<std::string, float> m_graphvalues;
	u64 m_start_time;
};

enum ScopeProfilerType : u8
{
	SPT_ADD = 1,
	SPT_AVG,
	SPT_GRAPH_ADD,
	SPT_MAX
};

// Note: this class should be kept lightweight.

class ScopeProfiler
{
public:
	ScopeProfiler(Profiler *profiler, const std::string &name,
			ScopeProfilerType type = SPT_ADD,
			TimePrecision precision = PRECISION_MILLI);
	inline ~ScopeProfiler() { stop(); }

	// End profiled scope early
	void stop() noexcept;

	DISABLE_CLASS_COPY(ScopeProfiler)

private:
	Profiler *m_profiler = nullptr;
	std::string m_name;
	u64 m_time1;
	ScopeProfilerType m_type;
	TimePrecision m_precision;
};
