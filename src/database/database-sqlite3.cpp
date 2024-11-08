// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

/*
SQLite format specification:
	blocks:
		(PK) INT id
		BLOB data
*/


#include "database-sqlite3.h"

#include "log.h"
#include "filesys.h"
#include "exceptions.h"
#include "settings.h"
#include "porting.h"
#include "util/string.h"
#include "remoteplayer.h"
#include "irrlicht_changes/printing.h"
#include "server/player_sao.h"

#include <cassert>

// When to print messages when the database is being held locked by another process
// Note: I've seen occasional delays of over 250ms while running minetestmapper.
#define BUSY_INFO_TRESHOLD	100	// Print first informational message after 100ms.
#define BUSY_WARNING_TRESHOLD	250	// Print warning message after 250ms. Lag is increased.
#define BUSY_ERROR_TRESHOLD	1000	// Print error message after 1000ms. Significant lag.
#define BUSY_FATAL_TRESHOLD	3000	// Allow SQLITE_BUSY to be returned, which will cause a minetest crash.
#define BUSY_ERROR_INTERVAL	10000	// Safety net: report again every 10 seconds


#define SQLRES(s, r, m) \
	if ((s) != (r)) { \
		throw DatabaseException(std::string(m) + ": " +\
				sqlite3_errmsg(m_database)); \
	}
#define SQLOK(s, m) SQLRES(s, SQLITE_OK, m)

#define PREPARE_STATEMENT(name, query) \
	SQLOK(sqlite3_prepare_v2(m_database, query, -1, &m_stmt_##name, NULL),\
		"Failed to prepare query '" query "'")

#define SQLOK_ERRSTREAM(s, m)                           \
	if ((s) != SQLITE_OK) {                             \
		errorstream << (m) << ": "                      \
			<< sqlite3_errmsg(m_database) << std::endl; \
	}

#define FINALIZE_STATEMENT(statement) SQLOK_ERRSTREAM(sqlite3_finalize(statement), \
	"Failed to finalize " #statement)

int Database_SQLite3::busyHandler(void *data, int count)
{
	s64 &first_time = reinterpret_cast<s64 *>(data)[0];
	s64 &prev_time = reinterpret_cast<s64 *>(data)[1];
	s64 cur_time = porting::getTimeMs();

	if (count == 0) {
		first_time = cur_time;
		prev_time = first_time;
	} else {
		while (cur_time < prev_time)
			cur_time += s64(1)<<32;
	}

	if (cur_time - first_time < BUSY_INFO_TRESHOLD) {
		; // do nothing
	} else if (cur_time - first_time >= BUSY_INFO_TRESHOLD &&
			prev_time - first_time < BUSY_INFO_TRESHOLD) {
		infostream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms." << std::endl;
	} else if (cur_time - first_time >= BUSY_WARNING_TRESHOLD &&
			prev_time - first_time < BUSY_WARNING_TRESHOLD) {
		warningstream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms." << std::endl;
	} else if (cur_time - first_time >= BUSY_ERROR_TRESHOLD &&
			prev_time - first_time < BUSY_ERROR_TRESHOLD) {
		errorstream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms; this causes lag." << std::endl;
	} else if (cur_time - first_time >= BUSY_FATAL_TRESHOLD &&
			prev_time - first_time < BUSY_FATAL_TRESHOLD) {
		errorstream << "SQLite3 database has been locked for "
			<< cur_time - first_time << " ms - giving up!" << std::endl;
	} else if ((cur_time - first_time) / BUSY_ERROR_INTERVAL !=
			(prev_time - first_time) / BUSY_ERROR_INTERVAL) {
		// Safety net: keep reporting every BUSY_ERROR_INTERVAL
		errorstream << "SQLite3 database has been locked for "
			<< (cur_time - first_time) / 1000 << " seconds!" << std::endl;
	}

	prev_time = cur_time;

	// Make sqlite transaction fail if delay exceeds BUSY_FATAL_TRESHOLD
	return cur_time - first_time < BUSY_FATAL_TRESHOLD;
}


Database_SQLite3::Database_SQLite3(const std::string &savedir, const std::string &dbname) :
	m_savedir(savedir),
	m_dbname(dbname)
{
}

void Database_SQLite3::beginSave()
{
	verifyDatabase();
	SQLRES(sqlite3_step(m_stmt_begin), SQLITE_DONE,
		"Failed to start SQLite3 transaction");
	sqlite3_reset(m_stmt_begin);
}

void Database_SQLite3::endSave()
{
	verifyDatabase();
	SQLRES(sqlite3_step(m_stmt_end), SQLITE_DONE,
		"Failed to commit SQLite3 transaction");
	sqlite3_reset(m_stmt_end);
}

void Database_SQLite3::openDatabase()
{
	if (m_database) return;

	std::string dbp = m_savedir + DIR_DELIM + m_dbname + ".sqlite";

	// Open the database connection

	if (!fs::CreateAllDirs(m_savedir)) {
		infostream << "Database_SQLite3: Failed to create directory \""
			<< m_savedir << "\"" << std::endl;
		throw FileNotGoodException("Failed to create database "
				"save directory");
	}

	bool needs_create = !fs::PathExists(dbp);

	SQLOK(sqlite3_open_v2(dbp.c_str(), &m_database,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL),
		std::string("Failed to open SQLite3 database file ") + dbp);

	SQLOK(sqlite3_busy_handler(m_database, Database_SQLite3::busyHandler,
		m_busy_handler_data), "Failed to set SQLite3 busy handler");

	if (needs_create) {
		createDatabase();
	}

	std::string query_str = std::string("PRAGMA synchronous = ")
			 + itos(g_settings->getU16("sqlite_synchronous"));
	SQLOK(sqlite3_exec(m_database, query_str.c_str(), NULL, NULL, NULL),
		"Failed to modify sqlite3 synchronous mode");
	SQLOK(sqlite3_exec(m_database, "PRAGMA foreign_keys = ON", NULL, NULL, NULL),
		"Failed to enable sqlite3 foreign key support");
}

void Database_SQLite3::verifyDatabase()
{
	if (m_initialized) return;

	openDatabase();

	PREPARE_STATEMENT(begin, "BEGIN;");
	PREPARE_STATEMENT(end, "COMMIT;");

	initStatements();

	m_initialized = true;
}

Database_SQLite3::~Database_SQLite3()
{
	FINALIZE_STATEMENT(m_stmt_begin)
	FINALIZE_STATEMENT(m_stmt_end)

	SQLOK_ERRSTREAM(sqlite3_close(m_database), "Failed to close database");
}

/*
 * Map database
 */

MapDatabaseSQLite3::MapDatabaseSQLite3(const std::string &savedir):
	Database_SQLite3(savedir, "map"),
	MapDatabase()
{
}

MapDatabaseSQLite3::~MapDatabaseSQLite3()
{
	FINALIZE_STATEMENT(m_stmt_read)
	FINALIZE_STATEMENT(m_stmt_write)
	FINALIZE_STATEMENT(m_stmt_list)
	FINALIZE_STATEMENT(m_stmt_delete)
}


void MapDatabaseSQLite3::createDatabase()
{
	assert(m_database); // Pre-condition

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `blocks` (\n"
			"	`pos` INT PRIMARY KEY,\n"
			"	`data` BLOB\n"
			");\n",
		NULL, NULL, NULL),
		"Failed to create database table");
}

