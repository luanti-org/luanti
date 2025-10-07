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

#include "config.h"

#if USE_MARIADB

#include <cstdlib>
#include <cstring>
#include "database-mariadb.h"
#include "debug.h"
#include "exceptions.h"
#include "player.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include "settings.h"
#include "util/string.h"


static void initBind(MYSQL_BIND &b, enum_field_types type,
	void *buffer, unsigned long buffer_length,
	unsigned long *length = nullptr, my_bool *is_null = nullptr)
{
	memset(&b, 0, sizeof(b));
	b.buffer_type = type;
	b.buffer = buffer;
	b.buffer_length = buffer_length;
	if (length)
		b.length = length;
	if (is_null)
		b.is_null = is_null;
}

static inline void closeStmt(MYSQL_STMT *&stmt)
{
	if (stmt) {
		mysql_stmt_close(stmt);
		stmt = nullptr;
	}
}

// RAII guard for mysql_stmt_free_result()
struct StmtResultGuard {
	MYSQL_STMT *stmt;
	explicit StmtResultGuard(MYSQL_STMT *s) : stmt(s) {}
	~StmtResultGuard() { mysql_stmt_free_result(stmt); }
};


/*
 * Database_MariaDB -- base class
 */

Database_MariaDB::Database_MariaDB(const std::string &connect_string, const char *type)
{
	if (connect_string.empty()) {
		std::ostringstream oss;
		oss << "[MariaDB] Error: connection string is empty or undefined.\n\n"
		    << "Set mariadb" << type << "_connection in world.mt to use the MariaDB backend.\n\n"
		    << "Example:\n\n"
		    << "  mariadb" << type << "_connection = host=127.0.0.1 port=3306 user=luanti password=changeme dbname=luanti\n\n"
		    << "See doc/mariadb-setup.md for database setup instructions.\n\n";
		throw SettingNotFoundException(oss.str());
	}

	m_params = parseConnectionString(connect_string);

	std::string key = "mariadb" + std::string(type);

	const char *required[] = {"host", "port", "user", "password", "dbname"};
	for (const char *param : required) {
		if (m_params.count(param) == 0 || m_params.at(param).empty()) {
			errorstream << "[MariaDB] Error: required parameter '" << param
				<< "' is undefined or empty (check " << key
				<< " in world.mt)" << std::endl;
			throw DatabaseException("MariaDB error");
		}
	}
}

Database_MariaDB::~Database_MariaDB()
{
	if (m_conn) {
		mysql_close(m_conn);
		m_conn = nullptr;
	}
}

void Database_MariaDB::beginTransaction()
{
	connCheck(mysql_query(m_conn, "START TRANSACTION"), "beginTransaction");
}

void Database_MariaDB::checkConnection()
{
	if (!m_conn) {
		errorstream << "[MariaDB] Error: no connection" << std::endl;
		throw DatabaseException("MariaDB Error");
	}
	mysql_ping(m_conn);
}

void Database_MariaDB::commit()
{
	connCheck(mysql_commit(m_conn), "commit");
}

void Database_MariaDB::connect()
{
	m_conn = mysql_init(nullptr);
	if (!m_conn) {
		errorstream << "[MariaDB] Error: mysql_init() failed" << std::endl;
		throw DatabaseException("MariaDB Error");
	}

	my_bool reconnect = 1;
	mysql_options(m_conn, MYSQL_OPT_RECONNECT, &reconnect);

	unsigned int port = mystoi(m_params.at("port"));
	const char *socket_path = nullptr;
	if (m_params.count("socket") && !m_params.at("socket").empty())
		socket_path = m_params.at("socket").c_str();
	if (!mysql_real_connect(m_conn,
			m_params.at("host").c_str(),
			m_params.at("user").c_str(),
			m_params.at("password").c_str(),
			m_params.at("dbname").c_str(),
			port,
			socket_path,
			0)) {    // client flags
		errorstream << "[MariaDB] Error: " << mysql_error(m_conn) << std::endl;
		mysql_close(m_conn);
		m_conn = nullptr;
		throw DatabaseException("MariaDB Error");
	}
}

void Database_MariaDB::createTable(const std::string &table_name, const std::string &table_spec)
{
	std::string sql = "CREATE TABLE IF NOT EXISTS " + table_name +
		" (" + table_spec + ") CHARACTER SET = 'utf8mb4'"
		" COLLATE = 'utf8mb4_unicode_ci' ENGINE=InnoDB";
	connCheck(mysql_query(m_conn, sql.c_str()), "createTable");
}

std::int64_t Database_MariaDB::lastInsertId()
{
	return (std::int64_t)mysql_insert_id(m_conn);
}

std::map<std::string, std::string> Database_MariaDB::parseConnectionString(std::string input)
{
	input += " ";
	std::map<std::string, std::string> params;
	size_t pos, last_pos = 0;

	while (std::string::npos != (pos = input.find(' ', last_pos))) {
		std::string part = input.substr(last_pos, pos - last_pos);
		std::size_t pos2 = part.find('=');
		if (std::string::npos != pos2)
			params[part.substr(0, pos2)] = part.substr(pos2 + 1);
		last_pos = pos + 1;
	}

	return params;
}

MYSQL_STMT *Database_MariaDB::prepareStatement(const std::string &query)
{
	MYSQL_STMT *stmt = mysql_stmt_init(m_conn);
	if (!stmt) {
		errorstream << "[MariaDB] Error: mysql_stmt_init() failed" << std::endl;
		throw DatabaseException("MariaDB Error");
	}
	if (mysql_stmt_prepare(stmt, query.c_str(), query.size()) != 0) {
		errorstream << "[MariaDB] Error preparing \"" << query << "\": "
			<< mysql_stmt_error(stmt) << std::endl;
		mysql_stmt_close(stmt);
		throw DatabaseException("MariaDB Error");
	}
	return stmt;
}

void Database_MariaDB::rollback()
{
	connCheck(mysql_rollback(m_conn), "rollback");
}

