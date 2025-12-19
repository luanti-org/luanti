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

// Base class providing the common parts of rollback handling (actor tracking,
// suspect selection, buffering and generic query wrappers). DB backends implement
// only the persistence and query primitives.
class RollbackManager : public IRollbackManager
{
public:
	explicit RollbackManager(IGameDef *gamedef_);
	~RollbackManager() override = default;

	// IRollbackManager
	void reportAction(const RollbackAction &action_) override;
	std::string getActor() override;
	bool isActorGuess() override;
	void setActor(const std::string &actor, bool is_guess) override;
	std::string getSuspect(v3s16 p, float nearness_shortcut, float min_nearness) override;
	void flush() override;
	std::list<RollbackAction> getNodeActors(v3s16 pos, int range, time_t seconds, int limit) override;
	std::list<RollbackAction> getRevertActions(const std::string &actor_filter, time_t seconds) override;

protected:
	static float getSuspectNearness(bool is_guess, v3s16 suspect_p,
			time_t suspect_t, v3s16 action_p, time_t action_t);

	void addActionInternal(const RollbackAction &action);

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