void MapDatabaseSQLite3::initStatements()
{
	PREPARE_STATEMENT(read, "SELECT `data` FROM `blocks` WHERE `pos` = ? LIMIT 1");
	PREPARE_STATEMENT(write, "REPLACE INTO `blocks` (`pos`, `data`) VALUES (?, ?)");
	PREPARE_STATEMENT(delete, "DELETE FROM `blocks` WHERE `pos` = ?");
	PREPARE_STATEMENT(list, "SELECT `pos` FROM `blocks`");

	verbosestream << "ServerMap: SQLite3 database opened." << std::endl;
}

inline void MapDatabaseSQLite3::bindPos(sqlite3_stmt *stmt, const v3s16 &pos, int index)
{
	SQLOK(sqlite3_bind_int64(stmt, index, getBlockAsInteger(pos)),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));
}

bool MapDatabaseSQLite3::deleteBlock(const v3s16 &pos)
{
	verifyDatabase();

	bindPos(m_stmt_delete, pos);

	bool good = sqlite3_step(m_stmt_delete) == SQLITE_DONE;
	sqlite3_reset(m_stmt_delete);

	if (!good) {
		warningstream << "deleteBlock: Block failed to delete "
			<< pos << ": " << sqlite3_errmsg(m_database) << std::endl;
	}
	return good;
}

bool MapDatabaseSQLite3::saveBlock(const v3s16 &pos, std::string_view data)
{
	verifyDatabase();

	bindPos(m_stmt_write, pos);
	SQLOK(sqlite3_bind_blob(m_stmt_write, 2, data.data(), data.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));

	SQLRES(sqlite3_step(m_stmt_write), SQLITE_DONE, "Failed to save block")
	sqlite3_reset(m_stmt_write);

	return true;
}

