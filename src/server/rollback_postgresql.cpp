// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

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

#define POINTS_PER_NODE (16.0)

/*
 * Postgres-focused improvements vs the straight SQLite port:
 * - O(1) actor/node lookups via unordered_map caches (avoid O(n) vector scans).
 * - More appropriate indexes:
 *     * BRIN on timestamp for fast time-window scans on append-only tables.
 *     * (actor, timestamp DESC, id DESC) for actor-filter queries.
 *   (keeps existing indexes for compatibility; you can later drop redundant ones after profiling)
 * - Bytea handling kept compatible with text-result mode. (If Database_PostgreSQL later adds a
 *   binary-result mode, you can switch to PQgetlength/PQgetvalue without PQunescapeBytea.)
 */

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
	// Note: PQunescapeBytea expects *text* output format of bytea.
	// This matches the current use of execPrepared(..., false, false) which returns text results.
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

/*
 * These caches are implemented here as translation-unit statics keyed by `this`,
 * so we can dramatically speed up lookups without requiring immediate header changes.
 *
 * If you prefer, move these maps into RollbackManagerPostgreSQL class members later.
 */
struct IdNameCaches {
	std::unordered_map<std::string, int> actor_name_to_id;
	std::unordered_map<int, std::string> actor_id_to_name;
	std::unordered_map<std::string, int> node_name_to_id;
	std::unordered_map<int, std::string> node_id_to_name;
};

static std::unordered_map<const RollbackManagerPostgreSQL *, IdNameCaches> g_caches;

RollbackManagerPostgreSQL::RollbackManagerPostgreSQL(
		const std::string &connect_string, IGameDef *gamedef_) :
	Database_PostgreSQL(connect_string, "_rollback"),
	gamedef(gamedef_)
{
	connectToDatabase();

	// Preload known actors/nodes into O(1) caches.
	{
		PGresult *res = execPrepared("actor_list", 0, nullptr, false, false);
		const int numrows = PQntuples(res);
		auto &cache = g_caches[this];
		cache.actor_name_to_id.reserve((size_t)numrows);
		cache.actor_id_to_name.reserve((size_t)numrows);
		for (int row = 0; row < numrows; ++row) {
			const int id = pg_to_int(res, row, 0);
			std::string name = pg_to_string(res, row, 1);
			cache.actor_name_to_id.emplace(name, id);
			cache.actor_id_to_name.emplace(id, std::move(name));
		}
		PQclear(res);
	}
	{
		PGresult *res = execPrepared("node_list", 0, nullptr, false, false);
		const int numrows = PQntuples(res);
		auto &cache = g_caches[this];
		cache.node_name_to_id.reserve((size_t)numrows);
		cache.node_id_to_name.reserve((size_t)numrows);
		for (int row = 0; row < numrows; ++row) {
			const int id = pg_to_int(res, row, 0);
			std::string name = pg_to_string(res, row, 1);
			cache.node_name_to_id.emplace(name, id);
			cache.node_id_to_name.emplace(id, std::move(name));
		}
		PQclear(res);
	}
}

RollbackManagerPostgreSQL::~RollbackManagerPostgreSQL()
{
	flush();
	g_caches.erase(this);
}

void RollbackManagerPostgreSQL::createDatabase()
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

	// Also creates indexes in one go (safe to re-run with IF NOT EXISTS).
	// Notes:
	// - action_ts_brin: tiny and fast for time-window queries on append-only tables.
	// - action_actor_ts: optimized for "since time AND actor = ?" with ORDER BY timestamp DESC.
	// - existing indexes kept for compatibility / conservative rollout.
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

		// BRIN is available in PostgreSQL 9.5+; harmless to try if your server supports it.
		// If you need strict compatibility with older versions, gate this behind getPGVersion().
		"CREATE INDEX IF NOT EXISTS action_ts_brin "
		"ON action USING brin(timestamp);");
}

