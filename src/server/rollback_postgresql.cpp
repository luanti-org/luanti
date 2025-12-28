// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2025 James Dornan <james@catch22.com>

#include "config.h"

#if USE_POSTGRESQL

#include "server/rollback_postgresql.h"

#include <cstdlib>
#include <climits>
#include <unordered_map>
#include <utility>

#include "exceptions.h"
#include "gamedef.h"
#include "log.h"
#include "util/numeric.h"
#include "util/string.h"

class ItemStackRow : public ItemStack {
public:
	ItemStackRow &operator=(const ItemStack &other)
	{
		*static_cast<ItemStack *>(this) = other;
		return *this;
	}

	int id = 0;
};

struct ActionRow {
	int          id = 0;
	int          actor = 0;
	time_t       timestamp = 0;
	int          type = 0;
	std::string  location, list;
	int          index = 0, add = 0;
	ItemStackRow stack;
	int          nodeMeta = 0;
	int          x = 0, y = 0, z = 0;
	int          oldNode = 0;
	int          oldParam1 = 0, oldParam2 = 0;
	std::string  oldMeta;
	int          newNode = 0;
	int          newParam1 = 0, newParam2 = 0;
	std::string  newMeta;
	int          guessed = 0;
};

static std::string pg_unescape_bytea_to_string(const char *val)
{
	// PQunescapeBytea expects text output format (matches execPrepared text results)
	if (!val)
		return "";
	size_t out_len = 0;
	unsigned char *out = PQunescapeBytea(reinterpret_cast<const unsigned char *>(val), &out_len);
	if (!out)
		return "";
	std::string ret(reinterpret_cast<const char *>(out), out_len);
	PQfreemem(out);
	return ret;
}

RollbackMgrPostgreSQL::RollbackMgrPostgreSQL(
		const std::string &connect_string, IGameDef *gamedef_) :
	RollbackMgr(gamedef_),
	Database_PostgreSQL(connect_string, "_rollback")
{
	connectToDatabase();
	loadActorCache();
	loadNodeCache();
}

RollbackMgrPostgreSQL::~RollbackMgrPostgreSQL()
{
	flush();
}

void RollbackMgrPostgreSQL::loadActorCache()
{
	PGresult *res = execPrepared("actor_list", 0, nullptr, false, false);
	const int numrows = PQntuples(res);
	m_cache.actor_name_to_id.reserve((size_t)numrows);
	m_cache.actor_id_to_name.reserve((size_t)numrows);
	for (int row = 0; row < numrows; ++row) {
		const int id = pg_to_int(res, row, 0);
		std::string name = pg_to_string(res, row, 1);
		m_cache.actor_name_to_id.emplace(name, id);
		m_cache.actor_id_to_name.emplace(id, std::move(name));
	}
	PQclear(res);
}

void RollbackMgrPostgreSQL::loadNodeCache()
{
	PGresult *res = execPrepared("node_list", 0, nullptr, false, false);
	const int numrows = PQntuples(res);
	m_cache.node_name_to_id.reserve((size_t)numrows);
	m_cache.node_id_to_name.reserve((size_t)numrows);
	for (int row = 0; row < numrows; ++row) {
		const int id = pg_to_int(res, row, 0);
		std::string name = pg_to_string(res, row, 1);
		m_cache.node_name_to_id.emplace(name, id);
		m_cache.node_id_to_name.emplace(id, std::move(name));
	}
	PQclear(res);
}

