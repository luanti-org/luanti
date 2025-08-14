// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <cstring>
#include <string>
#include "database.h"
#include "exceptions.h"

extern "C" {
#include "sqlite3.h"
}

// Template class for SQLite3 based data storage
class Database_SQLite3 : public Database
{
public:
	virtual ~Database_SQLite3();

	void beginSave() override;
	void endSave() override;

	bool initialized() const override { return m_initialized; }

	/// @note not thread-safe
	void verifyDatabase() override;

protected:
	Database_SQLite3(const std::string &savedir, const std::string &dbname);

	// Check if a specific table exists
	bool checkTable(const char *table);

	// Check if a table has a specific column
	bool checkColumn(const char *table, const char *column);

	/* Value conversion helpers */

	inline void str_to_sqlite(sqlite3_stmt *s, int iCol, std::string_view str) const
	{
		sqlite3_vrfy(sqlite3_bind_text(s, iCol, str.data(), str.size(), NULL));
	}

	inline void blob_to_sqlite(sqlite3_stmt *s, int iCol, std::string_view str) const
	{
		sqlite3_vrfy(sqlite3_bind_blob(s, iCol, str.data(), str.size(), NULL));
	}

	inline void int_to_sqlite(sqlite3_stmt *s, int iCol, int val) const
	{
		sqlite3_vrfy(sqlite3_bind_int(s, iCol, val));
	}

	inline void int64_to_sqlite(sqlite3_stmt *s, int iCol, s64 val) const
	{
		sqlite3_vrfy(sqlite3_bind_int64(s, iCol, (sqlite3_int64) val));
	}

	inline void double_to_sqlite(sqlite3_stmt *s, int iCol, double val) const
	{
		sqlite3_vrfy(sqlite3_bind_double(s, iCol, val));
	}

	// Note that the return value is only valid until the statement is stepped or reset.
	inline std::string_view sqlite_to_string_view(sqlite3_stmt *s, int iCol)
	{
		const char* text = reinterpret_cast<const char*>(sqlite3_column_text(s, iCol));
		return text ? std::string_view(text) : std::string_view();
	}

	// Avoid using this in favor of `sqlite_to_string_view`.
	inline std::string sqlite_to_string(sqlite3_stmt *s, int iCol)
	{
		return std::string(sqlite_to_string_view(s, iCol));
	}

	// Converts a BLOB-type column into a string_view (null byte safe).
	// Note that the return value is only valid until the statement is stepped or reset.
	inline std::string_view sqlite_to_blob(sqlite3_stmt *s, int iCol)
	{
		const char *data = reinterpret_cast<const char*>(sqlite3_column_blob(s, iCol));
		if (!data)
			return std::string_view();
		size_t len = sqlite3_column_bytes(s, iCol);
		return std::string_view(data, len);
	}

	inline s32 sqlite_to_int(sqlite3_stmt *s, int iCol)
	{
		return sqlite3_column_int(s, iCol);
	}

	inline u32 sqlite_to_uint(sqlite3_stmt *s, int iCol)
	{
		return (u32) sqlite3_column_int(s, iCol);
	}

	inline s64 sqlite_to_int64(sqlite3_stmt *s, int iCol)
	{
		return (s64) sqlite3_column_int64(s, iCol);
	}

	inline u64 sqlite_to_uint64(sqlite3_stmt *s, int iCol)
	{
		return (u64) sqlite3_column_int64(s, iCol);
	}

	inline float sqlite_to_float(sqlite3_stmt *s, int iCol)
	{
		return (float) sqlite3_column_double(s, iCol);
	}

	inline const v3f sqlite_to_v3f(sqlite3_stmt *s, int iCol)
	{
		return v3f(sqlite_to_float(s, iCol), sqlite_to_float(s, iCol + 1),
				sqlite_to_float(s, iCol + 2));
	}

	// Helper for verifying result of sqlite3_step() and such
	inline void sqlite3_vrfy(int s, std::string_view m = "", int r = SQLITE_OK) const
	{
		if (s != r) {
			std::string msg(m);
			if (!msg.empty())
				msg.append(": ");
			msg.append(sqlite3_errmsg(m_database));
			throw DatabaseException(msg);
		}
	}

	inline void sqlite3_vrfy(const int s, const int r, std::string_view m = "") const
	{
		sqlite3_vrfy(s, m, r);
	}

	// Called after opening a fresh database file. Should create tables and indices.
	virtual void createDatabase() = 0;

	// Should prepare the necessary statements.
	virtual void initStatements() = 0;

	sqlite3 *m_database = nullptr;

private:
	// Open the database
	void openDatabase();

	bool m_initialized = false;

	const std::string m_savedir;
	const std::string m_dbname;

	sqlite3_stmt *m_stmt_begin = nullptr;
	sqlite3_stmt *m_stmt_end = nullptr;

	u64 m_busy_handler_data[2];

	static int busyHandler(void *data, int count);
};

// Not sure why why we have to do this. can't C++ figure it out on its own?
#define PARENT_CLASS_FUNCS \
	void beginSave() { Database_SQLite3::beginSave(); } \
	void endSave() { Database_SQLite3::endSave(); } \
	void verifyDatabase() { Database_SQLite3::verifyDatabase(); }