void Database_MariaDB::stmtExec(MYSQL_STMT *stmt, MYSQL_BIND *params)
{
	if (params)
		stmtCheck(stmt, mysql_stmt_bind_param(stmt, params), "bind_param");
	stmtCheck(stmt, mysql_stmt_execute(stmt), "execute");
}

int Database_MariaDB::stmtQuery(MYSQL_STMT *stmt, MYSQL_BIND *params, MYSQL_BIND *results)
{
	if (params)
		stmtCheck(stmt, mysql_stmt_bind_param(stmt, params), "bind_param");
	stmtCheck(stmt, mysql_stmt_execute(stmt), "execute");
	if (results)
		stmtCheck(stmt, mysql_stmt_bind_result(stmt, results), "bind_result");
	stmtCheck(stmt, mysql_stmt_store_result(stmt), "store_result");
	return (int)mysql_stmt_num_rows(stmt);
}

void Database_MariaDB::stmtCheck(MYSQL_STMT *stmt, int rc, const char *context)
{
	if (rc != 0) {
		errorstream << "[MariaDB] Error in " << context << ": "
			<< mysql_stmt_error(stmt) << " (errno "
			<< mysql_stmt_errno(stmt) << ")" << std::endl;
		throw DatabaseException("MariaDB Error");
	}
}

void Database_MariaDB::connCheck(int rc, const char *context)
{
	if (rc != 0) {
		errorstream << "[MariaDB] Error in " << context << ": "
			<< mysql_error(m_conn) << " (errno "
			<< mysql_errno(m_conn) << ")" << std::endl;
		throw DatabaseException("MariaDB Error");
	}
}


/*
 * MapDatabaseMariaDB
 */

MapDatabaseMariaDB::MapDatabaseMariaDB(const std::string &connect_string)
	: Database_MariaDB(connect_string, ""), MapDatabase()
{
	connect();

	createTable("blocks",
		"posX SMALLINT NOT NULL, "
		"posY SMALLINT NOT NULL, "
		"posZ SMALLINT NOT NULL, "
		"data LONGBLOB NOT NULL, "
		"PRIMARY KEY (posX, posY, posZ)"
	);

	m_stmt_delete = prepareStatement(
		"DELETE FROM blocks WHERE posX = ? AND posY = ? AND posZ = ?");
	m_stmt_load = prepareStatement(
		"SELECT data FROM blocks WHERE posX = ? AND posY = ? AND posZ = ?");
	m_stmt_list = prepareStatement(
		"SELECT posX, posY, posZ FROM blocks");
	m_stmt_save = prepareStatement(
		"INSERT INTO blocks (posX, posY, posZ, data) VALUES (?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE data = VALUES(data)");
}

MapDatabaseMariaDB::~MapDatabaseMariaDB()
{
	closeStmt(m_stmt_delete);
	closeStmt(m_stmt_load);
	closeStmt(m_stmt_list);
	closeStmt(m_stmt_save);
}

bool MapDatabaseMariaDB::deleteBlock(const v3s16 &pos)
{
	checkConnection();

	int16_t px = pos.X, py = pos.Y, pz = pos.Z;
	MYSQL_BIND params[3] = {};
	initBind(params[0], MYSQL_TYPE_SHORT, &px, sizeof(px));
	initBind(params[1], MYSQL_TYPE_SHORT, &py, sizeof(py));
	initBind(params[2], MYSQL_TYPE_SHORT, &pz, sizeof(pz));

	stmtExec(m_stmt_delete, params);
	return mysql_stmt_affected_rows(m_stmt_delete) > 0;
}

void MapDatabaseMariaDB::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	checkConnection();

	int16_t px, py, pz;
	MYSQL_BIND results[3] = {};
	initBind(results[0], MYSQL_TYPE_SHORT, &px, sizeof(px));
	initBind(results[1], MYSQL_TYPE_SHORT, &py, sizeof(py));
	initBind(results[2], MYSQL_TYPE_SHORT, &pz, sizeof(pz));

	stmtQuery(m_stmt_list, nullptr, results);
	StmtResultGuard guard(m_stmt_list);

	while (mysql_stmt_fetch(m_stmt_list) == 0)
		dst.emplace_back(px, py, pz);
}

void MapDatabaseMariaDB::loadBlock(const v3s16 &pos, std::string *block)
{
	checkConnection();

	int16_t px = pos.X, py = pos.Y, pz = pos.Z;
	MYSQL_BIND params[3] = {};
	initBind(params[0], MYSQL_TYPE_SHORT, &px, sizeof(px));
	initBind(params[1], MYSQL_TYPE_SHORT, &py, sizeof(py));
	initBind(params[2], MYSQL_TYPE_SHORT, &pz, sizeof(pz));

	// Result: LONGBLOB data. Start with a reasonable buffer.
	char buf[65536];
	unsigned long data_len = 0;
	my_bool data_null = 0;
	MYSQL_BIND results[1] = {};
	initBind(results[0], MYSQL_TYPE_LONG_BLOB, buf, sizeof(buf), &data_len, &data_null);

	int rows = stmtQuery(m_stmt_load, params, results);
	StmtResultGuard guard(m_stmt_load);

	if (rows == 0) {
		block->clear();
		return;
	}

	int rc = mysql_stmt_fetch(m_stmt_load);
	if (rc == 0 && !data_null) {
		block->assign(buf, data_len);
	} else if (rc == MYSQL_DATA_TRUNCATED && !data_null) {
		// Data exceeded buffer, re-fetch with correct size
		block->resize(data_len);
		MYSQL_BIND col = {};
		initBind(col, MYSQL_TYPE_LONG_BLOB, block->data(), data_len, &data_len);
		stmtCheck(m_stmt_load,
			mysql_stmt_fetch_column(m_stmt_load, &col, 0, 0),
			"fetch_column");
	} else {
		block->clear();
	}
}

