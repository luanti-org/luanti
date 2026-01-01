// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2025 James Dornan <james@catch22.com>

#pragma once

#include "config.h"

#if USE_POSTGRESQL

#include <string>
#include <list>
#include <unordered_map>

#include "irr_v3d.h"
#include "server/rollback.h"
#include "database/database-postgresql.h"

class IGameDef;

struct ActionRow;

class RollbackMgrPostgreSQL : public RollbackMgr, private Database_PostgreSQL
{
public:
	RollbackMgrPostgreSQL(const std::string &connect_string, IGameDef *gamedef);
	~RollbackMgrPostgreSQL() override;

private:
	struct IdNameCaches {
		std::unordered_map<std::string, int> actor_name_to_id;
		std::unordered_map<int, std::string> actor_id_to_name;
		std::unordered_map<std::string, int> node_name_to_id;
		std::unordered_map<int, std::string> node_id_to_name;
	};

	// RollbackMgr hooks
	void flush() override;
	void beginSaveActions() override;
	void endSaveActions() override;
	void rollbackSaveActions() override;
	void persistAction(const RollbackAction &a) override;
	std::list<RollbackAction> getActionsSince(time_t firstTime, const std::string &actor_filter) override;
	std::list<RollbackAction> getActionsSince_range(time_t firstTime, v3s16 p, int range, int limit) override;

	// Database_PostgreSQL hooks
	void createDatabase() override;
	void initStatements() override;

	// Entity id mapping
	int upsertActorId(const std::string &name);
	int upsertNodeId(const std::string &name);

	// Action row mapping + queries
	bool registerRow(const ActionRow &row);
	std::list<ActionRow> actionRowsFromSelect(PGresult *res);
	ActionRow actionRowFromRollbackAction(const RollbackAction &action);
	std::list<RollbackAction> rollbackActionsFromActionRows(
			const std::list<ActionRow> &rows);
	std::list<ActionRow> getRowsSince(time_t firstTime, const std::string &actor);
	std::list<ActionRow> getRowsSince_range(time_t firstTime, v3s16 p, int range, int limit);

	void loadActorCache();
	void loadNodeCache();

private:
	IdNameCaches m_cache;
};

#endif // USE_POSTGRESQL
