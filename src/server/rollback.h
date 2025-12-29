// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Shared logic for rollback backends (SQLite/PostgreSQL).
// This exists to avoid copy-pasting the same buffering/suspect logic into each backend.

#pragma once

#include <deque>
#include <list>
#include <string>
#include <vector>

#include "rollback_interface.h"

class IGameDef;

class RollbackMgr : public IRollbackManager
{
public:
	~RollbackMgr() override = default;

	// IRollbackManager
	void reportAction(const RollbackAction &action_) override;
	std::string getActor() override;
	bool isActorGuess() override;
	void setActor(const std::string &actor, bool is_guess) override;
	std::string getSuspect(v3s16 p, float nearness_shortcut, float min_nearness) override;
	virtual void flush() = 0;
	std::list<RollbackAction> getNodeActors(v3s16 pos, int range, time_t seconds, int limit) override;
	std::list<RollbackAction> getRevertActions(const std::string &actor_filter, time_t seconds) override;

protected:
	explicit RollbackMgr(IGameDef *gamedef_);
	static float getSuspectNearness(bool is_guess, v3s16 suspect_p,
			time_t suspect_t, v3s16 action_p, time_t action_t);

	void addActionInternal(const RollbackAction &action);

	// Helper for derived classes to flush buffer contents
	// Derived classes handle transactions/persistence as needed
	void flushBufferContents();

public:
	// Helper to parse nodemeta location format: "nodemeta:x,y,z"
	// Throws std::invalid_argument on invalid format
	static void parseNodemetaLocation(const std::string &loc, int &x, int &y, int &z);

protected:

	// Backend-specific hooks
	virtual void beginSaveActions() = 0;
	virtual void endSaveActions() = 0;
	virtual void rollbackSaveActions() = 0;
	virtual void persistAction(const RollbackAction &a) = 0;
	virtual std::list<RollbackAction> getActionsSince(time_t firstTime, const std::string &actor_filter) = 0;
	virtual std::list<RollbackAction> getActionsSince_range(time_t firstTime, v3s16 p, int range, int limit) = 0;

protected:
	static constexpr float POINTS_PER_NODE = 16.0f;
	static constexpr size_t BUFFER_LIMIT = 500;

	IGameDef *gamedef = nullptr;

	std::string current_actor;
	bool current_actor_is_guess = false;

	std::vector<RollbackAction> action_todisk_buffer;
	std::deque<RollbackAction> action_latest_buffer;
};


