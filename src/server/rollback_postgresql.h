// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

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

class RollbackManagerPostgreSQL : public RollbackManager, private Database_PostgreSQL
{
public:
	RollbackManagerPostgreSQL(const std::string &connect_string, IGameDef *gamedef);
	~RollbackManagerPostgreSQL() override;

private:
	struct IdNameCaches {
		std::unordered_map<std::string, int> actor_name_to_id;
		std::unordered_map<int, std::string> actor_id_to_name;
		std::unordered_map<std::string, int> node_name_to_id;
		std::unordered_map<int, std::string> node_id_to_name;
	};

	// RollbackManager hooks
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
	int getActorId(const std::string &name);
	int getNodeId(const std::string &name);
	const char *getActorName(int id);
	const char *getNodeName(int id);

	// Action row mapping + queries
	bool registerRow(const ActionRow &row);
	std::list<ActionRow> actionRowsFromSelect(PGresult *res);
	ActionRow actionRowFromRollbackAction(const RollbackAction &action);
	std::list<RollbackAction> rollbackActionsFromActionRows(
			const std::list<ActionRow> &rows);
	std::list<ActionRow> getRowsSince(time_t firstTime, const std::string &actor);
	std::list<ActionRow> getRowsSince_range(time_t firstTime, v3s16 p, int range, int limit);

private:
	IdNameCaches m_cache;
};

#endif // USE_POSTGRESQL