void RollbackMgrPostgreSQL::createDatabase()
{
	createTableIfNotExists("actor",
		"CREATE TABLE actor ("
			"id SERIAL PRIMARY KEY,"
			"name TEXT NOT NULL UNIQUE"
		");");

	createTableIfNotExists("node",
		"CREATE TABLE node ("
			"id SERIAL PRIMARY KEY,"
			"name TEXT NOT NULL UNIQUE"
		");");

	createTableIfNotExists("action",
		"CREATE TABLE action ("
			"id BIGSERIAL PRIMARY KEY,"
			"actor INT NOT NULL REFERENCES actor(id),"
			"timestamp BIGINT NOT NULL,"
			"type INT NOT NULL,"
			"list TEXT,"
			"index INT,"
			"add INT,"
			"stackNode INT REFERENCES node(id),"
			"stackQuantity INT,"
			"nodeMeta INT,"
			"x INT,"
			"y INT,"
			"z INT,"
			"oldNode INT REFERENCES node(id),"
			"oldParam1 INT,"
			"oldParam2 INT,"
			"oldMeta BYTEA,"
			"newNode INT REFERENCES node(id),"
			"newParam1 INT,"
			"newParam2 INT,"
			"newMeta BYTEA,"
			"guessedActor INT"
		");"

		"CREATE INDEX IF NOT EXISTS actionIndex "
		"ON action(x, y, z, timestamp, actor);"

		"CREATE INDEX IF NOT EXISTS actionTimestampActorIndex "
		"ON action(timestamp, actor);"

		"CREATE INDEX IF NOT EXISTS action_actor_ts "
		"ON action(actor, timestamp DESC, id DESC);"

		"CREATE INDEX IF NOT EXISTS action_ts_brin "
		"ON action USING brin(timestamp);");
}

void RollbackMgrPostgreSQL::initStatements()
{
	prepareStatement("actor_list", "SELECT id, name FROM actor");
	prepareStatement("node_list", "SELECT id, name FROM node");

	prepareStatement("actor_upsert",
		"INSERT INTO actor(name) VALUES($1) "
		"ON CONFLICT (name) DO UPDATE SET name = EXCLUDED.name "
		"RETURNING id");
	prepareStatement("node_upsert",
		"INSERT INTO node(name) VALUES($1) "
		"ON CONFLICT (name) DO UPDATE SET name = EXCLUDED.name "
		"RETURNING id");

	prepareStatement("action_insert",
		"INSERT INTO action ("
			"actor, timestamp, type,"
			"list, index, add, stackNode, stackQuantity, nodeMeta,"
			"x, y, z,"
			"oldNode, oldParam1, oldParam2, oldMeta,"
			"newNode, newParam1, newParam2, newMeta,"
			"guessedActor"
		") VALUES ("
			"$1::int, $2::bigint, $3::int,"
			"$4, $5::int, $6::int, $7::int, $8::int, $9::int,"
			"$10::int, $11::int, $12::int,"
			"$13::int, $14::int, $15::int, $16::bytea,"
			"$17::int, $18::int, $19::int, $20::bytea,"
			"$21::int"
		")");

	prepareStatement("action_select",
		"SELECT "
			"actor, timestamp, type,"
			"list, index, add, stackNode, stackQuantity, nodeMeta,"
			"x, y, z,"
			"oldNode, oldParam1, oldParam2, oldMeta,"
			"newNode, newParam1, newParam2, newMeta,"
			"guessedActor "
		"FROM action "
		"WHERE timestamp >= $1::bigint "
		"ORDER BY timestamp DESC, id DESC");

	prepareStatement("action_select_with_actor",
		"SELECT "
			"actor, timestamp, type,"
			"list, index, add, stackNode, stackQuantity, nodeMeta,"
			"x, y, z,"
			"oldNode, oldParam1, oldParam2, oldMeta,"
			"newNode, newParam1, newParam2, newMeta,"
			"guessedActor "
		"FROM action "
		"WHERE timestamp >= $1::bigint AND actor = $2::int "
		"ORDER BY timestamp DESC, id DESC");

	prepareStatement("action_select_range",
		"SELECT "
			"actor, timestamp, type,"
			"list, index, add, stackNode, stackQuantity, nodeMeta,"
			"x, y, z,"
			"oldNode, oldParam1, oldParam2, oldMeta,"
			"newNode, newParam1, newParam2, newMeta,"
			"guessedActor "
		"FROM action "
		"WHERE timestamp >= $1::bigint "
			"AND x IS NOT NULL AND y IS NOT NULL AND z IS NOT NULL "
			"AND x BETWEEN $2::int AND $3::int "
			"AND y BETWEEN $4::int AND $5::int "
			"AND z BETWEEN $6::int AND $7::int "
		"ORDER BY timestamp DESC, id DESC "
		"LIMIT $8::int");
}

