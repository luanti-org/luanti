// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <map>
#include <set>
#include <string>
#include <mutex>
#include "database.h"
#include "irrlichttypes.h"

class Database_Dummy : public MapDatabase, public PlayerDatabase, public ModStorageDatabase
{
public:
	bool saveBlock(const v3s16 &pos, std::string_view data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

	void getModEntries(const std::string &modname, StringMap *storage);
	void getModKeys(const std::string &modname, std::vector<std::string> *storage);
	bool getModEntry(const std::string &modname,
			const std::string &key, std::string *value);
	bool hasModEntry(const std::string &modname, const std::string &key);
	bool setModEntry(const std::string &modname,
			const std::string &key, std::string_view value);
	bool removeModEntry(const std::string &modname, const std::string &key);
	bool removeModEntries(const std::string &modname);
	void listMods(std::vector<std::string> *res);

	void beginSave() {}
	void endSave() {}

private:
	// We could have used 3 mutexes, one per container, but as it's only for testing purposes, one mutex is enough.
	std::mutex m_mutex;
	std::map<s64, std::string> m_database;
	std::set<std::string> m_player_database;
	std::unordered_map<std::string, StringMap> m_mod_storage_database;
};

/*
 * Auth: kept here for completeness, declared alongside the other in-memory
 * backends.
 */
class AuthDatabaseDummy : public AuthDatabase
{
public:
	AuthDatabaseDummy() = default;
	virtual ~AuthDatabaseDummy() = default;

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

private:
	std::mutex m_mutex;
	std::unordered_map<std::string, AuthEntry> m_auth_entries;
	u64 m_next_id = 1;
};
