// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "server/rollback.h"

#include <ctime>

#include "gamedef.h"
#include "util/numeric.h"

RollbackManager::RollbackManager(IGameDef *gamedef_) :
	gamedef(gamedef_)
{
}

void RollbackManager::reportAction(const RollbackAction &action_)
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

	addActionInternal(action);
}

std::string RollbackManager::getActor()
{
	return current_actor;
}

bool RollbackManager::isActorGuess()
{
	return current_actor_is_guess;
}

void RollbackManager::setActor(const std::string &actor, bool is_guess)
{
	current_actor = actor;
	current_actor_is_guess = is_guess;
}

std::string RollbackManager::getSuspect(v3s16 p, float nearness_shortcut, float min_nearness)
{
	if (!current_actor.empty())
		return current_actor;

	time_t cur_time = time(0);
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

void RollbackManager::flush()
{
	if (action_todisk_buffer.empty())
		return;

	beginSaveActions();
	try {
		for (const auto &a : action_todisk_buffer) {
			if (a.actor.empty())
				continue;
			persistAction(a);
		}
		endSaveActions();
		action_todisk_buffer.clear();
	} catch (...) {
		rollbackSaveActions();
		throw;
	}
}

std::list<RollbackAction> RollbackManager::getNodeActors(
		v3s16 pos, int range, time_t seconds, int limit)
{
	flush();
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;
	return getActionsSince_range(first_time, pos, range, limit);
}

std::list<RollbackAction> RollbackManager::getRevertActions(
		const std::string &actor_filter, time_t seconds)
{
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;
	flush();
	return getActionsSince(first_time, actor_filter);
}

float RollbackManager::getSuspectNearness(bool is_guess, v3s16 suspect_p,
		time_t suspect_t, v3s16 action_p, time_t action_t)
{
	if (action_t < suspect_t)
		return 0;

	int f = 100;
	f -= POINTS_PER_NODE * intToFloat(suspect_p, 1).getDistanceFrom(intToFloat(action_p, 1));
	f -= 1 * (action_t - suspect_t);

	if (is_guess)
		f /= 2;

	if (f < 0)
		f = 0;

	return f;
}

void RollbackManager::addActionInternal(const RollbackAction &action)
{
	action_todisk_buffer.push_back(action);
	action_latest_buffer.push_back(action);

	// Flush to disk sometimes
	if (action_todisk_buffer.size() >= BUFFER_LIMIT)
		flush();

	// Keep only a bounded tail for suspect calculations
	while (action_latest_buffer.size() > BUFFER_LIMIT)
		action_latest_buffer.pop_front();
}