bool MapDatabaseMariaDB::saveBlock(const v3s16 &pos, std::string_view data)
{
	checkConnection();

	if (data.size() > UINT32_MAX) {
		errorstream << "[MariaDB] Error: refusing to save block at "
			<< "(" << pos.X << ", " << pos.Y << ", " << pos.Z << ") "
			<< "because data would be truncated: " << data.size()
			<< " > " << UINT32_MAX << std::endl;
		return false;
	}

	int16_t px = pos.X, py = pos.Y, pz = pos.Z;
	unsigned long blob_len = data.size();
	MYSQL_BIND params[4] = {};
	initBind(params[0], MYSQL_TYPE_SHORT, &px, sizeof(px));
	initBind(params[1], MYSQL_TYPE_SHORT, &py, sizeof(py));
	initBind(params[2], MYSQL_TYPE_SHORT, &pz, sizeof(pz));
	initBind(params[3], MYSQL_TYPE_LONG_BLOB, (void *)data.data(), blob_len, &blob_len);

	stmtExec(m_stmt_save, params);
	return mysql_stmt_affected_rows(m_stmt_save) > 0;
}


/*
 * PlayerDatabaseMariaDB
 */

PlayerDatabaseMariaDB::PlayerDatabaseMariaDB(const std::string &connect_string)
	: Database_MariaDB(connect_string, "_player"), PlayerDatabase()
{
	connect();

	createTable("player",
		"name VARCHAR(" + std::to_string(PLAYERNAME_SIZE) + ") NOT NULL, "
		"pitch DECIMAL(15, 7) NOT NULL, "
		"yaw DECIMAL(15, 7) NOT NULL, "
		"posX DECIMAL(15, 7) NOT NULL, "
		"posY DECIMAL(15, 7) NOT NULL, "
		"posZ DECIMAL(15, 7) NOT NULL, "
		"hp INT NOT NULL, "
		"breath INT NOT NULL, "
		"creation_date DATETIME NOT NULL DEFAULT NOW(), "
		"modification_date DATETIME NOT NULL DEFAULT NOW(), "
		"PRIMARY KEY (name)"
	);

	createTable("player_inventories",
		"player VARCHAR(" + std::to_string(PLAYERNAME_SIZE) + ") NOT NULL, "
		"inv_id INT NOT NULL, "
		"inv_width INT NOT NULL, "
		"inv_name TEXT NOT NULL DEFAULT '', "
		"inv_size INT NOT NULL, "
		"PRIMARY KEY(player, inv_id), "
		"INDEX idx_player (player), "
		"CONSTRAINT player_inventories_fkey FOREIGN KEY (player) REFERENCES player (name) ON DELETE CASCADE"
	);

	createTable("player_inventory_items",
		"player VARCHAR(" + std::to_string(PLAYERNAME_SIZE) + ") NOT NULL, "
		"inv_id INT NOT NULL, "
		"slot_id INT NOT NULL, "
		"item TEXT NOT NULL DEFAULT '', "
		"PRIMARY KEY(player, inv_id, slot_id), "
		"INDEX idx_player(player), "
		"CONSTRAINT player_inventory_items_fkey FOREIGN KEY (player) REFERENCES player (name) ON DELETE CASCADE"
	);

	createTable("player_metadata",
		"player VARCHAR(" + std::to_string(PLAYERNAME_SIZE) + ") NOT NULL, "
		"attr TEXT NOT NULL, "
		"value TEXT NOT NULL DEFAULT '', "
		"PRIMARY KEY(player, attr(192)), "
		"INDEX idx_player (player), "
		"CONSTRAINT player_metadata_fkey FOREIGN KEY (player) REFERENCES player (name) ON DELETE CASCADE"
	);

	m_stmt_save_player = prepareStatement(
		"INSERT INTO player (name, pitch, yaw, posX, posY, posZ, hp, breath) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE "
		"pitch = VALUES(pitch), yaw = VALUES(yaw), "
		"posX = VALUES(posX), posY = VALUES(posY), posZ = VALUES(posZ), "
		"hp = VALUES(hp), breath = VALUES(breath)");
	m_stmt_load_player = prepareStatement(
		"SELECT pitch, yaw, posX, posY, posZ, hp, breath FROM player WHERE name = ?");
	m_stmt_load_inv = prepareStatement(
		"SELECT inv_id, inv_width, inv_name, inv_size "
		"FROM player_inventories WHERE player = ? ORDER BY inv_id");
	m_stmt_load_inv_items = prepareStatement(
		"SELECT slot_id, item FROM player_inventory_items "
		"WHERE player = ? AND inv_id = ?");
	m_stmt_load_player_list = prepareStatement(
		"SELECT name FROM player");
	m_stmt_load_meta = prepareStatement(
		"SELECT attr, value FROM player_metadata WHERE player = ?");
	m_stmt_remove_player = prepareStatement(
		"DELETE FROM player WHERE name = ?");
	m_stmt_remove_inv = prepareStatement(
		"DELETE FROM player_inventories WHERE player = ?");
	m_stmt_remove_inv_items = prepareStatement(
		"DELETE FROM player_inventory_items WHERE player = ?");
	m_stmt_remove_meta = prepareStatement(
		"DELETE FROM player_metadata WHERE player = ?");
	m_stmt_add_inv = prepareStatement(
		"INSERT INTO player_inventories (player, inv_id, inv_width, inv_name, inv_size) "
		"VALUES (?, ?, ?, ?, ?)");
	m_stmt_add_inv_item = prepareStatement(
		"INSERT INTO player_inventory_items (player, inv_id, slot_id, item) "
		"VALUES (?, ?, ?, ?)");
	m_stmt_save_meta = prepareStatement(
		"INSERT INTO player_metadata (player, attr, value) VALUES (?, ?, ?)");
}

