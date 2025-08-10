// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 Loic Blot <loic.blot@unix-experience.fr>

#include "config.h"

#if USE_POSTGRESQL

#include "database-postgresql.h"

#ifdef _WIN32
	#include <windows.h>
	#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "debug.h"
#include "exceptions.h"
#include "settings.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include <cstdlib>

Database_PostgreSQL::Database_PostgreSQL(const std::string &connect_string,
	const char *type) :
	m_connect_string(connect_string)
{
	if (m_connect_string.empty()) {
		// Use given type to reference the exact setting in the error message
		std::string s = type;
		std::string msg =
			"Set pgsql" + s + "_connection string in world.mt to "
			"use the postgresql backend\n"
			"Notes:\n"
			"pgsql" + s + "_connection has the following form: \n"
			"\tpgsql" + s + "_connection = host=127.0.0.1 port=5432 "
			"user=mt_user password=mt_password dbname=minetest" + s + "\n"
			"mt_user should have CREATE TABLE, INSERT, SELECT, UPDATE and "
			"DELETE rights on the database. "
			"Don't create mt_user as a SUPERUSER!";
		throw SettingNotFoundException(msg);
	}
}

Database_PostgreSQL::~Database_PostgreSQL()
{
	PQfinish(m_conn);
}

void Database_PostgreSQL::connectToDatabase()
{
	m_conn = PQconnectdb(m_connect_string.c_str());

	if (PQstatus(m_conn) != CONNECTION_OK) {
		throw DatabaseException(std::string(
			"PostgreSQL database error: ") +
			PQerrorMessage(m_conn));
	}

	m_pgversion = PQserverVersion(m_conn);

	/*
	* We are using UPSERT feature from PostgreSQL 9.5
	* to have the better performance where possible.
	*/
	if (m_pgversion < 90500) {
		warningstream << "Your PostgreSQL server lacks UPSERT "
			<< "support. Use version 9.5 or better if possible."
			<< std::endl;
	}

	infostream << "PostgreSQL Database: Version " << m_pgversion
			<< " Connection made." << std::endl;

	createDatabase();
	initStatements();
}

void Database_PostgreSQL::verifyDatabase()
{
	if (PQstatus(m_conn) == CONNECTION_OK)
		return;

	PQreset(m_conn);
	ping();
}

void Database_PostgreSQL::ping()
{
	if (PQping(m_connect_string.c_str()) != PQPING_OK) {
		throw DatabaseException(std::string(
			"PostgreSQL database error: ") +
			PQerrorMessage(m_conn));
	}
}

bool Database_PostgreSQL::initialized() const
{
	return m_conn && PQstatus(m_conn) == CONNECTION_OK;
}

PGresult *Database_PostgreSQL::checkResults(PGresult *result, bool clear)
{
	ExecStatusType statusType = PQresultStatus(result);

	switch (statusType) {
	case PGRES_COMMAND_OK:
	case PGRES_TUPLES_OK:
		break;
	case PGRES_FATAL_ERROR:
	default:
		throw DatabaseException(
			std::string("PostgreSQL database error: ") +
			PQresultErrorMessage(result));
	}

	if (clear)
		PQclear(result);

	return result;
}

void Database_PostgreSQL::createTableIfNotExists(const std::string &table_name,
		const std::string &definition)
{
	std::string sql_check_table = "SELECT relname FROM pg_class WHERE relname='" +
		table_name + "';";
	PGresult *result = checkResults(PQexec(m_conn, sql_check_table.c_str()), false);

	// If table doesn't exist, create it
	if (!PQntuples(result)) {
		checkResults(PQexec(m_conn, definition.c_str()));
	}

	PQclear(result);
}

void Database_PostgreSQL::beginSave()
{
	verifyDatabase();
	checkResults(PQexec(m_conn, "BEGIN;"));
}

void Database_PostgreSQL::endSave()
{
	checkResults(PQexec(m_conn, "COMMIT;"));
}

void Database_PostgreSQL::rollback()
{
	checkResults(PQexec(m_conn, "ROLLBACK;"));
}