void RollbackManagerPostgreSQL::initStatements()
{
	prepareStatement("actor_select", "SELECT id FROM actor WHERE name = $1");
	prepareStatement("node_select", "SELECT id FROM node WHERE name = $1");
	prepareStatement("actor_list", "SELECT id, name FROM actor");
	prepareStatement("node_list", "SELECT id, name FROM node");

	if (getPGVersion() >= 90500) {
		// Use a single statement for "get or create". This avoids select+insert races.
		prepareStatement("actor_upsert",
			"INSERT INTO actor(name) VALUES($1) "
			"ON CONFLICT (name) DO UPDATE SET name = EXCLUDED.name "
			"RETURNING id");
		prepareStatement("node_upsert",
			"INSERT INTO node(name) VALUES($1) "
			"ON CONFLICT (name) DO UPDATE SET name = EXCLUDED.name "
			"RETURNING id");
	} else {
		prepareStatement("actor_insert", "INSERT INTO actor(name) VALUES($1) RETURNING id");
		prepareStatement("node_insert", "INSERT INTO node(name) VALUES($1) RETURNING id");
	}

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

int RollbackManagerPostgreSQL::getActorId(const std::string &name)
{
	auto &cache = g_caches[this];
	if (auto it = cache.actor_name_to_id.find(name); it != cache.actor_name_to_id.end())
		return it->second;

	verifyDatabase();
	const char *values[] = { name.c_str() };

	// If server supports UPSERT, prefer it (handles races cleanly).
	if (getPGVersion() >= 90500) {
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

	// Older PG: select-then-insert (race possible but acceptable for legacy).
	{
		PGresult *res = execPrepared("actor_select", 1, values, false, false);
		if (PQntuples(res) > 0) {
			const int id = pg_to_int(res, 0, 0);
			PQclear(res);
			cache.actor_name_to_id.emplace(name, id);
			cache.actor_id_to_name.emplace(id, name);
			return id;
		}
		PQclear(res);
	}

	PGresult *r2 = execPrepared("actor_insert", 1, values, false, false);
	if (PQntuples(r2) == 0) {
		PQclear(r2);
		throw DatabaseException("PostgreSQL rollback: failed to insert actor id");
	}
	const int id = pg_to_int(r2, 0, 0);
	PQclear(r2);
	cache.actor_name_to_id.emplace(name, id);
	cache.actor_id_to_name.emplace(id, name);
	return id;
}

int RollbackManagerPostgreSQL::getNodeId(const std::string &name)
{
	auto &cache = g_caches[this];
	if (auto it = cache.node_name_to_id.find(name); it != cache.node_name_to_id.end())
		return it->second;

	verifyDatabase();
	const char *values[] = { name.c_str() };

	if (getPGVersion() >= 90500) {
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

	{
		PGresult *res = execPrepared("node_select", 1, values, false, false);
		if (PQntuples(res) > 0) {
			const int id = pg_to_int(res, 0, 0);
			PQclear(res);
			cache.node_name_to_id.emplace(name, id);
			cache.node_id_to_name.emplace(id, name);
			return id;
		}
		PQclear(res);
	}

	PGresult *r2 = execPrepared("node_insert", 1, values, false, false);
	if (PQntuples(r2) == 0) {
		PQclear(r2);
		throw DatabaseException("PostgreSQL rollback: failed to insert node id");
	}
	const int id = pg_to_int(r2, 0, 0);
	PQclear(r2);
	cache.node_name_to_id.emplace(name, id);
	cache.node_id_to_name.emplace(id, name);
	return id;
}

const char *RollbackManagerPostgreSQL::getActorName(int id)
{
	auto &cache = g_caches[this];
	if (auto it = cache.actor_id_to_name.find(id); it != cache.actor_id_to_name.end())
		return it->second.c_str();
	return "";
}

const char *RollbackManagerPostgreSQL::getNodeName(int id)
{
	auto &cache = g_caches[this];
	if (auto it = cache.node_id_to_name.find(id); it != cache.node_id_to_name.end())
		return it->second.c_str();
	return "";
}

bool RollbackManagerPostgreSQL::registerRow(const ActionRow &row)
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

	const void *args[21] = {};
	int argLen[21] = {};
	int argFmt[21] = {};

	auto set_text = [&](int idx, const std::string &s) {
		args[idx] = s.c_str();
		argLen[idx] = -1;
		argFmt[idx] = 0;
	};
	auto set_null = [&](int idx) {
		args[idx] = nullptr;
		argLen[idx] = 0;
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
		} else {
			set_null(9);
			set_null(10);
			set_null(11);
		}
	} else {
		set_null(3);
		set_null(4);
		set_null(5);
		set_null(6);
		set_null(7);
		set_null(8);
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
	} else {
		if (!nodeMeta) {
			set_null(9);
			set_null(10);
			set_null(11);
		}
		set_null(12);
		set_null(13);
		set_null(14);
		set_null(15);
		set_null(16);
		set_null(17);
		set_null(18);
		set_null(19);
		set_null(20);
	}

	execPrepared("action_insert", 21, args, argLen, argFmt);
	return true;
}

std::list<ActionRow> RollbackManagerPostgreSQL::actionRowsFromSelect(PGresult *res)
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

ActionRow RollbackManagerPostgreSQL::actionRowFromRollbackAction(const RollbackAction &action)
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

		// Keep parity with SQLite rollback: only NODEMETA inventories are fully represented
		const std::string &loc = row.location;
		const bool is_node_meta = (loc.size() >= 9 && loc.compare(0, 9, "nodemeta:") == 0);
		row.nodeMeta = is_node_meta ? 1 : 0;

		if (is_node_meta) {
			// loc format: "nodemeta:x,y,z"
			std::string::size_type p1 = loc.find(':') + 1;
			std::string::size_type p2 = loc.find(',');
			std::string x = loc.substr(p1, p2 - p1);
			p1 = p2 + 1;
			p2 = loc.find(',', p1);
			std::string y = loc.substr(p1, p2 - p1);
			std::string z = loc.substr(p2 + 1);
			row.x = atoi(x.c_str());
			row.y = atoi(y.c_str());
			row.z = atoi(z.c_str());
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

std::list<RollbackAction> RollbackManagerPostgreSQL::rollbackActionsFromActionRows(
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

std::list<ActionRow> RollbackManagerPostgreSQL::getRowsSince(time_t firstTime, const std::string &actor)
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

std::list<ActionRow> RollbackManagerPostgreSQL::getRowsSince_range(
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

float RollbackManagerPostgreSQL::getSuspectNearness(bool is_guess, v3s16 suspect_p,
		time_t suspect_t, v3s16 action_p, time_t action_t)
{
	if (action_t < suspect_t)
		return 0;

	int f = 100;
	f -= POINTS_PER_NODE * intToFloat(suspect_p, 1).getDistanceFrom(intToFloat(action_p, 1));
	f -= 1 * (action_t - suspect_t);

	if (is_guess)
		f *= 0.5;

	if (f < 0)
		f = 0;

	return f;
}

void RollbackManagerPostgreSQL::reportAction(const RollbackAction &action_)
{
	if (!action_.isImportant(gamedef))
		return;

	RollbackAction action = action_;
	action.unix_time = time(0);

	action.actor = current_actor;
	action.actor_is_guess = current_actor_is_guess;

	if (action.actor.empty()) {
		v3s16 p;
		if (!action.getPosition(&p))
			return;

		action.actor = getSuspect(p, 83, 1);
		if (action.actor.empty())
			return;

		action.actor_is_guess = true;
	}

	addAction(action);
}

std::string RollbackManagerPostgreSQL::getActor()
{
	return current_actor;
}

bool RollbackManagerPostgreSQL::isActorGuess()
{
	return current_actor_is_guess;
}

void RollbackManagerPostgreSQL::setActor(const std::string &actor, bool is_guess)
{
	current_actor = actor;
	current_actor_is_guess = is_guess;
}

std::string RollbackManagerPostgreSQL::getSuspect(v3s16 p, float nearness_shortcut,
		float min_nearness)
{
	if (!current_actor.empty())
		return current_actor;

	int cur_time = time(0);
	time_t first_time = cur_time - (100 - min_nearness);
	RollbackAction likely_suspect;
	float likely_suspect_nearness = 0;

	for (auto i = action_latest_buffer.rbegin(); i != action_latest_buffer.rend(); ++i) {
		if (i->unix_time < first_time)
			break;
		if (i->actor.empty())
			continue;

		v3s16 suspect_p;
		if (!i->getPosition(&suspect_p))
			continue;

		float f = getSuspectNearness(i->actor_is_guess, suspect_p, i->unix_time, p, cur_time);
		if (f >= min_nearness && f > likely_suspect_nearness) {
			likely_suspect_nearness = f;
			likely_suspect = *i;
			if (likely_suspect_nearness >= nearness_shortcut)
				break;
		}
	}

	if (likely_suspect_nearness == 0)
		return "";

	return likely_suspect.actor;
}

void RollbackManagerPostgreSQL::flush()
{
	if (action_todisk_buffer.empty())
		return;

	beginSave();
	try {
		for (auto &a : action_todisk_buffer) {
			if (a.actor.empty())
				continue;
			registerRow(actionRowFromRollbackAction(a));
		}
		endSave();
		action_todisk_buffer.clear();
	} catch (...) {
		rollback();
		throw;
	}
}

void RollbackManagerPostgreSQL::addAction(const RollbackAction &action)
{
	action_todisk_buffer.push_back(action);
	action_latest_buffer.push_back(action);

	// 500 is fine; if you later add COPY-based flushing, you can increase this dramatically.
	if (action_todisk_buffer.size() >= 500)
		flush();
}

std::list<RollbackAction> RollbackManagerPostgreSQL::getNodeActors(
		v3s16 pos, int range, time_t seconds, int limit)
{
	flush();
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;
	return rollbackActionsFromActionRows(getRowsSince_range(first_time, pos, range, limit));
}

std::list<RollbackAction> RollbackManagerPostgreSQL::getRevertActions(
		const std::string &actor_filter, time_t seconds)
{
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;
	flush();
	return rollbackActionsFromActionRows(getRowsSince(first_time, actor_filter));
}

#endif // USE_POSTGRESQL