PlayerDatabaseMariaDB::~PlayerDatabaseMariaDB()
{
	closeStmt(m_stmt_save_player);
	closeStmt(m_stmt_load_player);
	closeStmt(m_stmt_load_inv);
	closeStmt(m_stmt_load_inv_items);
	closeStmt(m_stmt_load_player_list);
	closeStmt(m_stmt_load_meta);
	closeStmt(m_stmt_remove_player);
	closeStmt(m_stmt_remove_inv);
	closeStmt(m_stmt_remove_inv_items);
	closeStmt(m_stmt_remove_meta);
	closeStmt(m_stmt_add_inv);
	closeStmt(m_stmt_add_inv_item);
	closeStmt(m_stmt_save_meta);
}

bool PlayerDatabaseMariaDB::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	sanity_check(sao);
	checkConnection();

	std::string player_name(player->getName());
	unsigned long name_len = player_name.size();

	// Query player
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);

	double pitch, yaw, px, py, pz;
	int32_t hp, breath;
	MYSQL_BIND res[7] = {};
	initBind(res[0], MYSQL_TYPE_DOUBLE, &pitch, sizeof(pitch));
	initBind(res[1], MYSQL_TYPE_DOUBLE, &yaw, sizeof(yaw));
	initBind(res[2], MYSQL_TYPE_DOUBLE, &px, sizeof(px));
	initBind(res[3], MYSQL_TYPE_DOUBLE, &py, sizeof(py));
	initBind(res[4], MYSQL_TYPE_DOUBLE, &pz, sizeof(pz));
	initBind(res[5], MYSQL_TYPE_LONG, &hp, sizeof(hp));
	initBind(res[6], MYSQL_TYPE_LONG, &breath, sizeof(breath));

	int rows = stmtQuery(m_stmt_load_player, param, res);
	StmtResultGuard player_guard(m_stmt_load_player);

	if (rows == 0)
		return false;

	mysql_stmt_fetch(m_stmt_load_player);

	sao->setLookPitch((float)pitch);
	sao->setRotation(v3f(0, (float)yaw, 0));
	sao->setBasePosition(v3f((float)px, (float)py, (float)pz));
	sao->setHPRaw((u16)MYMIN(hp, U16_MAX));
	sao->setBreath((u16)MYMIN(breath, U16_MAX), false);

	// Load inventories
	MYSQL_BIND inv_param[1] = {};
	initBind(inv_param[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);

	int32_t inv_id, inv_width, inv_size;
	char inv_name_buf[256];
	unsigned long inv_name_len = 0;
	MYSQL_BIND inv_res[4] = {};
	initBind(inv_res[0], MYSQL_TYPE_LONG, &inv_id, sizeof(inv_id));
	initBind(inv_res[1], MYSQL_TYPE_LONG, &inv_width, sizeof(inv_width));
	initBind(inv_res[2], MYSQL_TYPE_STRING, inv_name_buf, sizeof(inv_name_buf), &inv_name_len);
	initBind(inv_res[3], MYSQL_TYPE_LONG, &inv_size, sizeof(inv_size));

	stmtQuery(m_stmt_load_inv, inv_param, inv_res);
	StmtResultGuard inv_guard(m_stmt_load_inv);

	while (mysql_stmt_fetch(m_stmt_load_inv) == 0) {
		std::string inv_name(inv_name_buf, inv_name_len);
		InventoryList *inventoryList = player->inventory.addList(inv_name, inv_size);
		inventoryList->setWidth(inv_width);

		// Load items for this inventory
		unsigned long item_name_len = name_len;
		MYSQL_BIND item_param[2] = {};
		initBind(item_param[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &item_name_len);
		initBind(item_param[1], MYSQL_TYPE_LONG, &inv_id, sizeof(inv_id));

		int32_t slot_id;
		char item_buf[1024];
		unsigned long item_len = 0;
		MYSQL_BIND item_res[2] = {};
		initBind(item_res[0], MYSQL_TYPE_LONG, &slot_id, sizeof(slot_id));
		initBind(item_res[1], MYSQL_TYPE_STRING, item_buf, sizeof(item_buf), &item_len);

		stmtQuery(m_stmt_load_inv_items, item_param, item_res);
		StmtResultGuard item_guard(m_stmt_load_inv_items);

		int item_rc;
		while ((item_rc = mysql_stmt_fetch(m_stmt_load_inv_items)) == 0 ||
				item_rc == MYSQL_DATA_TRUNCATED) {
			std::string item;
			if (item_rc == MYSQL_DATA_TRUNCATED && item_len > sizeof(item_buf)) {
				item.resize(item_len);
				MYSQL_BIND col = {};
				initBind(col, MYSQL_TYPE_STRING, item.data(), item_len, &item_len);
				stmtCheck(m_stmt_load_inv_items,
					mysql_stmt_fetch_column(m_stmt_load_inv_items, &col, 1, 0),
					"fetch_column");
			} else {
				item.assign(item_buf, item_len);
			}
			if (!item.empty()) {
				ItemStack stack;
				stack.deSerialize(item);
				inventoryList->changeItem((u16)slot_id, stack);
			}
		}
	}

	// Load metadata
	MYSQL_BIND meta_param[1] = {};
	unsigned long meta_name_len = name_len;
	initBind(meta_param[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &meta_name_len);

	char attr_buf[256], value_buf[4096];
	unsigned long attr_len = 0, value_len = 0;
	MYSQL_BIND meta_res[2] = {};
	initBind(meta_res[0], MYSQL_TYPE_STRING, attr_buf, sizeof(attr_buf), &attr_len);
	initBind(meta_res[1], MYSQL_TYPE_STRING, value_buf, sizeof(value_buf), &value_len);

	stmtQuery(m_stmt_load_meta, meta_param, meta_res);
	StmtResultGuard meta_guard(m_stmt_load_meta);

	while (mysql_stmt_fetch(m_stmt_load_meta) == 0) {
		sao->getMeta().setString(
			std::string(attr_buf, attr_len),
			std::string(value_buf, value_len));
	}

	sao->getMeta().setModified(false);

	return true;
}

void PlayerDatabaseMariaDB::listPlayers(std::vector<std::string> &list)
{
	checkConnection();

	char name_buf[PLAYERNAME_SIZE + 1];
	unsigned long name_len = 0;
	MYSQL_BIND res[1] = {};
	initBind(res[0], MYSQL_TYPE_STRING, name_buf, sizeof(name_buf), &name_len);

	stmtQuery(m_stmt_load_player_list, nullptr, res);
	StmtResultGuard guard(m_stmt_load_player_list);

	while (mysql_stmt_fetch(m_stmt_load_player_list) == 0)
		list.emplace_back(name_buf, name_len);
}

bool PlayerDatabaseMariaDB::playerDataExists(const std::string &player_name)
{
	checkConnection();

	unsigned long name_len = player_name.size();
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);

	double pitch, yaw, px, py, pz;
	int32_t hp, breath;
	MYSQL_BIND res[7] = {};
	initBind(res[0], MYSQL_TYPE_DOUBLE, &pitch, sizeof(pitch));
	initBind(res[1], MYSQL_TYPE_DOUBLE, &yaw, sizeof(yaw));
	initBind(res[2], MYSQL_TYPE_DOUBLE, &px, sizeof(px));
	initBind(res[3], MYSQL_TYPE_DOUBLE, &py, sizeof(py));
	initBind(res[4], MYSQL_TYPE_DOUBLE, &pz, sizeof(pz));
	initBind(res[5], MYSQL_TYPE_LONG, &hp, sizeof(hp));
	initBind(res[6], MYSQL_TYPE_LONG, &breath, sizeof(breath));

	int rows = stmtQuery(m_stmt_load_player, param, res);
	StmtResultGuard guard(m_stmt_load_player);

	return rows > 0;
}

bool PlayerDatabaseMariaDB::removePlayer(const std::string &player_name)
{
	if (!playerDataExists(player_name))
		return false;

	unsigned long name_len = player_name.size();
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);

	stmtExec(m_stmt_remove_player, param);
	return true;
}