MapDatabasePostgreSQL::MapDatabasePostgreSQL(const std::string &connect_string):
	Database_PostgreSQL(connect_string, ""),
	MapDatabase()
{
	connectToDatabase();
}


void MapDatabasePostgreSQL::createDatabase()
{
	createTableIfNotExists("blocks",
		"CREATE TABLE blocks ("
			"posX smallint NOT NULL,"
			"posY smallint NOT NULL,"
			"posZ smallint NOT NULL,"
			"data BYTEA,"
			"PRIMARY KEY (posX,posY,posZ)"
			");"
	);

	infostream << "PostgreSQL: Map Database was initialized." << std::endl;
}

void MapDatabasePostgreSQL::initStatements()
{
	prepareStatement("read_block",
		"SELECT data FROM blocks "
			"WHERE posX = $1::int4 AND posY = $2::int4 AND "
			"posZ = $3::int4");

	if (getPGVersion() < 90500) {
		prepareStatement("write_block_insert",
			"INSERT INTO blocks (posX, posY, posZ, data) SELECT "
				"$1::int4, $2::int4, $3::int4, $4::bytea "
				"WHERE NOT EXISTS (SELECT true FROM blocks "
				"WHERE posX = $1::int4 AND posY = $2::int4 AND "
				"posZ = $3::int4)");

		prepareStatement("write_block_update",
			"UPDATE blocks SET data = $4::bytea "
				"WHERE posX = $1::int4 AND posY = $2::int4 AND "
				"posZ = $3::int4");
	} else {
		prepareStatement("write_block",
			"INSERT INTO blocks (posX, posY, posZ, data) VALUES "
				"($1::int4, $2::int4, $3::int4, $4::bytea) "
				"ON CONFLICT ON CONSTRAINT blocks_pkey DO "
				"UPDATE SET data = $4::bytea");
	}

	prepareStatement("delete_block", "DELETE FROM blocks WHERE "
		"posX = $1::int4 AND posY = $2::int4 AND posZ = $3::int4");

	prepareStatement("list_all_loadable_blocks",
		"SELECT posX, posY, posZ FROM blocks");
}

bool MapDatabasePostgreSQL::saveBlock(const v3s16 &pos, std::string_view data)
{
	// Verify if we don't overflow the platform integer with the mapblock size
	if (data.size() > INT_MAX) {
		errorstream << "Database_PostgreSQL::saveBlock: Data truncation! "
			<< "data.size() over 0xFFFFFFFF (== " << data.size()
			<< ")" << std::endl;
		return false;
	}

	verifyDatabase();

	s32 x, y, z;
	x = htonl(pos.X);
	y = htonl(pos.Y);
	z = htonl(pos.Z);

	const void *args[] = { &x, &y, &z, data.data() };
	const int argLen[] = {
		sizeof(x), sizeof(y), sizeof(z), (int)data.size()
	};
	const int argFmt[] = { 1, 1, 1, 1 };

	if (getPGVersion() < 90500) {
		execPrepared("write_block_update", ARRLEN(args), args, argLen, argFmt);
		execPrepared("write_block_insert", ARRLEN(args), args, argLen, argFmt);
	} else {
		execPrepared("write_block", ARRLEN(args), args, argLen, argFmt);
	}
	return true;
}

void MapDatabasePostgreSQL::loadBlock(const v3s16 &pos, std::string *block)
{
	verifyDatabase();

	s32 x, y, z;
	x = htonl(pos.X);
	y = htonl(pos.Y);
	z = htonl(pos.Z);

	const void *args[] = { &x, &y, &z };
	const int argLen[] = { sizeof(x), sizeof(y), sizeof(z) };
	const int argFmt[] = { 1, 1, 1 };

	PGresult *results = execPrepared("read_block", ARRLEN(args), args,
		argLen, argFmt, false);

	if (PQntuples(results))
		*block = pg_to_string(results, 0, 0);
	else
		block->clear();

	PQclear(results);
}

