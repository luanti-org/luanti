// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, int> ItemGroupList;

static inline int itemgroup_get(const ItemGroupList &groups, const std::string &name)
{
	ItemGroupList::const_iterator i = groups.find(name);
	if (i == groups.end())
		return 0;
	return i->second;
}