void PlayerDatabaseMariaDB::savePlayer(RemotePlayer *player)
{
	PlayerSAO *sao = player->getPlayerSAO();
	if (!sao)
		return;

	checkConnection();

	std::string player_name(player->getName());
	unsigned long name_len = player_name.size();
	v3f pos = sao->getBasePosition();
	double pitch = sao->getLookPitch();
	double yaw = sao->getRotation().Y;
	double fpx = pos.X, fpy = pos.Y, fpz = pos.Z;
	int32_t hp = sao->getHP();
	int32_t breath = sao->getBreath();

	beginTransaction();

	// Save player
	MYSQL_BIND params[8] = {};
	initBind(params[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);
	initBind(params[1], MYSQL_TYPE_DOUBLE, &pitch, sizeof(pitch));
	initBind(params[2], MYSQL_TYPE_DOUBLE, &yaw, sizeof(yaw));
	initBind(params[3], MYSQL_TYPE_DOUBLE, &fpx, sizeof(fpx));
	initBind(params[4], MYSQL_TYPE_DOUBLE, &fpy, sizeof(fpy));
	initBind(params[5], MYSQL_TYPE_DOUBLE, &fpz, sizeof(fpz));
	initBind(params[6], MYSQL_TYPE_LONG, &hp, sizeof(hp));
	initBind(params[7], MYSQL_TYPE_LONG, &breath, sizeof(breath));

	stmtExec(m_stmt_save_player, params);

	// Delete old inventories
	MYSQL_BIND name_param[1] = {};
	initBind(name_param[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);

	stmtExec(m_stmt_remove_inv, name_param);
	stmtExec(m_stmt_remove_inv_items, name_param);

	// Save inventories
	const auto &inventory_lists = sao->getInventory()->getLists();
	std::ostringstream oss;
	for (u16 i = 0; i < inventory_lists.size(); i++) {
		const InventoryList *list = inventory_lists[i];
		std::string list_name = list->getName();
		unsigned long list_name_len = list_name.size();
		int32_t inv_id = i;
		int32_t width = list->getWidth();
		int32_t size = list->getSize();

		MYSQL_BIND inv_params[5] = {};
		initBind(inv_params[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);
		initBind(inv_params[1], MYSQL_TYPE_LONG, &inv_id, sizeof(inv_id));
		initBind(inv_params[2], MYSQL_TYPE_LONG, &width, sizeof(width));
		initBind(inv_params[3], MYSQL_TYPE_STRING, (void *)list_name.c_str(), list_name_len, &list_name_len);
		initBind(inv_params[4], MYSQL_TYPE_LONG, &size, sizeof(size));

		stmtExec(m_stmt_add_inv, inv_params);

		for (u32 j = 0; j < list->getSize(); j++) {
			oss.str("");
			oss.clear();
			list->getItem(j).serialize(oss);
			std::string item_str = oss.str();
			unsigned long item_len = item_str.size();
			int32_t slot_id = j;

			MYSQL_BIND item_params[4] = {};
			initBind(item_params[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);
			initBind(item_params[1], MYSQL_TYPE_LONG, &inv_id, sizeof(inv_id));
			initBind(item_params[2], MYSQL_TYPE_LONG, &slot_id, sizeof(slot_id));
			initBind(item_params[3], MYSQL_TYPE_STRING, (void *)item_str.c_str(), item_len, &item_len);

			stmtExec(m_stmt_add_inv_item, item_params);
		}
	}

	// Save metadata
	stmtExec(m_stmt_remove_meta, name_param);

	const StringMap &attrs = sao->getMeta().getStrings();
	for (const auto &attr : attrs) {
		unsigned long attr_len = attr.first.size();
		unsigned long val_len = attr.second.size();

		MYSQL_BIND meta_params[3] = {};
		initBind(meta_params[0], MYSQL_TYPE_STRING, (void *)player_name.c_str(), name_len, &name_len);
		initBind(meta_params[1], MYSQL_TYPE_STRING, (void *)attr.first.c_str(), attr_len, &attr_len);
		initBind(meta_params[2], MYSQL_TYPE_STRING, (void *)attr.second.c_str(), val_len, &val_len);

		stmtExec(m_stmt_save_meta, meta_params);
	}

	commit();

	player->onSuccessfulSave();
}


/*
 * AuthDatabaseMariaDB
 */

AuthDatabaseMariaDB::AuthDatabaseMariaDB(const std::string &connect_string)
	: Database_MariaDB(connect_string, "_auth"), AuthDatabase()
{
	connect();

	createTable("auth",
		"id INT(10) UNSIGNED NOT NULL AUTO_INCREMENT, "
		"name VARCHAR(" + std::to_string(PLAYERNAME_SIZE) + ") UNIQUE NOT NULL, "
		"password VARCHAR(512) NOT NULL, "
		"last_login INT NOT NULL DEFAULT 0, "
		"PRIMARY KEY (id), "
		"INDEX idx_name (name)"
	);

	createTable("user_privileges",
		"id INT(10) UNSIGNED NOT NULL, "
		"privilege VARCHAR(255) NOT NULL, "
		"PRIMARY KEY (id, privilege), "
		"INDEX idx_id (id), "
		"CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES auth (id) ON DELETE CASCADE"
	);

	m_stmt_create_auth = prepareStatement(
		"INSERT INTO auth (name, password, last_login) VALUES (?, ?, ?)");
	m_stmt_delete_auth = prepareStatement(
		"DELETE FROM auth WHERE name = ?");
	m_stmt_delete_privs = prepareStatement(
		"DELETE FROM user_privileges WHERE id = ?");
	m_stmt_list_names = prepareStatement(
		"SELECT name FROM auth ORDER BY name DESC");
	m_stmt_read_auth = prepareStatement(
		"SELECT id, name, password, last_login FROM auth WHERE name = ?");
	m_stmt_read_privs = prepareStatement(
		"SELECT privilege FROM user_privileges WHERE id = ?");
	m_stmt_write_auth = prepareStatement(
		"UPDATE auth SET name = ?, password = ?, last_login = ? WHERE id = ?");
	m_stmt_write_privs = prepareStatement(
		"INSERT INTO user_privileges (id, privilege) VALUES (?, ?)");
}

AuthDatabaseMariaDB::~AuthDatabaseMariaDB()
{
	closeStmt(m_stmt_create_auth);
	closeStmt(m_stmt_delete_auth);
	closeStmt(m_stmt_delete_privs);
	closeStmt(m_stmt_list_names);
	closeStmt(m_stmt_read_auth);
	closeStmt(m_stmt_read_privs);
	closeStmt(m_stmt_write_auth);
	closeStmt(m_stmt_write_privs);
}

bool AuthDatabaseMariaDB::getAuth(const std::string &name, AuthEntry &res)
{
	checkConnection();

	unsigned long name_len = name.size();
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)name.c_str(), name_len, &name_len);

	int32_t id, last_login;
	char name_buf[PLAYERNAME_SIZE + 1];
	char pass_buf[512];
	unsigned long res_name_len = 0, pass_len = 0;
	MYSQL_BIND result[4] = {};
	initBind(result[0], MYSQL_TYPE_LONG, &id, sizeof(id));
	result[0].is_unsigned = 1;
	initBind(result[1], MYSQL_TYPE_STRING, name_buf, sizeof(name_buf), &res_name_len);
	initBind(result[2], MYSQL_TYPE_STRING, pass_buf, sizeof(pass_buf), &pass_len);
	initBind(result[3], MYSQL_TYPE_LONG, &last_login, sizeof(last_login));

	int rows = stmtQuery(m_stmt_read_auth, param, result);
	StmtResultGuard guard(m_stmt_read_auth);

	if (rows == 0)
		return false;

	mysql_stmt_fetch(m_stmt_read_auth);

	res.id = (u64)id;
	res.name.assign(name_buf, res_name_len);
	res.password.assign(pass_buf, pass_len);
	res.last_login = last_login;

	// Read privileges
	MYSQL_BIND priv_param[1] = {};
	initBind(priv_param[0], MYSQL_TYPE_LONG, &id, sizeof(id));
	priv_param[0].is_unsigned = 1;

	char priv_buf[256];
	unsigned long priv_len = 0;
	MYSQL_BIND priv_res[1] = {};
	initBind(priv_res[0], MYSQL_TYPE_STRING, priv_buf, sizeof(priv_buf), &priv_len);

	stmtQuery(m_stmt_read_privs, priv_param, priv_res);
	StmtResultGuard priv_guard(m_stmt_read_privs);

	while (mysql_stmt_fetch(m_stmt_read_privs) == 0)
		res.privileges.emplace_back(priv_buf, priv_len);

	return true;
}

bool AuthDatabaseMariaDB::saveAuth(const AuthEntry &authEntry)
{
	checkConnection();

	beginTransaction();

	unsigned long name_len = authEntry.name.size();
	unsigned long pass_len = authEntry.password.size();
	int32_t last_login = authEntry.last_login;
	int32_t id = authEntry.id;

	MYSQL_BIND params[4] = {};
	initBind(params[0], MYSQL_TYPE_STRING, (void *)authEntry.name.c_str(), name_len, &name_len);
	initBind(params[1], MYSQL_TYPE_STRING, (void *)authEntry.password.c_str(), pass_len, &pass_len);
	initBind(params[2], MYSQL_TYPE_LONG, &last_login, sizeof(last_login));
	initBind(params[3], MYSQL_TYPE_LONG, &id, sizeof(id));
	params[3].is_unsigned = 1;

	stmtExec(m_stmt_write_auth, params);

	writePrivileges(authEntry);

	commit();
	return true;
}

bool AuthDatabaseMariaDB::createAuth(AuthEntry &authEntry)
{
	checkConnection();

	beginTransaction();

	unsigned long name_len = authEntry.name.size();
	unsigned long pass_len = authEntry.password.size();
	int32_t last_login = authEntry.last_login;

	MYSQL_BIND params[3] = {};
	initBind(params[0], MYSQL_TYPE_STRING, (void *)authEntry.name.c_str(), name_len, &name_len);
	initBind(params[1], MYSQL_TYPE_STRING, (void *)authEntry.password.c_str(), pass_len, &pass_len);
	initBind(params[2], MYSQL_TYPE_LONG, &last_login, sizeof(last_login));

	stmtExec(m_stmt_create_auth, params);
	authEntry.id = lastInsertId();

	writePrivileges(authEntry);

	commit();
	return true;
}

bool AuthDatabaseMariaDB::deleteAuth(const std::string &name)
{
	checkConnection();

	unsigned long name_len = name.size();
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)name.c_str(), name_len, &name_len);

	stmtExec(m_stmt_delete_auth, param);
	return mysql_stmt_affected_rows(m_stmt_delete_auth) > 0;
}

