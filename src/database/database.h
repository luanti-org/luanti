// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "util/string.h"

class Database
{
public:
	virtual void beginSave() = 0;
	virtual void endSave() = 0;

	/// @return true if database connection is open
	virtual bool initialized() const { return true; }

	/// Open and initialize the database if needed
	virtual void verifyDatabase() {};
};

class MapDatabase : public Database
{
public:
	virtual ~MapDatabase() = default;

	virtual bool saveBlock(const v3s16 &pos, std::string_view data) = 0;
	virtual void loadBlock(const v3s16 &pos, std::string *block) = 0;
	virtual bool deleteBlock(const v3s16 &pos) = 0;

	static s64 getBlockAsInteger(const v3s16 &pos);
	static v3s16 getIntegerAsBlock(s64 i);

	virtual void listAllLoadableBlocks(std::vector<v3s16> &dst) = 0;
};

class PlayerSAO;
class RemotePlayer;

class PlayerDatabase
{
public:
	virtual ~PlayerDatabase() = default;

	virtual void savePlayer(RemotePlayer *player) = 0;
	virtual bool loadPlayer(RemotePlayer *player, PlayerSAO *sao) = 0;
	virtual bool removePlayer(const std::string &name) = 0;
	virtual void listPlayers(std::vector<std::string> &res) = 0;
};

struct AuthEntry
{
	u64 id;
	std::string name;
	std::string password;
	std::vector<std::string> privileges;
	s64 last_login;
};

class AuthDatabase
{
public:
	virtual ~AuthDatabase() = default;

	virtual bool getAuth(const std::string &name, AuthEntry &res) = 0;
	virtual bool saveAuth(const AuthEntry &authEntry) = 0;
	virtual bool createAuth(AuthEntry &authEntry) = 0;
	virtual bool deleteAuth(const std::string &name) = 0;
	virtual void listNames(std::vector<std::string> &res) = 0;
	virtual void reload() = 0;
};

class ModStorageDatabase : public Database
{
public:
	virtual ~ModStorageDatabase() = default;

	virtual void getModEntries(const std::string &modname, StringMap *storage) = 0;
	virtual void getModKeys(const std::string &modname, std::vector<std::string> *storage) = 0;
	virtual bool hasModEntry(const std::string &modname, const std::string &key) = 0;
	virtual bool getModEntry(const std::string &modname,
		const std::string &key, std::string *value) = 0;
	virtual bool setModEntry(const std::string &modname,
		const std::string &key, std::string_view value) = 0;
	virtual bool removeModEntry(const std::string &modname, const std::string &key) = 0;
	virtual bool removeModEntries(const std::string &modname) = 0;
	virtual void listMods(std::vector<std::string> *res) = 0;
};