bool MapDatabasePostgreSQL::deleteBlock(const v3s16 &pos)
{
	verifyDatabase();

	s32 x, y, z;
	x = htonl(pos.X);
	y = htonl(pos.Y);
	z = htonl(pos.Z);

	const void *args[] = { &x, &y, &z };
	const int argLen[] = { sizeof(x), sizeof(y), sizeof(z) };
	const int argFmt[] = { 1, 1, 1 };

	execPrepared("delete_block", ARRLEN(args), args, argLen, argFmt);

	return true;
}

void MapDatabasePostgreSQL::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	verifyDatabase();

	PGresult *results = execPrepared("list_all_loadable_blocks", 0,
		NULL, NULL, NULL, false, false);

	int numrows = PQntuples(results);

	for (int row = 0; row < numrows; ++row)
		dst.push_back(pg_to_v3s16(results, row, 0));

	PQclear(results);
}

/*
 * Player Database
 */
PlayerDatabasePostgreSQL::PlayerDatabasePostgreSQL(const std::string &connect_string):
	Database_PostgreSQL(connect_string, "_player"),
	PlayerDatabase()
{
	connectToDatabase();
}


void PlayerDatabasePostgreSQL::createDatabase()
{
	createTableIfNotExists("player",
		"CREATE TABLE player ("
			"name VARCHAR(60) NOT NULL,"
			"pitch NUMERIC(15, 7) NOT NULL,"
			"yaw NUMERIC(15, 7) NOT NULL,"
			"posX NUMERIC(15, 7) NOT NULL,"
			"posY NUMERIC(15, 7) NOT NULL,"
			"posZ NUMERIC(15, 7) NOT NULL,"
			"hp INT NOT NULL,"
			"breath INT NOT NULL,"
			"creation_date TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT NOW(),"
			"modification_date TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT NOW(),"
			"PRIMARY KEY (name)"
			");"
	);

	createTableIfNotExists("player_inventories",
		"CREATE TABLE player_inventories ("
			"player VARCHAR(60) NOT NULL,"
			"inv_id INT NOT NULL,"
			"inv_width INT NOT NULL,"
			"inv_name TEXT NOT NULL DEFAULT '',"
			"inv_size INT NOT NULL,"
			"PRIMARY KEY(player, inv_id),"
			"CONSTRAINT player_inventories_fkey FOREIGN KEY (player) REFERENCES "
			"player (name) ON DELETE CASCADE"
			");"
	);

	createTableIfNotExists("player_inventory_items",
		"CREATE TABLE player_inventory_items ("
			"player VARCHAR(60) NOT NULL,"
			"inv_id INT NOT NULL,"
			"slot_id INT NOT NULL,"
			"item TEXT NOT NULL DEFAULT '',"
			"PRIMARY KEY(player, inv_id, slot_id),"
			"CONSTRAINT player_inventory_items_fkey FOREIGN KEY (player) REFERENCES "
			"player (name) ON DELETE CASCADE"
			");"
	);

	createTableIfNotExists("player_metadata",
		"CREATE TABLE player_metadata ("
			"player VARCHAR(60) NOT NULL,"
			"attr VARCHAR(256) NOT NULL,"
			"value TEXT,"
			"PRIMARY KEY(player, attr),"
			"CONSTRAINT player_metadata_fkey FOREIGN KEY (player) REFERENCES "
			"player (name) ON DELETE CASCADE"
			");"
	);

	infostream << "PostgreSQL: Player Database was inited." << std::endl;
}