void AuthDatabaseMariaDB::listNames(std::vector<std::string> &res)
{
	checkConnection();

	char name_buf[PLAYERNAME_SIZE + 1];
	unsigned long name_len = 0;
	MYSQL_BIND result[1] = {};
	initBind(result[0], MYSQL_TYPE_STRING, name_buf, sizeof(name_buf), &name_len);

	stmtQuery(m_stmt_list_names, nullptr, result);
	StmtResultGuard guard(m_stmt_list_names);

	while (mysql_stmt_fetch(m_stmt_list_names) == 0)
		res.emplace_back(name_buf, name_len);
}

void AuthDatabaseMariaDB::reload() {}

void AuthDatabaseMariaDB::writePrivileges(const AuthEntry &authEntry)
{
	checkConnection();

	int32_t id = authEntry.id;
	MYSQL_BIND del_param[1] = {};
	initBind(del_param[0], MYSQL_TYPE_LONG, &id, sizeof(id));
	del_param[0].is_unsigned = 1;

	stmtExec(m_stmt_delete_privs, del_param);

	for (const std::string &privilege : authEntry.privileges) {
		unsigned long priv_len = privilege.size();

		MYSQL_BIND params[2] = {};
		initBind(params[0], MYSQL_TYPE_LONG, &id, sizeof(id));
		params[0].is_unsigned = 1;
		initBind(params[1], MYSQL_TYPE_STRING, (void *)privilege.c_str(), priv_len, &priv_len);

		stmtExec(m_stmt_write_privs, params);
	}
}


