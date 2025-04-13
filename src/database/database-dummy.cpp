// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

/*
Dummy database class
*/

#include "database-dummy.h"
#include "remoteplayer.h"


bool Database_Dummy::saveBlock(const v3s16 &pos, std::string_view data)
{
	m_database[getBlockAsInteger(pos)] = data;
	return true;
}

void Database_Dummy::loadBlock(const v3s16 &pos, std::string *block)
{
	s64 i = getBlockAsInteger(pos);
	auto it = m_database.find(i);
	if (it == m_database.end()) {
		block->clear();
		return;
	}

	*block = it->second;
}

bool Database_Dummy::deleteBlock(const v3s16 &pos)
{
	return m_database.erase(getBlockAsInteger(pos)) > 0;
}

void Database_Dummy::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	dst.reserve(m_database.size());
	for (auto x = m_database.begin(); x != m_database.end(); ++x) {
		dst.push_back(getIntegerAsBlock(x->first));
	}
}

void Database_Dummy::savePlayer(RemotePlayer *player)
{
	m_player_database.insert(player->getName());
}

bool Database_Dummy::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	return m_player_database.find(player->getName()) != m_player_database.end();
}

bool Database_Dummy::removePlayer(const std::string &name)
{
	m_player_database.erase(name);
	return true;
}

void Database_Dummy::listPlayers(std::vector<std::string> &res)
{
	for (const auto &player : m_player_database) {
		res.emplace_back(player);
	}
}

void Database_Dummy::getModEntries(const std::string &modname, StringMap *storage)
{
	const auto mod_pair = m_mod_storage_database.find(modname);
	if (mod_pair != m_mod_storage_database.cend()) {
		for (const auto &pair : mod_pair->second) {
			(*storage)[pair.first] = pair.second;
		}
	}
}

void Database_Dummy::getModKeys(const std::string &modname, std::vector<std::string> *storage)
{
	const auto mod_pair = m_mod_storage_database.find(modname);
	if (mod_pair != m_mod_storage_database.cend()) {
		storage->reserve(storage->size() + mod_pair->second.size());
		for (const auto &pair : mod_pair->second)
			storage->push_back(pair.first);
	}
}

bool Database_Dummy::getModEntry(const std::string &modname,
	const std::string &key, std::string *value)
{
	auto mod_pair = m_mod_storage_database.find(modname);
	if (mod_pair == m_mod_storage_database.end())
		return false;
	const StringMap &meta = mod_pair->second;

	auto pair = meta.find(key);
	if (pair != meta.end()) {
		*value = pair->second;
		return true;
	}
	return false;
}

bool Database_Dummy::hasModEntry(const std::string &modname, const std::string &key)
{
	auto mod_pair = m_mod_storage_database.find(modname);
	if (mod_pair == m_mod_storage_database.end())
		return false;
	const StringMap &meta = mod_pair->second;

	return meta.find(key) != meta.cend();
}

bool Database_Dummy::setModEntry(const std::string &modname,
	const std::string &key, std::string_view value)
{
	auto mod_pair = m_mod_storage_database.find(modname);
	if (mod_pair == m_mod_storage_database.end()) {
		auto &map = m_mod_storage_database[modname];
		map[key] = value;
	} else {
		mod_pair->second[key] = value;
	}
	return true;
}

bool Database_Dummy::removeModEntry(const std::string &modname, const std::string &key)
{
	auto mod_pair = m_mod_storage_database.find(modname);
	if (mod_pair != m_mod_storage_database.end())
		return mod_pair->second.erase(key) > 0;
	return false;
}

bool Database_Dummy::removeModEntries(const std::string &modname)
{
	auto mod_pair = m_mod_storage_database.find(modname);
	if (mod_pair != m_mod_storage_database.end() && !mod_pair->second.empty()) {
		mod_pair->second.clear();
		return true;
	}
	return false;
}

void Database_Dummy::listMods(std::vector<std::string> *res)
{
	for (const auto &pair : m_mod_storage_database) {
		res->push_back(pair.first);
	}
}
