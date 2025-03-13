// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <iostream>
#include <unordered_map>
#include <cassert>
#include "irrlichttypes.h"

typedef std::unordered_map<u16, std::string> IdToNameMap;
typedef std::unordered_map<std::string, u16> NameToIdMap;

class NameIdMapping
{
public:
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

	void clear()
	{
		m_id_to_name.clear();
		m_name_to_id.clear();
	}

	void set(u16 id, const std::string &name)
	{
		assert(!name.empty());
		m_id_to_name[id] = name;
		m_name_to_id[name] = id;
	}

	void removeId(u16 id)
	{
		std::string name;
		bool found = getName(id, name);
		if (!found)
			return;
		m_id_to_name.erase(id);
		m_name_to_id.erase(name);
	}

	void eraseName(const std::string &name)
	{
		u16 id;
		bool found = getId(name, id);
		if (!found)
			return;
		m_id_to_name.erase(id);
		m_name_to_id.erase(name);
	}
	bool getName(u16 id, std::string &result) const
	{
		auto i = m_id_to_name.find(id);
		if (i == m_id_to_name.end())
			return false;
		result = i->second;
		return true;
	}
	bool getId(const std::string &name, u16 &result) const
	{
		auto i = m_name_to_id.find(name);
		if (i == m_name_to_id.end())
			return false;
		result = i->second;
		return true;
	}
	u16 size() const { return m_id_to_name.size(); }

private:
	IdToNameMap m_id_to_name;
	NameToIdMap m_name_to_id;
};