int RollbackMgrPostgreSQL::getActorId(const std::string &name)
{
	auto &cache = m_cache;
	if (auto it = cache.actor_name_to_id.find(name); it != cache.actor_name_to_id.end())
		return it->second;

	verifyDatabase();
	const char *values[] = { name.c_str() };

	PGresult *r = execPrepared("actor_upsert", 1, values, false, false);
	if (PQntuples(r) == 0) {
		PQclear(r);
		throw DatabaseException("PostgreSQL rollback: failed to upsert actor id");
	}
	const int id = pg_to_int(r, 0, 0);
	PQclear(r);
	cache.actor_name_to_id.emplace(name, id);
	cache.actor_id_to_name.emplace(id, name);
	return id;
}

int RollbackMgrPostgreSQL::getNodeId(const std::string &name)
{
	auto &cache = m_cache;
	if (auto it = cache.node_name_to_id.find(name); it != cache.node_name_to_id.end())
		return it->second;

	verifyDatabase();
	const char *values[] = { name.c_str() };

	PGresult *r = execPrepared("node_upsert", 1, values, false, false);
	if (PQntuples(r) == 0) {
		PQclear(r);
		throw DatabaseException("PostgreSQL rollback: failed to upsert node id");
	}
	const int id = pg_to_int(r, 0, 0);
	PQclear(r);
	cache.node_name_to_id.emplace(name, id);
	cache.node_id_to_name.emplace(id, name);
	return id;
}

const char *RollbackMgrPostgreSQL::getActorName(int id)
{
	auto &cache = m_cache;
	if (auto it = cache.actor_id_to_name.find(id); it != cache.actor_id_to_name.end())
		return it->second.c_str();
	return "";
}

const char *RollbackMgrPostgreSQL::getNodeName(int id)
{
	auto &cache = m_cache;
	if (auto it = cache.node_id_to_name.find(id); it != cache.node_id_to_name.end())
		return it->second.c_str();
	return "";
}

bool RollbackMgrPostgreSQL::registerRow(const ActionRow &row)
{
	verifyDatabase();

	std::string actor_s = itos(row.actor);
	std::string ts_s = itos((s64)row.timestamp);
	std::string type_s = itos(row.type);

	std::string index_s = itos(row.index);
	std::string add_s = itos(row.add);
	std::string stackNode_s = itos(row.stack.id);
	std::string stackQty_s = itos(row.stack.count);
	std::string nodeMeta_s = itos(row.nodeMeta);

	std::string x_s = itos(row.x);
	std::string y_s = itos(row.y);
	std::string z_s = itos(row.z);

	std::string oldNode_s = itos(row.oldNode);
	std::string oldP1_s = itos(row.oldParam1);
	std::string oldP2_s = itos(row.oldParam2);

	std::string newNode_s = itos(row.newNode);
	std::string newP1_s = itos(row.newParam1);
	std::string newP2_s = itos(row.newParam2);

	std::string guessed_s = itos(row.guessed);

	const void *args[21] = {nullptr};
	int argLen[21] = {0};
	int argFmt[21] = {0};

	auto set_text = [&](int idx, const std::string &s) {
		args[idx] = s.c_str();
		argLen[idx] = -1;
		argFmt[idx] = 0;
	};
	auto set_bytea = [&](int idx, const std::string &s) {
		args[idx] = s.data();
		argLen[idx] = (int)MYMIN(s.size(), (size_t)INT_MAX);
		argFmt[idx] = 1; // binary param format for BYTEA
	};

	set_text(0, actor_s);
	set_text(1, ts_s);
	set_text(2, type_s);

	bool nodeMeta = false;
	if (row.type == RollbackAction::TYPE_MODIFY_INVENTORY_STACK) {
		const std::string &loc = row.location;
		nodeMeta = (loc.size() >= 9 && loc.compare(0, 9, "nodemeta:") == 0);

		set_text(3, row.list);
		set_text(4, index_s);
		set_text(5, add_s);
		set_text(6, stackNode_s);
		set_text(7, stackQty_s);
		set_text(8, nodeMeta_s);

		if (nodeMeta) {
			set_text(9, x_s);
			set_text(10, y_s);
			set_text(11, z_s);
		}
	}

	if (row.type == RollbackAction::TYPE_SET_NODE) {
		set_text(9, x_s);
		set_text(10, y_s);
		set_text(11, z_s);

		set_text(12, oldNode_s);
		set_text(13, oldP1_s);
		set_text(14, oldP2_s);
		set_bytea(15, row.oldMeta);

		set_text(16, newNode_s);
		set_text(17, newP1_s);
		set_text(18, newP2_s);
		set_bytea(19, row.newMeta);

		set_text(20, guessed_s);
	}

	execPrepared("action_insert", 21, args, argLen, argFmt);
	return true;
}

