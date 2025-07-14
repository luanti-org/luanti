// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Marius Spix <marius.spix@web.de>

#pragma once

#include "config.h"

#if USE_TKRZW

#include "database.h"
#include "tkrzw_dbm_hash.h"
#include "tkrzw_dbm_tree.h"

template<typename T>
class DatabaseTkrzwBase
{
protected:
	T *dbm = nullptr;
	template <class U> struct tag { };
	void openDBM(const std::string &file);
	~DatabaseTkrzwBase();
private:
	void openDBM(tag<tkrzw::HashDBM>, const std::string &file);
	void openDBM(tag<tkrzw::TreeDBM>, const std::string &file);
};

class MapDatabaseTkrzw : public MapDatabase, public DatabaseTkrzwBase<tkrzw::HashDBM>
{
public:
	MapDatabaseTkrzw(const std::string &file);
	~MapDatabaseTkrzw();

	void beginSave() override;
	void endSave() override;

	bool saveBlock(const v3s16 &pos, std::string_view data) override;
	void loadBlock(const v3s16 &pos, std::string *block) override;
	bool deleteBlock(const v3s16 &pos) override;
	void listAllLoadableBlocks(std::vector<v3s16> &dst) override;
};

class PlayerDatabaseTkrzw : public PlayerDatabase, public DatabaseTkrzwBase<tkrzw::HashDBM>
{
public:
	PlayerDatabaseTkrzw(const std::string &file);
	~PlayerDatabaseTkrzw();

	void savePlayer(RemotePlayer *player) override;
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao) override;
	bool removePlayer(const std::string &name) override;
	void listPlayers(std::vector<std::string> &res) override;
};

class AuthDatabaseTkrzw : public AuthDatabase, public DatabaseTkrzwBase<tkrzw::HashDBM>
{
public:
	AuthDatabaseTkrzw(const std::string &file);
	~AuthDatabaseTkrzw();

	bool getAuth(const std::string &name, AuthEntry &res) override;
	bool saveAuth(const AuthEntry &authEntry) override;
	bool createAuth(AuthEntry &authEntry) override;
	bool deleteAuth(const std::string &name) override;
	void listNames(std::vector<std::string> &res) override;
	void reload() override {}
};

class ModStorageDatabaseTkrzw : public ModStorageDatabase, public DatabaseTkrzwBase<tkrzw::TreeDBM>
{
public:
	ModStorageDatabaseTkrzw(const std::string &file);
	~ModStorageDatabaseTkrzw();

	void getModEntries(const std::string &modname, StringMap *storage) override;
	void getModKeys(const std::string &modname, std::vector<std::string> *storage) override;
	bool hasModEntry(const std::string &modname, const std::string &key) override;
	bool getModEntry(const std::string &modname, const std::string &key, std::string *value) override;
	bool setModEntry(const std::string &modname, const std::string &key, std::string_view value) override;
	bool removeModEntry(const std::string &modname, const std::string &key) override;
	bool removeModEntries(const std::string &modname) override;
	void listMods(std::vector<std::string> *res) override;
	void beginSave() {}
	void endSave() {}
};

#endif // USE_TKRZW

