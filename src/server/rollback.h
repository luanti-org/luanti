// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include "irr_v3d.h"
#include "rollback_interface.h"
#include <list>
#include <vector>
#include "sqlite3.h"

class IGameDef;

struct ActionRow;
struct Entity;

class RollbackManager: public IRollbackManager
{
public:
	RollbackManager(const std::string & world_path, IGameDef * gamedef);
	~RollbackManager();

	void reportAction(const RollbackAction & action_);
	std::string getActor();
	bool isActorGuess();
	void setActor(const std::string & actor, bool is_guess);
	std::string getSuspect(v3s16 p, float nearness_shortcut,
			float min_nearness);
	void flush();

	void addAction(const RollbackAction & action);
	std::list<RollbackAction> getNodeActors(v3s16 pos, int range,
			time_t seconds, int limit);
	std::list<RollbackAction> getRevertActions(
			const std::string & actor_filter, time_t seconds);

private:
	void registerNewActor(const int id, const std::string & name);
	void registerNewNode(const int id, const std::string & name);
	int getActorId(const std::string & name);
	int getNodeId(const std::string & name);
	const char * getActorName(const int id);
	const char * getNodeName(const int id);
	bool createTables();
	bool initDatabase();
	bool registerRow(const ActionRow & row);
	const std::list<ActionRow> actionRowsFromSelect(sqlite3_stmt * stmt);
	ActionRow actionRowFromRollbackAction(const RollbackAction & action);
	const std::list<RollbackAction> rollbackActionsFromActionRows(
			const std::list<ActionRow> & rows);
	const std::list<ActionRow> getRowsSince(time_t firstTime,
			const std::string & actor);
	const std::list<ActionRow> getRowsSince_range(time_t firstTime, v3s16 p,
			int range, int limit);
	const std::list<RollbackAction> getActionsSince_range(time_t firstTime, v3s16 p,
			int range, int limit);
	const std::list<RollbackAction> getActionsSince(time_t firstTime,
			const std::string & actor = "");
	static float getSuspectNearness(bool is_guess, v3s16 suspect_p,
		time_t suspect_t, v3s16 action_p, time_t action_t);


	IGameDef *gamedef = nullptr;

	std::string current_actor;
	bool current_actor_is_guess = false;

	std::list<RollbackAction> action_todisk_buffer;
	std::list<RollbackAction> action_latest_buffer;

	std::string database_path;
	sqlite3 * db;
	sqlite3_stmt * stmt_insert;
	sqlite3_stmt * stmt_replace;
	sqlite3_stmt * stmt_select;
	sqlite3_stmt * stmt_select_range;
	sqlite3_stmt * stmt_select_withActor;
	sqlite3_stmt * stmt_knownActor_select;
	sqlite3_stmt * stmt_knownActor_insert;
	sqlite3_stmt * stmt_knownNode_select;
	sqlite3_stmt * stmt_knownNode_insert;

	std::vector<Entity> knownActors;
	std::vector<Entity> knownNodes;
};