void PlayerDatabasePostgreSQL::initStatements()
{
	if (getPGVersion() < 90500) {
		prepareStatement("create_player",
			"INSERT INTO player(name, pitch, yaw, posX, posY, posZ, hp, breath) VALUES "
				"($1, $2, $3, $4, $5, $6, $7::int, $8::int)");

		prepareStatement("update_player",
			"UPDATE SET pitch = $2, yaw = $3, posX = $4, posY = $5, posZ = $6, hp = $7::int, "
				"breath = $8::int, modification_date = NOW() WHERE name = $1");
	} else {
		prepareStatement("save_player",
			"INSERT INTO player(name, pitch, yaw, posX, posY, posZ, hp, breath) VALUES "
				"($1, $2, $3, $4, $5, $6, $7::int, $8::int)"
				"ON CONFLICT ON CONSTRAINT player_pkey DO UPDATE SET pitch = $2, yaw = $3, "
				"posX = $4, posY = $5, posZ = $6, hp = $7::int, breath = $8::int, "
				"modification_date = NOW()");
	}

	prepareStatement("remove_player", "DELETE FROM player WHERE name = $1");

	prepareStatement("load_player_list", "SELECT name FROM player");

	prepareStatement("remove_player_inventories",
		"DELETE FROM player_inventories WHERE player = $1");

	prepareStatement("remove_player_inventory_items",
		"DELETE FROM player_inventory_items WHERE player = $1");

	prepareStatement("add_player_inventory",
		"INSERT INTO player_inventories (player, inv_id, inv_width, inv_name, inv_size) VALUES "
			"($1, $2::int, $3::int, $4, $5::int)");

	prepareStatement("add_player_inventory_item",
		"INSERT INTO player_inventory_items (player, inv_id, slot_id, item) VALUES "
			"($1, $2::int, $3::int, $4)");

	prepareStatement("load_player_inventories",
		"SELECT inv_id, inv_width, inv_name, inv_size FROM player_inventories "
			"WHERE player = $1 ORDER BY inv_id");

	prepareStatement("load_player_inventory_items",
		"SELECT slot_id, item FROM player_inventory_items WHERE "
			"player = $1 AND inv_id = $2::int");

	prepareStatement("load_player",
		"SELECT pitch, yaw, posX, posY, posZ, hp, breath FROM player WHERE name = $1");

	prepareStatement("remove_player_metadata",
		"DELETE FROM player_metadata WHERE player = $1");

	prepareStatement("save_player_metadata",
		"INSERT INTO player_metadata (player, attr, value) VALUES ($1, $2, $3)");

	prepareStatement("load_player_metadata",
		"SELECT attr, value FROM player_metadata WHERE player = $1");

}

bool PlayerDatabasePostgreSQL::playerDataExists(const std::string &playername)
{
	verifyDatabase();

	const char *values[] = { playername.c_str() };
	PGresult *results = execPrepared("load_player", 1, values, false);

	bool res = (PQntuples(results) > 0);
	PQclear(results);
	return res;
}

void PlayerDatabasePostgreSQL::savePlayer(RemotePlayer *player)
{
	PlayerSAO* sao = player->getPlayerSAO();
	if (!sao)
		return;

	verifyDatabase();

	v3f pos = sao->getBasePosition();
	std::string pitch = ftos(sao->getLookPitch());
	std::string yaw = ftos(sao->getRotation().Y);
	std::string posx = ftos(pos.X);
	std::string posy = ftos(pos.Y);
	std::string posz = ftos(pos.Z);
	std::string hp = itos(sao->getHP());
	std::string breath = itos(sao->getBreath());
	const char *values[] = {
		player->getName().c_str(),
		pitch.c_str(),
		yaw.c_str(),
		posx.c_str(), posy.c_str(), posz.c_str(),
		hp.c_str(),
		breath.c_str()
	};

	const char* rmvalues[] = { player->getName().c_str() };
	beginSave();

	if (getPGVersion() < 90500) {
		if (!playerDataExists(player->getName()))
			execPrepared("create_player", 8, values, true, false);
		else
			execPrepared("update_player", 8, values, true, false);
	}
	else
		execPrepared("save_player", 8, values, true, false);

	// Write player inventories
	execPrepared("remove_player_inventories", 1, rmvalues);
	execPrepared("remove_player_inventory_items", 1, rmvalues);

	const auto &inventory_lists = sao->getInventory()->getLists();
	std::ostringstream oss;
	for (u16 i = 0; i < inventory_lists.size(); i++) {
		const InventoryList* list = inventory_lists[i];
		const std::string &name = list->getName();
		std::string width = itos(list->getWidth()),
			inv_id = itos(i), lsize = itos(list->getSize());

		const char* inv_values[] = {
			player->getName().c_str(),
			inv_id.c_str(),
			width.c_str(),
			name.c_str(),
			lsize.c_str()
		};
		execPrepared("add_player_inventory", 5, inv_values);

		for (u32 j = 0; j < list->getSize(); j++) {
			oss.str("");
			oss.clear();
			list->getItem(j).serialize(oss);
			std::string itemStr = oss.str(), slotId = itos(j);

			const char* invitem_values[] = {
				player->getName().c_str(),
				inv_id.c_str(),
				slotId.c_str(),
				itemStr.c_str()
			};
			execPrepared("add_player_inventory_item", 4, invitem_values);
		}
	}

	execPrepared("remove_player_metadata", 1, rmvalues);
	const StringMap &attrs = sao->getMeta().getStrings();
	for (const auto &attr : attrs) {
		const char *meta_values[] = {
			player->getName().c_str(),
			attr.first.c_str(),
			attr.second.c_str()
		};
		execPrepared("save_player_metadata", 3, meta_values);
	}
	endSave();

	player->onSuccessfulSave();
}