/*
 * ModStorageDatabaseMariaDB
 */

ModStorageDatabaseMariaDB::ModStorageDatabaseMariaDB(const std::string &connect_string)
	: Database_MariaDB(connect_string, "_mod_storage"), ModStorageDatabase()
{
	connect();

	createTable("mod_storage",
		"mod_name VARCHAR(192) NOT NULL, "
		"mod_key VARCHAR(192) NOT NULL, "
		"mod_value TEXT NOT NULL DEFAULT '', "
		"PRIMARY KEY (mod_name, mod_key)"
	);

	m_stmt_get_entries = prepareStatement(
		"SELECT mod_key, mod_value FROM mod_storage WHERE mod_name = ?");
	m_stmt_get_keys = prepareStatement(
		"SELECT mod_key FROM mod_storage WHERE mod_name = ?");
	m_stmt_get_entry = prepareStatement(
		"SELECT mod_value FROM mod_storage WHERE mod_name = ? AND mod_key = ?");
	m_stmt_has_entry = prepareStatement(
		"SELECT 1 FROM mod_storage WHERE mod_name = ? AND mod_key = ? LIMIT 1");
	m_stmt_list_mods = prepareStatement(
		"SELECT DISTINCT mod_name FROM mod_storage");
	m_stmt_remove_entry = prepareStatement(
		"DELETE FROM mod_storage WHERE mod_name = ? AND mod_key = ?");
	m_stmt_remove_entries = prepareStatement(
		"DELETE FROM mod_storage WHERE mod_name = ?");
	m_stmt_set_entry = prepareStatement(
		"INSERT INTO mod_storage (mod_name, mod_key, mod_value) VALUES (?, ?, ?) "
		"ON DUPLICATE KEY UPDATE mod_value = VALUES(mod_value)");
}

ModStorageDatabaseMariaDB::~ModStorageDatabaseMariaDB()
{
	closeStmt(m_stmt_get_entries);
	closeStmt(m_stmt_get_keys);
	closeStmt(m_stmt_get_entry);
	closeStmt(m_stmt_has_entry);
	closeStmt(m_stmt_list_mods);
	closeStmt(m_stmt_remove_entry);
	closeStmt(m_stmt_remove_entries);
	closeStmt(m_stmt_set_entry);
}

void ModStorageDatabaseMariaDB::getModEntries(const std::string &modname, StringMap *storage)
{
	checkConnection();

	unsigned long modname_len = modname.size();
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)modname.c_str(), modname_len, &modname_len);

	// Keys are VARCHAR(192), values are TEXT (up to 16MB).
	// Use stack buffers for the common case, re-fetch on truncation.
	char key_buf[193], value_buf[4096];
	unsigned long key_len = 0, value_len = 0;
	MYSQL_BIND res[2] = {};
	initBind(res[0], MYSQL_TYPE_STRING, key_buf, sizeof(key_buf), &key_len);
	initBind(res[1], MYSQL_TYPE_STRING, value_buf, sizeof(value_buf), &value_len);

	stmtQuery(m_stmt_get_entries, param, res);
	StmtResultGuard guard(m_stmt_get_entries);

	int rc;
	while ((rc = mysql_stmt_fetch(m_stmt_get_entries)) == 0 ||
			rc == MYSQL_DATA_TRUNCATED) {
		std::string key(key_buf, std::min(key_len, (unsigned long)sizeof(key_buf)));
		if (rc == MYSQL_DATA_TRUNCATED && value_len > sizeof(value_buf)) {
			std::string value(value_len, '\0');
			MYSQL_BIND col = {};
			initBind(col, MYSQL_TYPE_STRING, value.data(), value_len, &value_len);
			stmtCheck(m_stmt_get_entries,
				mysql_stmt_fetch_column(m_stmt_get_entries, &col, 1, 0),
				"fetch_column");
			(*storage)[key] = std::move(value);
		} else {
			(*storage)[key] = std::string(value_buf, value_len);
		}
	}
}