std::list<ActionRow> RollbackMgrPostgreSQL::actionRowsFromSelect(PGresult *res)
{
	std::list<ActionRow> rows;
	const int numrows = PQntuples(res);

	for (int r = 0; r < numrows; ++r) {
		ActionRow row;
		row.actor = pg_to_int(res, r, 0);
		row.timestamp = (time_t)atoll(PQgetvalue(res, r, 1));
		row.type = pg_to_int(res, r, 2);
		row.nodeMeta = 0;

		if (row.type == RollbackAction::TYPE_MODIFY_INVENTORY_STACK) {
			row.list = pg_to_string(res, r, 3);
			row.index = pg_to_int(res, r, 4);
			row.add = pg_to_int(res, r, 5);
			row.stack.id = pg_to_int(res, r, 6);
			row.stack.count = pg_to_int(res, r, 7);
			row.nodeMeta = pg_to_int(res, r, 8);
		}

		if (row.type == RollbackAction::TYPE_SET_NODE || row.nodeMeta) {
			row.x = pg_to_int(res, r, 9);
			row.y = pg_to_int(res, r, 10);
			row.z = pg_to_int(res, r, 11);
		}

		if (row.type == RollbackAction::TYPE_SET_NODE) {
			row.oldNode = pg_to_int(res, r, 12);
			row.oldParam1 = pg_to_int(res, r, 13);
			row.oldParam2 = pg_to_int(res, r, 14);
			row.oldMeta = pg_unescape_bytea_to_string(PQgetvalue(res, r, 15));
			row.newNode = pg_to_int(res, r, 16);
			row.newParam1 = pg_to_int(res, r, 17);
			row.newParam2 = pg_to_int(res, r, 18);
			row.newMeta = pg_unescape_bytea_to_string(PQgetvalue(res, r, 19));
			row.guessed = pg_to_int(res, r, 20);
		}

		if (row.nodeMeta) {
			row.location = "nodemeta:";
			row.location += itos(row.x);
			row.location += ',';
			row.location += itos(row.y);
			row.location += ',';
			row.location += itos(row.z);
		} else {
			row.location = getActorName(row.actor);
		}

		rows.push_back(std::move(row));
	}

	PQclear(res);
	return rows;
}

ActionRow RollbackMgrPostgreSQL::actionRowFromRollbackAction(const RollbackAction &action)
{
	ActionRow row;
	row.actor = getActorId(action.actor);
	row.timestamp = action.unix_time;
	row.type = action.type;

	if (row.type == RollbackAction::TYPE_MODIFY_INVENTORY_STACK) {
		row.location = action.inventory_location;
		row.list = action.inventory_list;
		row.index = action.inventory_index;
		row.add = action.inventory_add;
		row.stack = action.inventory_stack;
		row.stack.id = getNodeId(row.stack.name);

		const std::string &loc = row.location;
		const bool is_node_meta = (loc.size() >= 9 && loc.compare(0, 9, "nodemeta:") == 0);
		row.nodeMeta = is_node_meta ? 1 : 0;

		if (is_node_meta) {
			parseNodemetaLocation(loc, row.x, row.y, row.z);
		}
	} else {
		row.x = action.p.X;
		row.y = action.p.Y;
		row.z = action.p.Z;
		row.oldNode = getNodeId(action.n_old.name);
		row.oldParam1 = action.n_old.param1;
		row.oldParam2 = action.n_old.param2;
		row.oldMeta = action.n_old.meta;
		row.newNode = getNodeId(action.n_new.name);
		row.newParam1 = action.n_new.param1;
		row.newParam2 = action.n_new.param2;
		row.newMeta = action.n_new.meta;
		row.guessed = action.actor_is_guess ? 1 : 0;
	}

	return row;
}

