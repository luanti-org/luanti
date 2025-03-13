// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "quicktune.h"
#include "threading/mutex_auto_lock.h"
#include "util/string.h"

std::string QuicktuneValue::getString()
{
	switch(type){
	case QVT_NONE:
		return "(none)";
	case QVT_FLOAT:
		return ftos(value_QVT_FLOAT.current);
	case QVT_INT:
		return itos(value_QVT_INT.current);
	}
	return "<invalid type>";
}

void QuicktuneValue::relativeAdd(float amount)
{
	switch(type){
	case QVT_NONE:
		break;
	case QVT_FLOAT: {
		float &v = value_QVT_FLOAT.current;
		v += amount * (value_QVT_FLOAT.max - value_QVT_FLOAT.min);
		v = core::clamp(v, value_QVT_FLOAT.min, value_QVT_FLOAT.max);
		break;
	}
	case QVT_INT: {
		int &v = value_QVT_INT.current;
		int diff = std::floor(amount * (value_QVT_INT.max - value_QVT_INT.min));
		if (!diff)
			diff = amount < 0 ? -1 : 1;
		v = core::clamp(v + diff, value_QVT_INT.min, value_QVT_INT.max);
		break;
	}
	}
}

static std::map<std::string, QuicktuneValue> g_values;
static std::vector<std::string> g_names;
static std::mutex g_mutex;

const std::vector<std::string> &getQuicktuneNames()
{
	return g_names;
}

QuicktuneValue getQuicktuneValue(const std::string &name)
{
	MutexAutoLock lock(g_mutex);
	auto i = g_values.find(name);
	if (i == g_values.end())
		return QuicktuneValue();
	return i->second;
}

void setQuicktuneValue(const std::string &name, const QuicktuneValue &val)
{
	MutexAutoLock lock(g_mutex);
	g_values[name] = val;
	g_values[name].modified = true;
}

void updateQuicktuneValue(const std::string &name, QuicktuneValue &val)
{
	MutexAutoLock lock(g_mutex);
	auto i = g_values.find(name);
	if (i == g_values.end()) {
		g_values[name] = val;
		g_names.push_back(name);
		return;
	}
	QuicktuneValue &ref = i->second;
	if (ref.modified) {
		val = ref;
	} else {
		ref = val;
		ref.modified = false;
	}
}

