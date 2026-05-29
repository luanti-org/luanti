/*
 * MariaDB Database Backend for Luanti
 * Copyright 2023 CaffeinatedBlocks (caffeinatedblocks@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include "config.h"

#if USE_MARIADB

#include <mysql.h>
#include <string>
#include <map>
#include "database.h"

class Database_MariaDB: public Database {
public:
	Database_MariaDB(const std::string &connect_string, const char *type);
	~Database_MariaDB();
	void beginSave() { beginTransaction(); }
	void endSave() { commit(); }
	void rollback();

protected:
	void beginTransaction();
	void checkConnection();
	void commit();
	void connect();
	void createTable(const std::string &table_name, const std::string &table_spec);
	std::int64_t lastInsertId();
	MYSQL_STMT *prepareStatement(const std::string &query);

	void stmtExec(MYSQL_STMT *stmt, MYSQL_BIND *params = nullptr);
	int stmtQuery(MYSQL_STMT *stmt, MYSQL_BIND *params, MYSQL_BIND *results);
	static void stmtCheck(MYSQL_STMT *stmt, int rc, const char *context);
	void connCheck(int rc, const char *context);

	MYSQL *m_conn = nullptr;

private:
	static std::map<std::string, std::string> parseConnectionString(std::string input);
	std::map<std::string, std::string> m_params;
};

class MapDatabaseMariaDB : private Database_MariaDB, public MapDatabase {
public:
	MapDatabaseMariaDB(const std::string &connect_string);
	~MapDatabaseMariaDB();
	void beginSave() { Database_MariaDB::beginSave(); }
	void endSave() { Database_MariaDB::endSave(); }
	void rollback() { Database_MariaDB::rollback(); }
	bool deleteBlock(const v3s16 &pos);
	void listAllLoadableBlocks(std::vector<v3s16> &dst);
	void loadBlock(const v3s16 &pos, std::string *block);
	bool saveBlock(const v3s16 &pos, std::string_view data);

private:
	MYSQL_STMT *m_stmt_delete = nullptr;
	MYSQL_STMT *m_stmt_list = nullptr;
	MYSQL_STMT *m_stmt_load = nullptr;
	MYSQL_STMT *m_stmt_save = nullptr;
};

class PlayerDatabaseMariaDB : private Database_MariaDB, public PlayerDatabase {
public:
	PlayerDatabaseMariaDB(const std::string &connect_string);
	~PlayerDatabaseMariaDB();
	void beginSave() { Database_MariaDB::beginSave(); }
	void endSave() { Database_MariaDB::endSave(); }
	void rollback() { Database_MariaDB::rollback(); }
	void savePlayer(RemotePlayer *player);
	bool loadPlayer(RemotePlayer *player, PlayerSAO *sao);
	bool removePlayer(const std::string &name);
	void listPlayers(std::vector<std::string> &res);

private:
	bool playerDataExists(const std::string &playername);
	MYSQL_STMT *m_stmt_add_inv = nullptr;
	MYSQL_STMT *m_stmt_add_inv_item = nullptr;
	MYSQL_STMT *m_stmt_load_player = nullptr;
	MYSQL_STMT *m_stmt_load_inv = nullptr;
	MYSQL_STMT *m_stmt_load_inv_items = nullptr;
	MYSQL_STMT *m_stmt_load_player_list = nullptr;
	MYSQL_STMT *m_stmt_load_meta = nullptr;
	MYSQL_STMT *m_stmt_remove_player = nullptr;
	MYSQL_STMT *m_stmt_remove_inv = nullptr;
	MYSQL_STMT *m_stmt_remove_inv_items = nullptr;
	MYSQL_STMT *m_stmt_remove_meta = nullptr;
	MYSQL_STMT *m_stmt_save_player = nullptr;
	MYSQL_STMT *m_stmt_save_meta = nullptr;
};

class AuthDatabaseMariaDB : private Database_MariaDB, public AuthDatabase {
public:
	AuthDatabaseMariaDB(const std::string &connect_string);
	~AuthDatabaseMariaDB();
	void beginSave() { Database_MariaDB::beginSave(); }
	void endSave() { Database_MariaDB::endSave(); }
	void rollback() { Database_MariaDB::rollback(); }
	bool getAuth(const std::string &name, AuthEntry &res);
	bool saveAuth(const AuthEntry &authEntry);
	bool createAuth(AuthEntry &authEntry);
	bool deleteAuth(const std::string &name);
	void listNames(std::vector<std::string> &res);
	void reload();

private:
	void writePrivileges(const AuthEntry &authEntry);
	MYSQL_STMT *m_stmt_create_auth = nullptr;
	MYSQL_STMT *m_stmt_delete_auth = nullptr;
	MYSQL_STMT *m_stmt_delete_privs = nullptr;
	MYSQL_STMT *m_stmt_list_names = nullptr;
	MYSQL_STMT *m_stmt_read_auth = nullptr;
	MYSQL_STMT *m_stmt_read_privs = nullptr;
	MYSQL_STMT *m_stmt_write_auth = nullptr;
	MYSQL_STMT *m_stmt_write_privs = nullptr;
};

class ModStorageDatabaseMariaDB : private Database_MariaDB, public ModStorageDatabase {
public:
	ModStorageDatabaseMariaDB(const std::string &connect_string);
	~ModStorageDatabaseMariaDB();
	void beginSave() { Database_MariaDB::beginSave(); }
	void endSave() { Database_MariaDB::endSave(); }
	void rollback() { Database_MariaDB::rollback(); }
	void getModEntries(const std::string &modname, StringMap *storage);
	void getModKeys(const std::string &modname, std::vector<std::string> *storage);
	bool getModEntry(const std::string &modname, const std::string &key, std::string *value);
	bool hasModEntry(const std::string &modname, const std::string &key);
	bool setModEntry(const std::string &modname, const std::string &key, std::string_view value);
	bool removeModEntry(const std::string &modname, const std::string &key);
	bool removeModEntries(const std::string &modname);
	void listMods(std::vector<std::string> *res);

private:
	MYSQL_STMT *m_stmt_get_entries = nullptr;
	MYSQL_STMT *m_stmt_get_keys = nullptr;
	MYSQL_STMT *m_stmt_get_entry = nullptr;
	MYSQL_STMT *m_stmt_has_entry = nullptr;
	MYSQL_STMT *m_stmt_list_mods = nullptr;
	MYSQL_STMT *m_stmt_remove_entry = nullptr;
	MYSQL_STMT *m_stmt_remove_entries = nullptr;
	MYSQL_STMT *m_stmt_set_entry = nullptr;
};

#endif // USE_MARIADB
