// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <libpq-fe.h>
#include "database.h"
#include "util/basic_macros.h"

class Settings;

class Database_PostgreSQL: public Database
{
public:
	Database_PostgreSQL(const std::string &connect_string, const char *type);
	~Database_PostgreSQL();

	void beginSave();
	void endSave();
	void rollback();

	bool initialized() const;


protected:
	// Conversion helpers
	inline int pg_to_int(PGresult *res, int row, int col)
	{
		return atoi(PQgetvalue(res, row, col));
	}

	inline u32 pg_to_uint(PGresult *res, int row, int col)
	{
		return (u32) atoi(PQgetvalue(res, row, col));
	}

	inline float pg_to_float(PGresult *res, int row, int col)
	{
		return (float) atof(PQgetvalue(res, row, col));
	}

	inline v3s16 pg_to_v3s16(PGresult *res, int row, int col)
	{
		return v3s16(
			pg_to_int(res, row, col),
			pg_to_int(res, row, col + 1),
			pg_to_int(res, row, col + 2)
		);
	}

	inline std::string pg_to_string(PGresult *res, int row, int col)
	{
		return std::string(PQgetvalue(res, row, col), PQgetlength(res, row, col));
	}

	inline PGresult *execPrepared(const char *stmtName, const int paramsNumber,
		const void **params,
		const int *paramsLengths = NULL, const int *paramsFormats = NULL,
		bool clear = true, bool nobinary = true)
	{
		return checkResults(PQexecPrepared(m_conn, stmtName, paramsNumber,
			(const char* const*) params, paramsLengths, paramsFormats,
			nobinary ? 1 : 0), clear);
	}

	inline PGresult *execPrepared(const char *stmtName, const int paramsNumber,
		const char **params, bool clear = true, bool nobinary = true)
	{
		return execPrepared(stmtName, paramsNumber,
			(const void **)params, NULL, NULL, clear, nobinary);
	}

	void createTableIfNotExists(const std::string &table_name, const std::string &definition);
	void verifyDatabase();

	// Database initialization
	void connectToDatabase();
	virtual void createDatabase() = 0;
	virtual void initStatements() = 0;
	inline void prepareStatement(const std::string &name, const std::string &sql)
	{
		checkResults(PQprepare(m_conn, name.c_str(), sql.c_str(), 0, NULL));
	}

	int getPGVersion() const { return m_pgversion; }

private:
	// Database connectivity checks
	void ping();

	// Database usage
	PGresult *checkResults(PGresult *res, bool clear = true);

	// Attributes
	std::string m_connect_string;
	PGconn *m_conn = nullptr;
	int m_pgversion = 0;
};

class MapDatabasePostgreSQL : private Database_PostgreSQL, public MapDatabase
{
public:
	MapDatabasePostgreSQL(const std::string &connect_string);
	virtual ~MapDatabasePostgreSQL() = default;

	bool saveBlock(const v3s16 &pos, std::string_view data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

	void beginSave() { Database_PostgreSQL::beginSave(); }
	void endSave() { Database_PostgreSQL::endSave(); }

protected:
	virtual void createDatabase();
	virtual void initStatements();
};

class PlayerDatabasePostgreSQL : private Database_PostgreSQL, public PlayerDatabase
{
public:
	PlayerDatabasePostgreSQL(const std::string &connect_string);
	virtual ~PlayerDatabasePostgreSQL() = default;

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

protected:
	virtual void createDatabase();
	virtual void initStatements();

private:
	bool playerDataExists(const std::string &playername);
};

class AuthDatabasePostgreSQL : private Database_PostgreSQL, public AuthDatabase
{
public:
	AuthDatabasePostgreSQL(const std::string &connect_string);
	virtual ~AuthDatabasePostgreSQL() = default;

	virtual void verifyDatabase() { Database_PostgreSQL::verifyDatabase(); }

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

protected:
	virtual void createDatabase();
	virtual void initStatements();

private:
	virtual void writePrivileges(const AuthEntry &authEntry);
};

class ModStorageDatabasePostgreSQL : private Database_PostgreSQL, public ModStorageDatabase
{
public:
	ModStorageDatabasePostgreSQL(const std::string &connect_string);
	~ModStorageDatabasePostgreSQL() = default;

	void getModEntries(const std::string &modname, StringMap *storage);
	void getModKeys(const std::string &modname, std::vector<std::string> *storage);
	bool getModEntry(const std::string &modname, const std::string &key, std::string *value);
	bool hasModEntry(const std::string &modname, const std::string &key);
	bool setModEntry(const std::string &modname,
			const std::string &key, std::string_view value);
	bool removeModEntry(const std::string &modname, const std::string &key);
	bool removeModEntries(const std::string &modname);
	void listMods(std::vector<std::string> *res);

	void beginSave() { Database_PostgreSQL::beginSave(); }
	void endSave() { Database_PostgreSQL::endSave(); }

protected:
	virtual void createDatabase();
	virtual void initStatements();
};