bool PlayerDatabasePostgreSQL::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	sanity_check(sao);
	verifyDatabase();

	const char *values[] = { player->getName().c_str() };
	PGresult *results = execPrepared("load_player", 1, values, false, false);

	// Player not found, return not found
	if (!PQntuples(results)) {
		PQclear(results);
		return false;
	}

	sao->setLookPitch(pg_to_float(results, 0, 0));
	sao->setRotation(v3f(0, pg_to_float(results, 0, 1), 0));
	sao->setBasePosition(v3f(
		pg_to_float(results, 0, 2),
		pg_to_float(results, 0, 3),
		pg_to_float(results, 0, 4))
	);
	sao->setHPRaw((u16) pg_to_int(results, 0, 5));
	sao->setBreath((u16) pg_to_int(results, 0, 6), false);

	PQclear(results);

	// Load inventory
	results = execPrepared("load_player_inventories", 1, values, false, false);

	int resultCount = PQntuples(results);

	for (int row = 0; row < resultCount; ++row) {
		InventoryList* invList = player->inventory.
			addList(PQgetvalue(results, row, 2), pg_to_uint(results, row, 3));
		invList->setWidth(pg_to_uint(results, row, 1));

		u32 invId = pg_to_uint(results, row, 0);
		std::string invIdStr = itos(invId);

		const char* values2[] = {
			player->getName().c_str(),
			invIdStr.c_str()
		};
		PGresult *results2 = execPrepared("load_player_inventory_items", 2,
			values2, false, false);

		int resultCount2 = PQntuples(results2);
		for (int row2 = 0; row2 < resultCount2; row2++) {
			const std::string itemStr = PQgetvalue(results2, row2, 1);
			if (itemStr.length() > 0) {
				ItemStack stack;
				stack.deSerialize(itemStr);
				invList->changeItem(pg_to_uint(results2, row2, 0), stack);
			}
		}
		PQclear(results2);
	}

	PQclear(results);

	results = execPrepared("load_player_metadata", 1, values, false);

	int numrows = PQntuples(results);
	for (int row = 0; row < numrows; row++) {
		sao->getMeta().setString(PQgetvalue(results, row, 0), PQgetvalue(results, row, 1));
	}
	sao->getMeta().setModified(false);

	PQclear(results);

	return true;
}

bool PlayerDatabasePostgreSQL::removePlayer(const std::string &name)
{
	if (!playerDataExists(name))
		return false;

	verifyDatabase();

	const char *values[] = { name.c_str() };
	execPrepared("remove_player", 1, values);

	return true;
}

void PlayerDatabasePostgreSQL::listPlayers(std::vector<std::string> &res)
{
	verifyDatabase();

	PGresult *results = execPrepared("load_player_list", 0, NULL, false);

	int numrows = PQntuples(results);
	for (int row = 0; row < numrows; row++)
		res.emplace_back(PQgetvalue(results, row, 0));

	PQclear(results);
}