std::list<RollbackAction> RollbackMgrPostgreSQL::rollbackActionsFromActionRows(
		const std::list<ActionRow> &rows)
{
	std::list<RollbackAction> actions;

	for (const ActionRow &row : rows) {
		RollbackAction action;
		action.actor = (row.actor) ? getActorName(row.actor) : "";
		action.unix_time = row.timestamp;
		action.type = static_cast<RollbackAction::Type>(row.type);

		switch (action.type) {
		case RollbackAction::TYPE_MODIFY_INVENTORY_STACK:
			action.inventory_location = row.location;
			action.inventory_list = row.list;
			action.inventory_index = row.index;
			action.inventory_add = row.add;
			action.inventory_stack = row.stack;
			if (action.inventory_stack.name.empty())
				action.inventory_stack.name = getNodeName(row.stack.id);
			break;

		case RollbackAction::TYPE_SET_NODE:
			action.p = v3s16(row.x, row.y, row.z);
			action.n_old.name = getNodeName(row.oldNode);
			action.n_old.param1 = row.oldParam1;
			action.n_old.param2 = row.oldParam2;
			action.n_old.meta = row.oldMeta;
			action.n_new.name = getNodeName(row.newNode);
			action.n_new.param1 = row.newParam1;
			action.n_new.param2 = row.newParam2;
			action.n_new.meta = row.newMeta;
			action.actor_is_guess = row.guessed != 0;
			break;

		default:
			throw BaseException("PostgreSQL rollback: unknown action type");
		}

		actions.push_back(std::move(action));
	}

	return actions;
}

std::list<ActionRow> RollbackMgrPostgreSQL::getRowsSince(time_t firstTime, const std::string &actor)
{
	verifyDatabase();

	if (actor.empty()) {
		std::string ts_s = itos((s64)firstTime);
		const char *values[] = { ts_s.c_str() };
		return actionRowsFromSelect(execPrepared("action_select", 1, values, false, false));
	}

	std::string ts_s = itos((s64)firstTime);
	std::string actor_id_s = itos(getActorId(actor));
	const char *values[] = { ts_s.c_str(), actor_id_s.c_str() };
	return actionRowsFromSelect(execPrepared("action_select_with_actor", 2, values, false, false));
}

std::list<ActionRow> RollbackMgrPostgreSQL::getRowsSince_range(
		time_t start_time, v3s16 p, int range, int limit)
{
	verifyDatabase();

	std::string ts_s = itos((s64)start_time);
	std::string x1 = itos((int)(p.X - range));
	std::string x2 = itos((int)(p.X + range));
	std::string y1 = itos((int)(p.Y - range));
	std::string y2 = itos((int)(p.Y + range));
	std::string z1 = itos((int)(p.Z - range));
	std::string z2 = itos((int)(p.Z + range));
	std::string lim = itos(limit);

	const char *values[] = {
		ts_s.c_str(),
		x1.c_str(), x2.c_str(),
		y1.c_str(), y2.c_str(),
		z1.c_str(), z2.c_str(),
		lim.c_str()
	};

	return actionRowsFromSelect(execPrepared("action_select_range", 8, values, false, false));
}

void RollbackMgrPostgreSQL::beginSaveActions()
{
	beginSave();
}

void RollbackMgrPostgreSQL::endSaveActions()
{
	endSave();
}

void RollbackMgrPostgreSQL::rollbackSaveActions()
{
	rollback();
}

void RollbackMgrPostgreSQL::flush()
{
	if (!initialized())
		return;

	if (action_todisk_buffer.empty())
		return;

	beginSaveActions();
	try {
		flushBufferContents();
		endSaveActions();
	} catch (...) {
		rollbackSaveActions();
		throw;
	}
}

void RollbackMgrPostgreSQL::persistAction(const RollbackAction &a)
{
	registerRow(actionRowFromRollbackAction(a));
}

std::list<RollbackAction> RollbackMgrPostgreSQL::getActionsSince(
		time_t firstTime, const std::string &actor_filter)
{
	return rollbackActionsFromActionRows(getRowsSince(firstTime, actor_filter));
}

std::list<RollbackAction> RollbackMgrPostgreSQL::getActionsSince_range(
		time_t firstTime, v3s16 p, int range, int limit)
{
	return rollbackActionsFromActionRows(getRowsSince_range(firstTime, p, range, limit));
}

#endif // USE_POSTGRESQL
