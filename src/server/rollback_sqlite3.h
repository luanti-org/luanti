// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <list>
#include <vector>
#include <deque>

#include "sqlite3.h"
#include "server/rollback.h"

class IGameDef;

struct ActionRow;
struct Entity;

class RollbackMgrSQLite3 final : public RollbackMgr
{
public:
	RollbackMgrSQLite3(const std::string &world_path, IGameDef *gamedef);
	~RollbackMgrSQLite3();

private:
	// RollbackMgr hooks
	void flush() override;
	void beginSaveActions() override;
	void endSaveActions() override;
	void rollbackSaveActions() override;
	void persistAction(const RollbackAction &a) override;
	std::list<RollbackAction> getActionsSince(time_t start_time, const std::string &actor) override;
	std::list<RollbackAction> getActionsSince_range(time_t start_time, v3s16 p, int range, int limit) override;

	void registerNewActor(int id, const std::string &name);
	void registerNewNode(int id, const std::string &name);
	int getActorId(const std::string &name);
	int getNodeId(const std::string &name);
	const char *getActorName(int id);
	const char *getNodeName(int id);
	bool createTables();
	bool initDatabase();
	bool registerRow(const ActionRow &row);
	std::list<ActionRow> actionRowsFromSelect(sqlite3_stmt *stmt);
	ActionRow actionRowFromRollbackAction(const RollbackAction &action);
	std::list<RollbackAction> rollbackActionsFromActionRows(const std::list<ActionRow> &rows);
	std::list<ActionRow> getRowsSince(time_t firstTime, const std::string &actor);
	std::list<ActionRow> getRowsSince_range(time_t firstTime, v3s16 p, int range, int limit);

	std::string database_path;
	sqlite3 *db = nullptr;
	sqlite3_stmt *stmt_insert = nullptr;
	sqlite3_stmt *stmt_replace = nullptr;
	sqlite3_stmt *stmt_select = nullptr;
	sqlite3_stmt *stmt_select_range = nullptr;
	sqlite3_stmt *stmt_select_withActor = nullptr;
	sqlite3_stmt *stmt_knownActor_select = nullptr;
	sqlite3_stmt *stmt_knownActor_insert = nullptr;
	sqlite3_stmt *stmt_knownNode_select = nullptr;
	sqlite3_stmt *stmt_knownNode_insert = nullptr;

	std::vector<Entity> knownActors;
	std::vector<Entity> knownNodes;
};


