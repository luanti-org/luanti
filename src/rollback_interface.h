// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irr_v3d.h"
#include <string>
#include <iostream>
#include <list>
#include "exceptions.h"
#include "inventory.h"

class Map;
class IGameDef;
class InventoryManager;

struct RollbackNode
{
	std::string name;
	int param1 = 0;
	int param2 = 0;
	std::string meta;

	bool operator == (const RollbackNode &other)
	{
		return (name == other.name && param1 == other.param1 &&
				param2 == other.param2 && meta == other.meta);
	}
	bool operator != (const RollbackNode &other) { return !(*this == other); }

	RollbackNode() = default;

	RollbackNode(Map *map, v3s16 p, IGameDef *gamedef);
};


struct RollbackAction
{
	enum Type{
		TYPE_NOTHING,
		TYPE_SET_NODE,
		TYPE_MODIFY_INVENTORY_STACK,
	} type = TYPE_NOTHING;

	time_t unix_time = 0;
	std::string actor;
	bool actor_is_guess = false;

	v3s16 p;
	RollbackNode n_old;
	RollbackNode n_new;

	std::string inventory_location;
	std::string inventory_list;
	u32 inventory_index;
	bool inventory_add;
	ItemStack inventory_stack;

	RollbackAction() = default;

	void setSetNode(v3s16 p_, const RollbackNode &n_old_,
			const RollbackNode &n_new_)
	{
		type = TYPE_SET_NODE;
		p = p_;
		n_old = n_old_;
		n_new = n_new_;
	}

	void setModifyInventoryStack(const std::string &inventory_location_,
			const std::string &inventory_list_, u32 index_,
			bool add_, const ItemStack &inventory_stack_)
	{
		type = TYPE_MODIFY_INVENTORY_STACK;
		inventory_location = inventory_location_;
		inventory_list = inventory_list_;
		inventory_index = index_;
		inventory_add = add_;
		inventory_stack = inventory_stack_;
	}

	// String should not contain newlines or nulls
	std::string toString() const;

	// Eg. flowing water level changes are not important
	bool isImportant(IGameDef *gamedef) const;

	bool getPosition(v3s16 *dst) const;

	bool applyRevert(Map *map, InventoryManager *imgr, IGameDef *gamedef) const;
};


class IRollbackManager
{
public:
	virtual void reportAction(const RollbackAction &action) = 0;
	virtual std::string getActor() = 0;
	virtual bool isActorGuess() = 0;
	virtual void setActor(const std::string &actor, bool is_guess) = 0;
	virtual std::string getSuspect(v3s16 p, float nearness_shortcut,
	                               float min_nearness) = 0;

	virtual ~IRollbackManager() = default;;
	virtual void flush() = 0;
	// Get all actors that did something to position p, but not further than
	// <seconds> in history
	virtual std::list<RollbackAction> getNodeActors(v3s16 pos, int range,
	                time_t seconds, int limit) = 0;
	// Get actions to revert <seconds> of history made by <actor>
	virtual std::list<RollbackAction> getRevertActions(const std::string &actor,
	                time_t seconds) = 0;
};


class RollbackScopeActor
{
public:
	RollbackScopeActor(IRollbackManager * rollback_,
			const std::string & actor, bool is_guess = false) :
		rollback(rollback_)
	{
		if (rollback) {
			old_actor = rollback->getActor();
			old_actor_guess = rollback->isActorGuess();
			rollback->setActor(actor, is_guess);
		}
	}
	~RollbackScopeActor()
	{
		if (rollback) {
			rollback->setActor(old_actor, old_actor_guess);
		}
	}

private:
	IRollbackManager * rollback;
	std::string old_actor;
	bool old_actor_guess;
};