AuthDatabasePostgreSQL::AuthDatabasePostgreSQL(const std::string &connect_string) :
	Database_PostgreSQL(connect_string, "_auth"),
	AuthDatabase()
{
	connectToDatabase();
}

void AuthDatabasePostgreSQL::createDatabase()
{
	createTableIfNotExists("auth",
		"CREATE TABLE auth ("
			"id SERIAL,"
			"name TEXT UNIQUE,"
			"password TEXT,"
			"last_login INT NOT NULL DEFAULT 0,"
			"PRIMARY KEY (id)"
		");");

	createTableIfNotExists("user_privileges",
		"CREATE TABLE user_privileges ("
			"id INT,"
			"privilege TEXT,"
			"PRIMARY KEY (id, privilege),"
			"CONSTRAINT fk_id FOREIGN KEY (id) REFERENCES auth (id) ON DELETE CASCADE"
		");");
}

void AuthDatabasePostgreSQL::initStatements()
{
	prepareStatement("auth_read", "SELECT id, name, password, last_login FROM auth WHERE name = $1");
	prepareStatement("auth_write", "UPDATE auth SET name = $1, password = $2, last_login = $3 WHERE id = $4");
	prepareStatement("auth_create", "INSERT INTO auth (name, password, last_login) VALUES ($1, $2, $3) RETURNING id");
	prepareStatement("auth_delete", "DELETE FROM auth WHERE name = $1");

	prepareStatement("auth_list_names", "SELECT name FROM auth ORDER BY name DESC");

	prepareStatement("auth_read_privs", "SELECT privilege FROM user_privileges WHERE id = $1");
	prepareStatement("auth_write_privs", "INSERT INTO user_privileges (id, privilege) VALUES ($1, $2)");
	prepareStatement("auth_delete_privs", "DELETE FROM user_privileges WHERE id = $1");
}

bool AuthDatabasePostgreSQL::getAuth(const std::string &name, AuthEntry &res)
{
	verifyDatabase();

	const char *values[] = { name.c_str() };
	PGresult *result = execPrepared("auth_read", 1, values, false, false);
	int numrows = PQntuples(result);
	if (numrows == 0) {
		PQclear(result);
		return false;
	}

	res.id = pg_to_uint(result, 0, 0);
	res.name = pg_to_string(result, 0, 1);
	res.password = pg_to_string(result, 0, 2);
	res.last_login = pg_to_int(result, 0, 3);

	PQclear(result);

	std::string playerIdStr = itos(res.id);
	const char *privsValues[] = { playerIdStr.c_str() };
	PGresult *results = execPrepared("auth_read_privs", 1, privsValues, false);

	numrows = PQntuples(results);
	for (int row = 0; row < numrows; row++)
		res.privileges.emplace_back(PQgetvalue(results, row, 0));

	PQclear(results);

	return true;
}

