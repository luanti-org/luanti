// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "mtevent.h"
#include <vector>
#include <cassert>

class EventManager : public MtEventManager
{
	struct FuncSpec
	{
		event_receive_func f;
		void *d;
		FuncSpec(event_receive_func f, void *d) : f(f), d(d) {}
	};

	struct Dest
	{
		std::vector<FuncSpec> funcs;
	};
	std::vector<Dest> m_dest;

public:
	EventManager() {
		m_dest.resize(MtEvent::Type::TYPE_MAX);
	}

	~EventManager() override = default;

	void put(MtEvent *e) override
	{
		const size_t type = e->getType();
		if (type >= MtEvent::Type::TYPE_MAX) {
			assert(false);
			return;
		}
		const auto &funcs = m_dest[type].funcs;
		for (const FuncSpec &func : funcs) {
			(*(func.f))(e, func.d);
		}
		delete e;
	}

	void reg(MtEvent::Type type, event_receive_func f, void *data) override
	{
		if (type >= MtEvent::Type::TYPE_MAX) {
			assert(false);
			return;
		}
		m_dest[type].funcs.emplace_back(f, data);
	}

	void dereg(MtEvent::Type type, event_receive_func f, void *data) override
	{
		if (type >= MtEvent::Type::TYPE_MAX) {
			assert(false);
			return;
		}
		auto &funcs = m_dest[type].funcs;
		for (auto j = funcs.begin(); j != funcs.end(); ) {
			bool remove = j->f == f && (!data || j->d == data);
			if (remove)
				j = funcs.erase(j);
			else
				++j;
		}
	}
};