void MapDatabaseSQLite3::loadBlock(const v3s16 &pos, std::string *block)
{
	verifyDatabase();

	bindPos(m_stmt_read, pos);

	if (sqlite3_step(m_stmt_read) != SQLITE_ROW) {
		sqlite3_reset(m_stmt_read);
		return;
	}

	auto data = sqlite_to_blob(m_stmt_read, 0);
	block->assign(data);

	sqlite3_step(m_stmt_read);
	// We should never get more than 1 row, so ok to reset
	sqlite3_reset(m_stmt_read);
}

void MapDatabaseSQLite3::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	verifyDatabase();

	while (sqlite3_step(m_stmt_list) == SQLITE_ROW)
		dst.push_back(getIntegerAsBlock(sqlite3_column_int64(m_stmt_list, 0)));

	sqlite3_reset(m_stmt_list);
}

/*
 * Player Database
 */

PlayerDatabaseSQLite3::PlayerDatabaseSQLite3(const std::string &savedir):
	Database_SQLite3(savedir, "players"),
	PlayerDatabase()
{
}

PlayerDatabaseSQLite3::~PlayerDatabaseSQLite3()
{
	FINALIZE_STATEMENT(m_stmt_player_load)
	FINALIZE_STATEMENT(m_stmt_player_add)
	FINALIZE_STATEMENT(m_stmt_player_update)
	FINALIZE_STATEMENT(m_stmt_player_remove)
	FINALIZE_STATEMENT(m_stmt_player_list)
	FINALIZE_STATEMENT(m_stmt_player_add_inventory)
	FINALIZE_STATEMENT(m_stmt_player_add_inventory_items)
	FINALIZE_STATEMENT(m_stmt_player_remove_inventory)
	FINALIZE_STATEMENT(m_stmt_player_remove_inventory_items)
	FINALIZE_STATEMENT(m_stmt_player_load_inventory)
	FINALIZE_STATEMENT(m_stmt_player_load_inventory_items)
	FINALIZE_STATEMENT(m_stmt_player_metadata_load)
	FINALIZE_STATEMENT(m_stmt_player_metadata_add)
	FINALIZE_STATEMENT(m_stmt_player_metadata_remove)
};


void PlayerDatabaseSQLite3::createDatabase()
{
	assert(m_database); // Pre-condition

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `player` ("
			"`name` VARCHAR(50) NOT NULL,"
			"`pitch` NUMERIC(11, 4) NOT NULL,"
			"`yaw` NUMERIC(11, 4) NOT NULL,"
			"`posX` NUMERIC(11, 4) NOT NULL,"
			"`posY` NUMERIC(11, 4) NOT NULL,"
			"`posZ` NUMERIC(11, 4) NOT NULL,"
			"`hp` INT NOT NULL,"
			"`breath` INT NOT NULL,"
			"`creation_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
			"`modification_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
			"PRIMARY KEY (`name`));",
		NULL, NULL, NULL),
		"Failed to create player table");

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `player_metadata` ("
			"    `player` VARCHAR(50) NOT NULL,"
			"    `metadata` VARCHAR(256) NOT NULL,"
			"    `value` TEXT,"
			"    PRIMARY KEY(`player`, `metadata`),"
			"    FOREIGN KEY (`player`) REFERENCES player (`name`) ON DELETE CASCADE );",
		NULL, NULL, NULL),
		"Failed to create player metadata table");

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `player_inventories` ("
			"   `player` VARCHAR(50) NOT NULL,"
			"	`inv_id` INT NOT NULL,"
			"	`inv_width` INT NOT NULL,"
			"	`inv_name` TEXT NOT NULL DEFAULT '',"
			"	`inv_size` INT NOT NULL,"
			"	PRIMARY KEY(player, inv_id),"
			"   FOREIGN KEY (`player`) REFERENCES player (`name`) ON DELETE CASCADE );",
		NULL, NULL, NULL),
		"Failed to create player inventory table");

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE `player_inventory_items` ("
			"   `player` VARCHAR(50) NOT NULL,"
			"	`inv_id` INT NOT NULL,"
			"	`slot_id` INT NOT NULL,"
			"	`item` TEXT NOT NULL DEFAULT '',"
			"	PRIMARY KEY(player, inv_id, slot_id),"
			"   FOREIGN KEY (`player`) REFERENCES player (`name`) ON DELETE CASCADE );",
		NULL, NULL, NULL),
		"Failed to create player inventory items table");
}

