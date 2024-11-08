// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "metadata.h"
#include "log.h"

/*
	IMetadata
*/

bool IMetadata::operator==(const IMetadata &other) const
{
	StringMap this_map_, other_map_;
	const StringMap &this_map = getStrings(&this_map_);
	const StringMap &other_map = other.getStrings(&other_map_);

	if (this_map.size() != other_map.size())
		return false;

	for (const auto &this_pair : this_map) {
		const auto &other_pair = other_map.find(this_pair.first);
		if (other_pair == other_map.cend() || other_pair->second != this_pair.second)
			return false;
	}

	return true;
}

const std::string &IMetadata::getString(const std::string &name, std::string *place,
		u16 recursion) const
{
	const std::string *raw = getStringRaw(name, place);
	if (!raw) {
		static const std::string empty_string = std::string("");
		return empty_string;
	}

	return resolveString(*raw, place, recursion, true);
}

bool IMetadata::getStringToRef(const std::string &name,
		std::string &str, u16 recursion) const
{
	const std::string *raw = getStringRaw(name, &str);
	if (!raw)
		return false;

	const std::string &resolved = resolveString(*raw, &str, recursion, true);
	if (&resolved != &str)
		str = resolved;
	return true;
}

const std::string &IMetadata::resolveString(const std::string &str, std::string *place,
		u16 recursion, bool deprecated) const
{
	if (recursion <= 1 && str_starts_with(str, "${") && str.back() == '}') {
		if (deprecated) {
			warningstream << "Deprecated use of recursive resolution syntax in metadata: ";
			safe_print_string(warningstream, str);
			warningstream << std::endl;
		}
		// It may be the case that &str == place, but that's fine.
		return getString(str.substr(2, str.length() - 3), place, recursion + 1);
	}

	return str;
}

/*
	SimpleMetadata
*/

void SimpleMetadata::clear()
{
	m_stringvars.clear();
	m_modified = true;
}

bool SimpleMetadata::empty() const
{
	return m_stringvars.empty();
}

size_t SimpleMetadata::size() const
{
	return m_stringvars.size();
}

bool SimpleMetadata::contains(const std::string &name) const
{
	return m_stringvars.find(name) != m_stringvars.end();
}

const StringMap &SimpleMetadata::getStrings(StringMap *) const
{
	return m_stringvars;
}

const std::vector<std::string> &SimpleMetadata::getKeys(std::vector<std::string> *place) const
{
	place->clear();
	place->reserve(m_stringvars.size());
	for (const auto &pair : m_stringvars)
		place->push_back(pair.first);
	return *place;
}

const std::string *SimpleMetadata::getStringRaw(const std::string &name, std::string *) const
{
	const auto found = m_stringvars.find(name);
	return found != m_stringvars.cend() ? &found->second : nullptr;
}

bool SimpleMetadata::setString(const std::string &name, std::string_view var)
{
	if (var.empty()) {
		if (m_stringvars.erase(name) == 0)
			return false;
	} else {
		StringMap::iterator it = m_stringvars.find(name);
		if (it != m_stringvars.end() && it->second == var)
			return false;
		m_stringvars[name].assign(var);
	}
	m_modified = true;
	return true;
}
