// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "config.h"

#if USE_POSTGRESQL

#include <string>
#include <list>

#include "irr_v3d.h"
#include "rollback_interface.h"
#include "database/database-postgresql.h"

class IGameDef;

struct ActionRow;

class RollbackManagerPostgreSQL : private Database_PostgreSQL, public IRollbackManager
{
public:
	RollbackManagerPostgreSQL(const std::string &connect_string, IGameDef *gamedef);
	~RollbackManagerPostgreSQL() override;

	void reportAction(const RollbackAction &action_) override;
	std::string getActor() override;
	bool isActorGuess() override;
	void setActor(const std::string &actor, bool is_guess) override;
	std::string getSuspect(v3s16 p, float nearness_shortcut,
			float min_nearness) override;
	void flush() override;

	std::list<RollbackAction> getNodeActors(v3s16 pos, int range,
			time_t seconds, int limit) override;
	std::list<RollbackAction> getRevertActions(
			const std::string &actor_filter, time_t seconds) override;

private:
	void addAction(const RollbackAction &action);
	static float getSuspectNearness(bool is_guess, v3s16 suspect_p,
			time_t suspect_t, v3s16 action_p, time_t action_t);

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
	IGameDef *gamedef = nullptr;

	std::string current_actor;
	bool current_actor_is_guess = false;

	std::list<RollbackAction> action_todisk_buffer;
	std::list<RollbackAction> action_latest_buffer;
};

#endif // USE_POSTGRESQL