void PlayerDatabaseSQLite3::initStatements()
{
	PREPARE_STATEMENT(player_load, "SELECT `pitch`, `yaw`, `posX`, `posY`, `posZ`, `hp`, "
		"`breath`"
		"FROM `player` WHERE `name` = ?")
	PREPARE_STATEMENT(player_add, "INSERT INTO `player` (`name`, `pitch`, `yaw`, `posX`, "
		"`posY`, `posZ`, `hp`, `breath`) VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
	PREPARE_STATEMENT(player_update, "UPDATE `player` SET `pitch` = ?, `yaw` = ?, "
			"`posX` = ?, `posY` = ?, `posZ` = ?, `hp` = ?, `breath` = ?, "
			"`modification_date` = CURRENT_TIMESTAMP WHERE `name` = ?")
	PREPARE_STATEMENT(player_remove, "DELETE FROM `player` WHERE `name` = ?")
	PREPARE_STATEMENT(player_list, "SELECT `name` FROM `player`")

	PREPARE_STATEMENT(player_add_inventory, "INSERT INTO `player_inventories` "
		"(`player`, `inv_id`, `inv_width`, `inv_name`, `inv_size`) VALUES (?, ?, ?, ?, ?)")
	PREPARE_STATEMENT(player_add_inventory_items, "INSERT INTO `player_inventory_items` "
		"(`player`, `inv_id`, `slot_id`, `item`) VALUES (?, ?, ?, ?)")
	PREPARE_STATEMENT(player_remove_inventory, "DELETE FROM `player_inventories` "
		"WHERE `player` = ?")
	PREPARE_STATEMENT(player_remove_inventory_items, "DELETE FROM `player_inventory_items` "
		"WHERE `player` = ?")
	PREPARE_STATEMENT(player_load_inventory, "SELECT `inv_id`, `inv_width`, `inv_name`, "
		"`inv_size` FROM `player_inventories` WHERE `player` = ? ORDER BY inv_id")
	PREPARE_STATEMENT(player_load_inventory_items, "SELECT `slot_id`, `item` "
		"FROM `player_inventory_items` WHERE `player` = ? AND `inv_id` = ?")

	PREPARE_STATEMENT(player_metadata_load, "SELECT `metadata`, `value` FROM "
		"`player_metadata` WHERE `player` = ?")
	PREPARE_STATEMENT(player_metadata_add, "INSERT INTO `player_metadata` "
		"(`player`, `metadata`, `value`) VALUES (?, ?, ?)")
	PREPARE_STATEMENT(player_metadata_remove, "DELETE FROM `player_metadata` "
		"WHERE `player` = ?")
	verbosestream << "ServerEnvironment: SQLite3 database opened (players)." << std::endl;
}

bool PlayerDatabaseSQLite3::playerDataExists(const std::string &name)
{
	verifyDatabase();
	str_to_sqlite(m_stmt_player_load, 1, name);
	bool res = (sqlite3_step(m_stmt_player_load) == SQLITE_ROW);
	sqlite3_reset(m_stmt_player_load);
	return res;
}

void PlayerDatabaseSQLite3::savePlayer(RemotePlayer *player)
{
	PlayerSAO* sao = player->getPlayerSAO();
	sanity_check(sao);

	const v3f &pos = sao->getBasePosition();
	// Begin save in brace is mandatory
	if (!playerDataExists(player->getName())) {
		beginSave();
		str_to_sqlite(m_stmt_player_add, 1, player->getName());
		double_to_sqlite(m_stmt_player_add, 2, sao->getLookPitch());
		double_to_sqlite(m_stmt_player_add, 3, sao->getRotation().Y);
		double_to_sqlite(m_stmt_player_add, 4, pos.X);
		double_to_sqlite(m_stmt_player_add, 5, pos.Y);
		double_to_sqlite(m_stmt_player_add, 6, pos.Z);
		int64_to_sqlite(m_stmt_player_add, 7, sao->getHP());
		int64_to_sqlite(m_stmt_player_add, 8, sao->getBreath());

		sqlite3_vrfy(sqlite3_step(m_stmt_player_add), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_add);
	} else {
		beginSave();
		double_to_sqlite(m_stmt_player_update, 1, sao->getLookPitch());
		double_to_sqlite(m_stmt_player_update, 2, sao->getRotation().Y);
		double_to_sqlite(m_stmt_player_update, 3, pos.X);
		double_to_sqlite(m_stmt_player_update, 4, pos.Y);
		double_to_sqlite(m_stmt_player_update, 5, pos.Z);
		int64_to_sqlite(m_stmt_player_update, 6, sao->getHP());
		int64_to_sqlite(m_stmt_player_update, 7, sao->getBreath());
		str_to_sqlite(m_stmt_player_update, 8, player->getName());

		sqlite3_vrfy(sqlite3_step(m_stmt_player_update), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_update);
	}

	// Write player inventories
	str_to_sqlite(m_stmt_player_remove_inventory, 1, player->getName());
	sqlite3_vrfy(sqlite3_step(m_stmt_player_remove_inventory), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_remove_inventory);

	str_to_sqlite(m_stmt_player_remove_inventory_items, 1, player->getName());
	sqlite3_vrfy(sqlite3_step(m_stmt_player_remove_inventory_items), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_remove_inventory_items);

	const auto &inventory_lists = sao->getInventory()->getLists();
	std::ostringstream oss;
	for (u16 i = 0; i < inventory_lists.size(); i++) {
		const InventoryList *list = inventory_lists[i];

		str_to_sqlite(m_stmt_player_add_inventory, 1, player->getName());
		int_to_sqlite(m_stmt_player_add_inventory, 2, i);
		int_to_sqlite(m_stmt_player_add_inventory, 3, list->getWidth());
		str_to_sqlite(m_stmt_player_add_inventory, 4, list->getName());
		int_to_sqlite(m_stmt_player_add_inventory, 5, list->getSize());
		sqlite3_vrfy(sqlite3_step(m_stmt_player_add_inventory), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_add_inventory);

		for (u32 j = 0; j < list->getSize(); j++) {
			oss.str("");
			oss.clear();
			list->getItem(j).serialize(oss);
			std::string itemStr = oss.str();

			str_to_sqlite(m_stmt_player_add_inventory_items, 1, player->getName());
			int_to_sqlite(m_stmt_player_add_inventory_items, 2, i);
			int_to_sqlite(m_stmt_player_add_inventory_items, 3, j);
			str_to_sqlite(m_stmt_player_add_inventory_items, 4, itemStr);
			sqlite3_vrfy(sqlite3_step(m_stmt_player_add_inventory_items), SQLITE_DONE);
			sqlite3_reset(m_stmt_player_add_inventory_items);
		}
	}

	str_to_sqlite(m_stmt_player_metadata_remove, 1, player->getName());
	sqlite3_vrfy(sqlite3_step(m_stmt_player_metadata_remove), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_metadata_remove);

	const StringMap &attrs = sao->getMeta().getStrings();
	for (const auto &attr : attrs) {
		str_to_sqlite(m_stmt_player_metadata_add, 1, player->getName());
		str_to_sqlite(m_stmt_player_metadata_add, 2, attr.first);
		str_to_sqlite(m_stmt_player_metadata_add, 3, attr.second);
		sqlite3_vrfy(sqlite3_step(m_stmt_player_metadata_add), SQLITE_DONE);
		sqlite3_reset(m_stmt_player_metadata_add);
	}

	endSave();

	player->onSuccessfulSave();
}

bool PlayerDatabaseSQLite3::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_player_load, 1, player->getName());
	if (sqlite3_step(m_stmt_player_load) != SQLITE_ROW) {
		sqlite3_reset(m_stmt_player_load);
		return false;
	}
	sao->setLookPitch(sqlite_to_float(m_stmt_player_load, 0));
	sao->setPlayerYaw(sqlite_to_float(m_stmt_player_load, 1));
	sao->setBasePosition(sqlite_to_v3f(m_stmt_player_load, 2));
	sao->setHPRaw((u16) MYMIN(sqlite_to_int(m_stmt_player_load, 5), U16_MAX));
	sao->setBreath((u16) MYMIN(sqlite_to_int(m_stmt_player_load, 6), U16_MAX), false);
	sqlite3_reset(m_stmt_player_load);

	// Load inventory
	str_to_sqlite(m_stmt_player_load_inventory, 1, player->getName());
	while (sqlite3_step(m_stmt_player_load_inventory) == SQLITE_ROW) {
		InventoryList *invList = player->inventory.addList(
			sqlite_to_string(m_stmt_player_load_inventory, 2),
			sqlite_to_uint(m_stmt_player_load_inventory, 3));
		invList->setWidth(sqlite_to_uint(m_stmt_player_load_inventory, 1));

		u32 invId = sqlite_to_uint(m_stmt_player_load_inventory, 0);

		str_to_sqlite(m_stmt_player_load_inventory_items, 1, player->getName());
		int_to_sqlite(m_stmt_player_load_inventory_items, 2, invId);
		while (sqlite3_step(m_stmt_player_load_inventory_items) == SQLITE_ROW) {
			const std::string itemStr = sqlite_to_string(m_stmt_player_load_inventory_items, 1);
			if (!itemStr.empty()) {
				ItemStack stack;
				stack.deSerialize(itemStr);
				invList->changeItem(sqlite_to_uint(m_stmt_player_load_inventory_items, 0), stack);
			}
		}
		sqlite3_reset(m_stmt_player_load_inventory_items);
	}

	sqlite3_reset(m_stmt_player_load_inventory);

	str_to_sqlite(m_stmt_player_metadata_load, 1, sao->getPlayer()->getName());
	while (sqlite3_step(m_stmt_player_metadata_load) == SQLITE_ROW) {
		std::string attr = sqlite_to_string(m_stmt_player_metadata_load, 0);
		auto value = sqlite_to_string_view(m_stmt_player_metadata_load, 1);

		sao->getMeta().setString(attr, value);
	}
	sao->getMeta().setModified(false);
	sqlite3_reset(m_stmt_player_metadata_load);
	return true;
}