bool ModStorageDatabaseMariaDB::getModEntry(const std::string &modname,
	const std::string &key, std::string *value)
{
	checkConnection();

	unsigned long modname_len = modname.size();
	unsigned long key_len = key.size();
	MYSQL_BIND params[2] = {};
	initBind(params[0], MYSQL_TYPE_STRING, (void *)modname.c_str(), modname_len, &modname_len);
	initBind(params[1], MYSQL_TYPE_STRING, (void *)key.c_str(), key_len, &key_len);

	char value_buf[4096];
	unsigned long value_len = 0;
	MYSQL_BIND res[1] = {};
	initBind(res[0], MYSQL_TYPE_STRING, value_buf, sizeof(value_buf), &value_len);

	int rows = stmtQuery(m_stmt_get_entry, params, res);
	StmtResultGuard guard(m_stmt_get_entry);

	if (rows == 0)
		return false;

	int rc = mysql_stmt_fetch(m_stmt_get_entry);
	if (rc == 0) {
		value->assign(value_buf, value_len);
	} else if (rc == MYSQL_DATA_TRUNCATED) {
		value->resize(value_len);
		MYSQL_BIND col = {};
		initBind(col, MYSQL_TYPE_STRING, value->data(), value_len, &value_len);
		stmtCheck(m_stmt_get_entry,
			mysql_stmt_fetch_column(m_stmt_get_entry, &col, 0, 0),
			"fetch_column");
	}

	return true;
}

void ModStorageDatabaseMariaDB::getModKeys(const std::string &modname,
	std::vector<std::string> *storage)
{
	checkConnection();

	unsigned long modname_len = modname.size();
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)modname.c_str(), modname_len, &modname_len);

	char key_buf[193]; // mod_key is VARCHAR(192)
	unsigned long key_len = 0;
	MYSQL_BIND res[1] = {};
	initBind(res[0], MYSQL_TYPE_STRING, key_buf, sizeof(key_buf), &key_len);

	int rows = stmtQuery(m_stmt_get_keys, param, res);
	StmtResultGuard guard(m_stmt_get_keys);
	storage->reserve(storage->size() + rows);

	while (mysql_stmt_fetch(m_stmt_get_keys) == 0)
		storage->emplace_back(key_buf, key_len);
}

bool ModStorageDatabaseMariaDB::hasModEntry(const std::string &modname,
	const std::string &key)
{
	checkConnection();

	unsigned long modname_len = modname.size();
	unsigned long key_len = key.size();
	MYSQL_BIND params[2] = {};
	initBind(params[0], MYSQL_TYPE_STRING, (void *)modname.c_str(), modname_len, &modname_len);
	initBind(params[1], MYSQL_TYPE_STRING, (void *)key.c_str(), key_len, &key_len);

	int32_t dummy;
	MYSQL_BIND res[1] = {};
	initBind(res[0], MYSQL_TYPE_LONG, &dummy, sizeof(dummy));

	int rows = stmtQuery(m_stmt_has_entry, params, res);
	StmtResultGuard guard(m_stmt_has_entry);

	return rows > 0;
}

void ModStorageDatabaseMariaDB::listMods(std::vector<std::string> *res)
{
	checkConnection();

	char name_buf[192];
	unsigned long name_len = 0;
	MYSQL_BIND result[1] = {};
	initBind(result[0], MYSQL_TYPE_STRING, name_buf, sizeof(name_buf), &name_len);

	stmtQuery(m_stmt_list_mods, nullptr, result);
	StmtResultGuard guard(m_stmt_list_mods);

	while (mysql_stmt_fetch(m_stmt_list_mods) == 0)
		res->emplace_back(name_buf, name_len);
}

bool ModStorageDatabaseMariaDB::removeModEntries(const std::string &modname)
{
	checkConnection();

	unsigned long modname_len = modname.size();
	MYSQL_BIND param[1] = {};
	initBind(param[0], MYSQL_TYPE_STRING, (void *)modname.c_str(), modname_len, &modname_len);

	stmtExec(m_stmt_remove_entries, param);
	return mysql_stmt_affected_rows(m_stmt_remove_entries) > 0;
}

bool ModStorageDatabaseMariaDB::removeModEntry(const std::string &modname,
	const std::string &key)
{
	checkConnection();

	unsigned long modname_len = modname.size();
	unsigned long key_len = key.size();
	MYSQL_BIND params[2] = {};
	initBind(params[0], MYSQL_TYPE_STRING, (void *)modname.c_str(), modname_len, &modname_len);
	initBind(params[1], MYSQL_TYPE_STRING, (void *)key.c_str(), key_len, &key_len);

	stmtExec(m_stmt_remove_entry, params);
	return mysql_stmt_affected_rows(m_stmt_remove_entry) > 0;
}

bool ModStorageDatabaseMariaDB::setModEntry(const std::string &modname,
	const std::string &key, std::string_view value)
{
	checkConnection();

	unsigned long modname_len = modname.size();
	unsigned long key_len = key.size();
	unsigned long value_len = value.size();
	MYSQL_BIND params[3] = {};
	initBind(params[0], MYSQL_TYPE_STRING, (void *)modname.c_str(), modname_len, &modname_len);
	initBind(params[1], MYSQL_TYPE_STRING, (void *)key.c_str(), key_len, &key_len);
	initBind(params[2], MYSQL_TYPE_STRING, (void *)value.data(), value_len, &value_len);

	stmtExec(m_stmt_set_entry, params);
	return true;
}

#endif // USE_MARIADB