bool AuthDatabasePostgreSQL::saveAuth(const AuthEntry &authEntry)
{
	verifyDatabase();

	beginSave();

	std::string lastLoginStr = itos(authEntry.last_login);
	std::string idStr = itos(authEntry.id);
	const char *values[] = {
		authEntry.name.c_str() ,
		authEntry.password.c_str(),
		lastLoginStr.c_str(),
		idStr.c_str(),
	};
	execPrepared("auth_write", 4, values);

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabasePostgreSQL::createAuth(AuthEntry &authEntry)
{
	verifyDatabase();

	std::string lastLoginStr = itos(authEntry.last_login);
	const char *values[] = {
		authEntry.name.c_str() ,
		authEntry.password.c_str(),
		lastLoginStr.c_str()
	};

	beginSave();

	PGresult *result = execPrepared("auth_create", 3, values, false, false);

	int numrows = PQntuples(result);
	if (numrows == 0) {
		errorstream << "Strange behavior on auth creation, no ID returned." << std::endl;
		PQclear(result);
		rollback();
		return false;
	}

	authEntry.id = pg_to_uint(result, 0, 0);
	PQclear(result);

	writePrivileges(authEntry);

	endSave();
	return true;
}

bool AuthDatabasePostgreSQL::deleteAuth(const std::string &name)
{
	verifyDatabase();

	const char *values[] = { name.c_str() };
	execPrepared("auth_delete", 1, values);

	// privileges deleted by foreign key on delete cascade
	return true;
}

void AuthDatabasePostgreSQL::listNames(std::vector<std::string> &res)
{
	verifyDatabase();

	PGresult *results = execPrepared("auth_list_names", 0,
		NULL, NULL, NULL, false, false);

	int numrows = PQntuples(results);

	for (int row = 0; row < numrows; ++row)
		res.emplace_back(PQgetvalue(results, row, 0));

	PQclear(results);
}

void AuthDatabasePostgreSQL::reload()
{
	// noop for PgSQL
}

void AuthDatabasePostgreSQL::writePrivileges(const AuthEntry &authEntry)
{
	std::string authIdStr = itos(authEntry.id);
	const char *values[] = { authIdStr.c_str() };
	execPrepared("auth_delete_privs", 1, values);

	for (const std::string &privilege : authEntry.privileges) {
		const char *values[] = { authIdStr.c_str(), privilege.c_str() };
		execPrepared("auth_write_privs", 2, values);
	}
}

ModStorageDatabasePostgreSQL::ModStorageDatabasePostgreSQL(const std::string &connect_string):
	Database_PostgreSQL(connect_string, "_mod_storage"),
	ModStorageDatabase()
{
	connectToDatabase();
}

void ModStorageDatabasePostgreSQL::createDatabase()
{
	createTableIfNotExists("mod_storage",
		"CREATE TABLE mod_storage ("
			"modname TEXT NOT NULL,"
			"key BYTEA NOT NULL,"
			"value BYTEA NOT NULL,"
			"PRIMARY KEY (modname, key)"
		");");

	infostream << "PostgreSQL: Mod Storage Database was initialized." << std::endl;
}

void ModStorageDatabasePostgreSQL::initStatements()
{
	prepareStatement("get_all",
		"SELECT key, value FROM mod_storage WHERE modname = $1");
	prepareStatement("get_all_keys",
		"SELECT key FROM mod_storage WHERE modname = $1");
	prepareStatement("get",
		"SELECT value FROM mod_storage WHERE modname = $1 AND key = $2::bytea");
	prepareStatement("has",
		"SELECT true FROM mod_storage WHERE modname = $1 AND key = $2::bytea");
	if (getPGVersion() < 90500) {
		prepareStatement("set_insert",
			"INSERT INTO mod_storage (modname, key, value) "
				"SELECT $1, $2::bytea, $3::bytea "
					"WHERE NOT EXISTS ("
						"SELECT true FROM mod_storage WHERE modname = $1 AND key = $2::bytea"
					")");
		prepareStatement("set_update",
			"UPDATE mod_storage SET value = $3::bytea WHERE modname = $1 AND key = $2::bytea");
	} else {
		prepareStatement("set",
			"INSERT INTO mod_storage (modname, key, value) VALUES ($1, $2::bytea, $3::bytea) "
				"ON CONFLICT ON CONSTRAINT mod_storage_pkey DO "
					"UPDATE SET value = $3::bytea");
	}
	prepareStatement("remove",
		"DELETE FROM mod_storage WHERE modname = $1 AND key = $2::bytea");
	prepareStatement("remove_all",
		"DELETE FROM mod_storage WHERE modname = $1");
	prepareStatement("list",
		"SELECT DISTINCT modname FROM mod_storage");
}

void ModStorageDatabasePostgreSQL::getModEntries(const std::string &modname, StringMap *storage)
{
	verifyDatabase();

	const void *args[] = { modname.c_str() };
	const int argLen[] = { -1 };
	const int argFmt[] = { 0 };
	PGresult *results = execPrepared("get_all", ARRLEN(args),
			args, argLen, argFmt, false);

	int numrows = PQntuples(results);

	for (int row = 0; row < numrows; ++row)
		(*storage)[pg_to_string(results, row, 0)] = pg_to_string(results, row, 1);

	PQclear(results);
}

void ModStorageDatabasePostgreSQL::getModKeys(const std::string &modname,
		std::vector<std::string> *storage)
{
	verifyDatabase();

	const void *args[] = { modname.c_str() };
	const int argLen[] = { -1 };
	const int argFmt[] = { 0 };
	PGresult *results = execPrepared("get_all_keys", ARRLEN(args),
			args, argLen, argFmt, false);

	int numrows = PQntuples(results);

	storage->reserve(storage->size() + numrows);
	for (int row = 0; row < numrows; ++row)
		storage->push_back(pg_to_string(results, row, 0));

	PQclear(results);
}

bool ModStorageDatabasePostgreSQL::getModEntry(const std::string &modname,
	const std::string &key, std::string *value)
{
	verifyDatabase();

	const void *args[] = { modname.c_str(), key.c_str() };
	const int argLen[] = { -1, (int)MYMIN(key.size(), INT_MAX) };
	const int argFmt[] = { 0, 1 };
	PGresult *results = execPrepared("get", ARRLEN(args), args, argLen, argFmt, false);

	int numrows = PQntuples(results);
	bool found = numrows > 0;

	if (found)
		*value = pg_to_string(results, 0, 0);

	PQclear(results);

	return found;
}

bool ModStorageDatabasePostgreSQL::hasModEntry(const std::string &modname,
		const std::string &key)
{
	verifyDatabase();

	const void *args[] = { modname.c_str(), key.c_str() };
	const int argLen[] = { -1, (int)MYMIN(key.size(), INT_MAX) };
	const int argFmt[] = { 0, 1 };
	PGresult *results = execPrepared("has", ARRLEN(args), args, argLen, argFmt, false);

	int numrows = PQntuples(results);
	bool found = numrows > 0;

	PQclear(results);

	return found;
}

bool ModStorageDatabasePostgreSQL::setModEntry(const std::string &modname,
	const std::string &key, std::string_view value)
{
	verifyDatabase();

	const void *args[] = { modname.c_str(), key.c_str(), value.data() };
	const int argLen[] = {
		-1,
		(int)MYMIN(key.size(), INT_MAX),
		(int)MYMIN(value.size(), INT_MAX),
	};
	const int argFmt[] = { 0, 1, 1 };
	if (getPGVersion() < 90500) {
		execPrepared("set_insert", ARRLEN(args), args, argLen, argFmt);
		execPrepared("set_update", ARRLEN(args), args, argLen, argFmt);
	} else {
		execPrepared("set", ARRLEN(args), args, argLen, argFmt);
	}

	return true;
}

bool ModStorageDatabasePostgreSQL::removeModEntry(const std::string &modname,
		const std::string &key)
{
	verifyDatabase();

	const void *args[] = { modname.c_str(), key.c_str() };
	const int argLen[] = { -1, (int)MYMIN(key.size(), INT_MAX) };
	const int argFmt[] = { 0, 1 };
	PGresult *results = execPrepared("remove", ARRLEN(args), args, argLen, argFmt, false);

	int affected = atoi(PQcmdTuples(results));

	PQclear(results);

	return affected > 0;
}

bool ModStorageDatabasePostgreSQL::removeModEntries(const std::string &modname)
{
	verifyDatabase();

	const void *args[] = { modname.c_str() };
	const int argLen[] = { -1 };
	const int argFmt[] = { 0 };
	PGresult *results = execPrepared("remove_all", ARRLEN(args), args, argLen, argFmt, false);

	int affected = atoi(PQcmdTuples(results));

	PQclear(results);

	return affected > 0;
}

void ModStorageDatabasePostgreSQL::listMods(std::vector<std::string> *res)
{
	verifyDatabase();

	PGresult *results = execPrepared("list", 0, NULL, false);

	int numrows = PQntuples(results);

	for (int row = 0; row < numrows; ++row)
		res->push_back(pg_to_string(results, row, 0));

	PQclear(results);
}


#endif // USE_POSTGRESQL