bool PlayerDatabaseSQLite3::removePlayer(const std::string &name)
{
	if (!playerDataExists(name))
		return false;

	str_to_sqlite(m_stmt_player_remove, 1, name);
	sqlite3_vrfy(sqlite3_step(m_stmt_player_remove), SQLITE_DONE);
	sqlite3_reset(m_stmt_player_remove);
	return true;
}

void PlayerDatabaseSQLite3::listPlayers(std::vector<std::string> &res)
{
	verifyDatabase();

	while (sqlite3_step(m_stmt_player_list) == SQLITE_ROW)
		res.emplace_back(sqlite_to_string_view(m_stmt_player_list, 0));

	sqlite3_reset(m_stmt_player_list);
}

/*
 * Auth database
 */

AuthDatabaseSQLite3::AuthDatabaseSQLite3(const std::string &savedir) :
		Database_SQLite3(savedir, "auth"), AuthDatabase()
{
}

AuthDatabaseSQLite3::~AuthDatabaseSQLite3()
{
	FINALIZE_STATEMENT(m_stmt_read)
	FINALIZE_STATEMENT(m_stmt_write)
	FINALIZE_STATEMENT(m_stmt_create)
	FINALIZE_STATEMENT(m_stmt_delete)
	FINALIZE_STATEMENT(m_stmt_list_names)
	FINALIZE_STATEMENT(m_stmt_read_privs)
	FINALIZE_STATEMENT(m_stmt_write_privs)
	FINALIZE_STATEMENT(m_stmt_delete_privs)
	FINALIZE_STATEMENT(m_stmt_last_insert_rowid)
}

