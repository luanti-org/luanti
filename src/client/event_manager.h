// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "mtevent.h"
#include <list>
#include <map>

class EventManager : public MtEventManager
{
	static void receiverReceive(MtEvent *e, void *data)
	{
		MtEventReceiver *r = (MtEventReceiver *)data;
		r->onEvent(e);
	}
	struct FuncSpec
	{
		event_receive_func f;
		void *d;
		FuncSpec(event_receive_func f, void *d) : f(f), d(d) {}
	};

	struct Dest
	{
		std::list<FuncSpec> funcs{};
	};
	std::map<MtEvent::Type, Dest> m_dest{};

public:
	~EventManager() override = default;

	void put(MtEvent *e) override
	{
		auto i = m_dest.find(e->getType());
		if (i != m_dest.end()) {
			std::list<FuncSpec> &funcs = i->second.funcs;
			for (FuncSpec &func : funcs) {
				(*(func.f))(e, func.d);
			}
		}
		delete e;
	}
	void reg(MtEvent::Type type, event_receive_func f, void *data) override
	{
		auto i = m_dest.find(type);
		if (i != m_dest.end()) {
			i->second.funcs.emplace_back(f, data);
		} else {
			Dest dest;
			dest.funcs.emplace_back(f, data);
			m_dest[type] = dest;
		}
	}
	void dereg(MtEvent::Type type, event_receive_func f, void *data) override
	{
		auto i = m_dest.find(type);
		if (i != m_dest.end()) {
			std::list<FuncSpec> &funcs = i->second.funcs;
			for (auto j = funcs.begin(); j != funcs.end(); ) {
				bool remove = (j->f == f && (!data || j->d == data));
				if (remove)
					j = funcs.erase(j);
				else
					++j;
			}
		}
	}
};
