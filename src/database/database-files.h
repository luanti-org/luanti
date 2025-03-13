// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

// !!! WARNING !!!
// This backend is intended to be used on Minetest 0.4.16 only for the transition backend
// for player files

#include "database.h"
#include <unordered_map>
#include <unordered_set>
#include <json/json.h> // for Json::Value

class PlayerDatabaseFiles : public PlayerDatabase
{
public:
	PlayerDatabaseFiles(const std::string &savedir);
	virtual ~PlayerDatabaseFiles() = default;

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

private:
	void deSerialize(RemotePlayer *p, std::istream &is, const std::string &playername,
			PlayerSAO *sao);
	/*
		serialize() writes a bunch of text that can contain
		any characters except a '\0', and such an ending that
		deSerialize stops reading exactly at the right point.
	*/
	void serialize(RemotePlayer *p, std::ostream &os);

	std::string m_savedir;
};

class AuthDatabaseFiles : public AuthDatabase
{
public:
	AuthDatabaseFiles(const std::string &savedir);
	virtual ~AuthDatabaseFiles() = default;

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

private:
	std::unordered_map<std::string, AuthEntry> m_auth_list;
	std::string m_savedir;
	bool readAuthFile();
	bool writeAuthFile();
};

class ModStorageDatabaseFiles : public ModStorageDatabase
{
public:
	ModStorageDatabaseFiles(const std::string &savedir);
	virtual ~ModStorageDatabaseFiles() = default;

	virtual void getModEntries(const std::string &modname, StringMap *storage);
	virtual void getModKeys(const std::string &modname, std::vector<std::string> *storage);
	virtual bool getModEntry(const std::string &modname,
		const std::string &key, std::string *value);
	virtual bool hasModEntry(const std::string &modname, const std::string &key);
	virtual bool setModEntry(const std::string &modname,
		const std::string &key, std::string_view value);
	virtual bool removeModEntry(const std::string &modname, const std::string &key);
	virtual bool removeModEntries(const std::string &modname);
	virtual void listMods(std::vector<std::string> *res);

	virtual void beginSave();
	virtual void endSave();

private:
	Json::Value *getOrCreateJson(const std::string &modname);

	std::string m_storage_dir;
	std::unordered_map<std::string, Json::Value> m_mod_storage;
	std::unordered_set<std::string> m_modified;
};