void AuthDatabaseSQLite3::createDatabase()
{
	assert(m_database); // Pre-condition

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `auth` ("
			"`id` INTEGER PRIMARY KEY AUTOINCREMENT,"
			"`name` VARCHAR(32) UNIQUE,"
			"`password` VARCHAR(512),"
			"`last_login` INTEGER"
		");",
		NULL, NULL, NULL),
		"Failed to create auth table");

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `user_privileges` ("
			"`id` INTEGER,"
			"`privilege` VARCHAR(32),"
			"PRIMARY KEY (id, privilege)"
			"CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES auth (id) ON DELETE CASCADE"
		");",
		NULL, NULL, NULL),
		"Failed to create auth privileges table");
}

void AuthDatabaseSQLite3::initStatements()
{
	PREPARE_STATEMENT(read, "SELECT id, name, password, last_login FROM auth WHERE name = ?");
	PREPARE_STATEMENT(write, "UPDATE auth set name = ?, password = ?, last_login = ? WHERE id = ?");
	PREPARE_STATEMENT(create, "INSERT INTO auth (name, password, last_login) VALUES (?, ?, ?)");
	PREPARE_STATEMENT(delete, "DELETE FROM auth WHERE name = ?");

	PREPARE_STATEMENT(list_names, "SELECT name FROM auth ORDER BY name DESC");

	PREPARE_STATEMENT(read_privs, "SELECT privilege FROM user_privileges WHERE id = ?");
	PREPARE_STATEMENT(write_privs, "INSERT OR IGNORE INTO user_privileges (id, privilege) VALUES (?, ?)");
	PREPARE_STATEMENT(delete_privs, "DELETE FROM user_privileges WHERE id = ?");

	PREPARE_STATEMENT(last_insert_rowid, "SELECT last_insert_rowid()");
}

bool AuthDatabaseSQLite3::getAuth(const std::string &name, AuthEntry &res)
{
	verifyDatabase();
	str_to_sqlite(m_stmt_read, 1, name);
	if (sqlite3_step(m_stmt_read) != SQLITE_ROW) {
		sqlite3_reset(m_stmt_read);
		return false;
	}
	res.id = sqlite_to_uint(m_stmt_read, 0);
	res.name = sqlite_to_string_view(m_stmt_read, 1);
	res.password = sqlite_to_string_view(m_stmt_read, 2);
	res.last_login = sqlite_to_int64(m_stmt_read, 3);
	sqlite3_reset(m_stmt_read);

	int64_to_sqlite(m_stmt_read_privs, 1, res.id);
	while (sqlite3_step(m_stmt_read_privs) == SQLITE_ROW) {
		res.privileges.emplace_back(sqlite_to_string_view(m_stmt_read_privs, 0));
	}
	sqlite3_reset(m_stmt_read_privs);

	return true;
}

