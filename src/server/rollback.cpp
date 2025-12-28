// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "server/rollback.h"

#include <ctime>

#include "gamedef.h"
#include "util/numeric.h"

RollbackMgr::RollbackMgr(IGameDef *gamedef_) :
	gamedef(gamedef_)
{
}

void RollbackMgr::reportAction(const RollbackAction &action_)
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

std::string RollbackMgr::getActor()
{
	return current_actor;
}

bool RollbackMgr::isActorGuess()
{
	return current_actor_is_guess;
}

void RollbackMgr::setActor(const std::string &actor, bool is_guess)
{
	current_actor = actor;
	current_actor_is_guess = is_guess;
}

std::string RollbackMgr::getSuspect(v3s16 p, float nearness_shortcut, float min_nearness)
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

void RollbackMgr::flushBufferContents()
{
	if (action_todisk_buffer.empty())
		return;

	for (const auto &a : action_todisk_buffer) {
		if (a.actor.empty())
			continue;
		persistAction(a);
	}
	action_todisk_buffer.clear();
}

std::list<RollbackAction> RollbackMgr::getNodeActors(
		v3s16 pos, int range, time_t seconds, int limit)
{
	flush();
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;
	return getActionsSince_range(first_time, pos, range, limit);
}

std::list<RollbackAction> RollbackMgr::getRevertActions(
		const std::string &actor_filter, time_t seconds)
{
	time_t cur_time = time(0);
	time_t first_time = cur_time - seconds;
	flush();
	return getActionsSince(first_time, actor_filter);
}

// Get nearness factor for subject's action for this action
// Return value: 0 = impossible, >0 = factor
float RollbackMgr::getSuspectNearness(bool is_guess, v3s16 suspect_p,
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

void RollbackMgr::addActionInternal(const RollbackAction &action)
{
	action_todisk_buffer.push_back(action);
	action_latest_buffer.push_back(action);

	// Keep only a bounded tail for suspect calculations
	while (action_latest_buffer.size() > BUFFER_LIMIT)
		action_latest_buffer.pop_front();

	// Flush to disk sometimes
	if (action_todisk_buffer.size() >= BUFFER_LIMIT)
		flush();
}

bool RollbackMgr::parseNodemetaLocation(const std::string &loc, int &x, int &y, int &z)
{
	// Format: "nodemeta:x,y,z"
	if (loc.size() < 9 || loc.compare(0, 9, "nodemeta:") != 0)
		return false;

	std::string::size_type p1 = loc.find(':') + 1;
	std::string::size_type p2 = loc.find(',');
	if (p2 == std::string::npos)
		return false;

	std::string x_str = loc.substr(p1, p2 - p1);
	p1 = p2 + 1;
	p2 = loc.find(',', p1);
	if (p2 == std::string::npos)
		return false;

	std::string y_str = loc.substr(p1, p2 - p1);
	std::string z_str = loc.substr(p2 + 1);

	x = atoi(x_str.c_str());
	y = atoi(y_str.c_str());
	z = atoi(z_str.c_str());

	return true;
}