class MapDatabaseSQLite3 : private Database_SQLite3, public MapDatabase
{
public:
	MapDatabaseSQLite3(const std::string &savedir);
	virtual ~MapDatabaseSQLite3();

	bool saveBlock(const v3s16 &pos, std::string_view data);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);

	PARENT_CLASS_FUNCS

protected:
	virtual void createDatabase();
	virtual void initStatements();

private:
	/// @brief Bind block position into statement at column index
	/// @return index of next column after position
	int bindPos(sqlite3_stmt *stmt, v3s16 pos, int index = 1);

	bool m_new_format = false;

	sqlite3_stmt *m_stmt_read = nullptr;
	sqlite3_stmt *m_stmt_write = nullptr;
	sqlite3_stmt *m_stmt_list = nullptr;
	sqlite3_stmt *m_stmt_delete = nullptr;
};

class PlayerDatabaseSQLite3 : private Database_SQLite3, public PlayerDatabase
{
public:
	PlayerDatabaseSQLite3(const std::string &savedir);
	virtual ~PlayerDatabaseSQLite3();

	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

	PARENT_CLASS_FUNCS

protected:
	virtual void createDatabase();
	virtual void initStatements();

private:
	bool playerDataExists(const std::string &name);

	// Players
	sqlite3_stmt *m_stmt_player_load = nullptr;
	sqlite3_stmt *m_stmt_player_add = nullptr;
	sqlite3_stmt *m_stmt_player_update = nullptr;
	sqlite3_stmt *m_stmt_player_remove = nullptr;
	sqlite3_stmt *m_stmt_player_list = nullptr;
	sqlite3_stmt *m_stmt_player_load_inventory = nullptr;
	sqlite3_stmt *m_stmt_player_load_inventory_items = nullptr;
	sqlite3_stmt *m_stmt_player_add_inventory = nullptr;
	sqlite3_stmt *m_stmt_player_add_inventory_items = nullptr;
	sqlite3_stmt *m_stmt_player_remove_inventory = nullptr;
	sqlite3_stmt *m_stmt_player_remove_inventory_items = nullptr;
	sqlite3_stmt *m_stmt_player_metadata_load = nullptr;
	sqlite3_stmt *m_stmt_player_metadata_remove = nullptr;
	sqlite3_stmt *m_stmt_player_metadata_add = nullptr;
};

class AuthDatabaseSQLite3 : private Database_SQLite3, public AuthDatabase
{
public:
	AuthDatabaseSQLite3(const std::string &savedir);
	virtual ~AuthDatabaseSQLite3();

	virtual bool getAuth(const std::string &name, AuthEntry &res);
	virtual bool saveAuth(const AuthEntry &authEntry);
	virtual bool createAuth(AuthEntry &authEntry);
	virtual bool deleteAuth(const std::string &name);
	virtual void listNames(std::vector<std::string> &res);
	virtual void reload();

	PARENT_CLASS_FUNCS

protected:
	virtual void createDatabase();
	virtual void initStatements();

private:
	virtual void writePrivileges(const AuthEntry &authEntry);

	sqlite3_stmt *m_stmt_read = nullptr;
	sqlite3_stmt *m_stmt_write = nullptr;
	sqlite3_stmt *m_stmt_create = nullptr;
	sqlite3_stmt *m_stmt_delete = nullptr;
	sqlite3_stmt *m_stmt_list_names = nullptr;
	sqlite3_stmt *m_stmt_read_privs = nullptr;
	sqlite3_stmt *m_stmt_write_privs = nullptr;
	sqlite3_stmt *m_stmt_delete_privs = nullptr;
	sqlite3_stmt *m_stmt_last_insert_rowid = nullptr;
};

class ModStorageDatabaseSQLite3 : private Database_SQLite3, public ModStorageDatabase
{
public:
	ModStorageDatabaseSQLite3(const std::string &savedir);
	virtual ~ModStorageDatabaseSQLite3();

	virtual void getModEntries(const std::string &modname, StringMap *storage);
	virtual void getModKeys(const std::string &modname, std::vector<std::string> *storage);
	virtual bool getModEntry(const std::string &modname,
		const std::string &key, std::string *value);
	virtual bool hasModEntry(const std::string &modname, const std::string &key);
	virtual bool setModEntry(const std::string &modname,
		const std::string &key,std::string_view value);
	virtual bool removeModEntry(const std::string &modname, const std::string &key);
	virtual bool removeModEntries(const std::string &modname);
	virtual void listMods(std::vector<std::string> *res);

	PARENT_CLASS_FUNCS

protected:
	virtual void createDatabase();
	virtual void initStatements();

private:
	sqlite3_stmt *m_stmt_get_mods = nullptr;
	sqlite3_stmt *m_stmt_get_all = nullptr;
	sqlite3_stmt *m_stmt_get_keys = nullptr;
	sqlite3_stmt *m_stmt_get = nullptr;
	sqlite3_stmt *m_stmt_has = nullptr;
	sqlite3_stmt *m_stmt_set = nullptr;
	sqlite3_stmt *m_stmt_remove = nullptr;
	sqlite3_stmt *m_stmt_remove_all = nullptr;
};

#undef PARENT_CLASS_FUNCS