bool AuthDatabaseSQLite3::saveAuth(const AuthEntry &authEntry)
{
	beginSave();

	str_to_sqlite(m_stmt_write, 1, authEntry.name);
	str_to_sqlite(m_stmt_write, 2, authEntry.password);
	int64_to_sqlite(m_stmt_write, 3, authEntry.last_login);
	int64_to_sqlite(m_stmt_write, 4, authEntry.id);
	sqlite3_vrfy(sqlite3_step(m_stmt_write), SQLITE_DONE);
	sqlite3_reset(m_stmt_write);

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabaseSQLite3::createAuth(AuthEntry &authEntry)
{
	beginSave();

	// id autoincrements
	str_to_sqlite(m_stmt_create, 1, authEntry.name);
	str_to_sqlite(m_stmt_create, 2, authEntry.password);
	int64_to_sqlite(m_stmt_create, 3, authEntry.last_login);
	sqlite3_vrfy(sqlite3_step(m_stmt_create), SQLITE_DONE);
	sqlite3_reset(m_stmt_create);

	// obtain id and write back to original authEntry
	sqlite3_step(m_stmt_last_insert_rowid);
	authEntry.id = sqlite_to_uint(m_stmt_last_insert_rowid, 0);
	sqlite3_reset(m_stmt_last_insert_rowid);

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabaseSQLite3::deleteAuth(const std::string &name)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_delete, 1, name);
	sqlite3_vrfy(sqlite3_step(m_stmt_delete), SQLITE_DONE);
	int changes = sqlite3_changes(m_database);
	sqlite3_reset(m_stmt_delete);

	// privileges deleted by foreign key on delete cascade

	return changes > 0;
}

void AuthDatabaseSQLite3::listNames(std::vector<std::string> &res)
{
	verifyDatabase();

	while (sqlite3_step(m_stmt_list_names) == SQLITE_ROW) {
		res.emplace_back(sqlite_to_string_view(m_stmt_list_names, 0));
	}
	sqlite3_reset(m_stmt_list_names);
}

void AuthDatabaseSQLite3::reload()
{
	// noop for SQLite
}

void AuthDatabaseSQLite3::writePrivileges(const AuthEntry &authEntry)
{
	int64_to_sqlite(m_stmt_delete_privs, 1, authEntry.id);
	sqlite3_vrfy(sqlite3_step(m_stmt_delete_privs), SQLITE_DONE);
	sqlite3_reset(m_stmt_delete_privs);
	for (const std::string &privilege : authEntry.privileges) {
		int64_to_sqlite(m_stmt_write_privs, 1, authEntry.id);
		str_to_sqlite(m_stmt_write_privs, 2, privilege);
		sqlite3_vrfy(sqlite3_step(m_stmt_write_privs), SQLITE_DONE);
		sqlite3_reset(m_stmt_write_privs);
	}
}

ModStorageDatabaseSQLite3::ModStorageDatabaseSQLite3(const std::string &savedir):
	Database_SQLite3(savedir, "mod_storage"), ModStorageDatabase()
{
}

ModStorageDatabaseSQLite3::~ModStorageDatabaseSQLite3()
{
	FINALIZE_STATEMENT(m_stmt_remove_all)
	FINALIZE_STATEMENT(m_stmt_remove)
	FINALIZE_STATEMENT(m_stmt_set)
	FINALIZE_STATEMENT(m_stmt_has)
	FINALIZE_STATEMENT(m_stmt_get)
	FINALIZE_STATEMENT(m_stmt_get_keys)
	FINALIZE_STATEMENT(m_stmt_get_all)
}

void ModStorageDatabaseSQLite3::createDatabase()
{
	assert(m_database); // Pre-condition

	SQLOK(sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `entries` (\n"
			"	`modname` TEXT NOT NULL,\n"
			"	`key` BLOB NOT NULL,\n"
			"	`value` BLOB NOT NULL,\n"
			"	PRIMARY KEY (`modname`, `key`)\n"
			");\n",
		NULL, NULL, NULL),
		"Failed to create database table");
}

void ModStorageDatabaseSQLite3::initStatements()
{
	PREPARE_STATEMENT(get_all, "SELECT `key`, `value` FROM `entries` WHERE `modname` = ?");
	PREPARE_STATEMENT(get_keys, "SELECT `key` FROM `entries` WHERE `modname` = ?");
	PREPARE_STATEMENT(get,
		"SELECT `value` FROM `entries` WHERE `modname` = ? AND `key` = ? LIMIT 1");
	PREPARE_STATEMENT(has,
		"SELECT 1 FROM `entries` WHERE `modname` = ? AND `key` = ? LIMIT 1");
	PREPARE_STATEMENT(set,
		"REPLACE INTO `entries` (`modname`, `key`, `value`) VALUES (?, ?, ?)");
	PREPARE_STATEMENT(remove, "DELETE FROM `entries` WHERE `modname` = ? AND `key` = ?");
	PREPARE_STATEMENT(remove_all, "DELETE FROM `entries` WHERE `modname` = ?");
}

void ModStorageDatabaseSQLite3::getModEntries(const std::string &modname, StringMap *storage)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_get_all, 1, modname);
	while (sqlite3_step(m_stmt_get_all) == SQLITE_ROW) {
		auto key = sqlite_to_blob(m_stmt_get_all, 0);
		auto value = sqlite_to_blob(m_stmt_get_all, 1);
		(*storage)[std::string(key)].assign(value);
	}
	sqlite3_vrfy(sqlite3_errcode(m_database), SQLITE_DONE);

	sqlite3_reset(m_stmt_get_all);
}

void ModStorageDatabaseSQLite3::getModKeys(const std::string &modname,
		std::vector<std::string> *storage)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_get_keys, 1, modname);
	while (sqlite3_step(m_stmt_get_keys) == SQLITE_ROW) {
		auto key = sqlite_to_blob(m_stmt_get_keys, 0);
		storage->emplace_back(key);
	}
	sqlite3_vrfy(sqlite3_errcode(m_database), SQLITE_DONE);

	sqlite3_reset(m_stmt_get_keys);
}

bool ModStorageDatabaseSQLite3::getModEntry(const std::string &modname,
	const std::string &key, std::string *value)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_get, 1, modname);
	SQLOK(sqlite3_bind_blob(m_stmt_get, 2, key.data(), key.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));
	bool found = sqlite3_step(m_stmt_get) == SQLITE_ROW;
	if (found) {
		auto sv = sqlite_to_blob(m_stmt_get, 0);
		value->assign(sv);
		sqlite3_step(m_stmt_get);
	}

	sqlite3_reset(m_stmt_get);

	return found;
}

bool ModStorageDatabaseSQLite3::hasModEntry(const std::string &modname,
		const std::string &key)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_has, 1, modname);
	SQLOK(sqlite3_bind_blob(m_stmt_has, 2, key.data(), key.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));
	bool found = sqlite3_step(m_stmt_has) == SQLITE_ROW;
	if (found)
		sqlite3_step(m_stmt_has);

	sqlite3_reset(m_stmt_has);

	return found;
}

bool ModStorageDatabaseSQLite3::setModEntry(const std::string &modname,
	const std::string &key, std::string_view value)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_set, 1, modname);
	SQLOK(sqlite3_bind_blob(m_stmt_set, 2, key.data(), key.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));
	SQLOK(sqlite3_bind_blob(m_stmt_set, 3, value.data(), value.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));
	SQLRES(sqlite3_step(m_stmt_set), SQLITE_DONE, "Failed to set mod entry")

	sqlite3_reset(m_stmt_set);

	return true;
}

bool ModStorageDatabaseSQLite3::removeModEntry(const std::string &modname,
		const std::string &key)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_remove, 1, modname);
	SQLOK(sqlite3_bind_blob(m_stmt_remove, 2, key.data(), key.size(), NULL),
		"Internal error: failed to bind query at " __FILE__ ":" TOSTRING(__LINE__));
	sqlite3_vrfy(sqlite3_step(m_stmt_remove), SQLITE_DONE);
	int changes = sqlite3_changes(m_database);

	sqlite3_reset(m_stmt_remove);

	return changes > 0;
}

bool ModStorageDatabaseSQLite3::removeModEntries(const std::string &modname)
{
	verifyDatabase();

	str_to_sqlite(m_stmt_remove_all, 1, modname);
	sqlite3_vrfy(sqlite3_step(m_stmt_remove_all), SQLITE_DONE);
	int changes = sqlite3_changes(m_database);

	sqlite3_reset(m_stmt_remove_all);

	return changes > 0;
}

void ModStorageDatabaseSQLite3::listMods(std::vector<std::string> *res)
{
	verifyDatabase();

	char *errmsg;
	int status = sqlite3_exec(m_database,
		"SELECT `modname` FROM `entries` GROUP BY `modname`;",
		[](void *res_vp, int n_col, char **cols, char **col_names) noexcept -> int {
			try {
				((decltype(res)) res_vp)->emplace_back(cols[0]);
			} catch (...) {
				return 1;
			}
			return 0;
		}, (void *) res, &errmsg);
	if (status != SQLITE_OK) {
		auto msg = std::string("Error trying to list mods with metadata: ") + errmsg;
		sqlite3_free(errmsg);
		throw DatabaseException(msg);
	}
}
