// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irr_v3d.h"
#include <iostream>
#include <string>
#include <vector>

class ServerActiveObject;
struct ItemStack;
class Inventory;
class IGameDef;

struct InventoryLocation
{
	enum Type{
		UNDEFINED,
		CURRENT_PLAYER,
		PLAYER,
		NODEMETA,
		DETACHED,
	} type;

	std::string name; // PLAYER, DETACHED
	v3s16 p; // NODEMETA

	InventoryLocation()
	{
		setUndefined();
	}
	void setUndefined()
	{
		type = UNDEFINED;
	}
	void setCurrentPlayer()
	{
		type = CURRENT_PLAYER;
	}
	void setPlayer(const std::string &name_)
	{
		type = PLAYER;
		name = name_;
	}
	void setNodeMeta(const v3s16 &p_)
	{
		type = NODEMETA;
		p = p_;
	}
	void setDetached(const std::string &name_)
	{
		type = DETACHED;
		name = name_;
	}

	bool operator==(const InventoryLocation &other) const
	{
		if(type != other.type)
			return false;
		switch(type){
		case UNDEFINED:
			return false;
		case CURRENT_PLAYER:
			return true;
		case PLAYER:
			return (name == other.name);
		case NODEMETA:
			return (p == other.p);
		case DETACHED:
			return (name == other.name);
		}
		return false;
	}
	bool operator!=(const InventoryLocation &other) const
	{
		return !(*this == other);
	}

	void applyCurrentPlayer(const std::string &name_)
	{
		if(type == CURRENT_PLAYER)
			setPlayer(name_);
	}

	std::string dump() const;
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);
	void deSerialize(const std::string &s);
};

struct InventoryAction;

class InventoryManager
{
public:
	InventoryManager() = default;
	virtual ~InventoryManager() = default;

	// Get an inventory (server and client)
	virtual Inventory* getInventory(const InventoryLocation &loc){return NULL;}
	// Set modified (will be saved and sent over network; only on server)
	virtual void setInventoryModified(const InventoryLocation &loc) {}
	// Send inventory action to server (only on client)
	virtual void inventoryAction(InventoryAction *a){}
};

enum class IAction : u16 {
	Move,
	Drop,
	Craft
};

struct InventoryAction
{
	static InventoryAction *deSerialize(std::istream &is);

	virtual IAction getType() const = 0;
	virtual void serialize(std::ostream &os) const = 0;
	virtual void apply(InventoryManager *mgr, ServerActiveObject *player,
			IGameDef *gamedef) = 0;
	virtual void clientApply(InventoryManager *mgr, IGameDef *gamedef) = 0;
	virtual ~InventoryAction() = default;;
};

struct MoveAction
{
	InventoryLocation from_inv;
	std::string from_list;
	s16 from_i = -1;
	InventoryLocation to_inv;
	std::string to_list;
	s16 to_i = -1;
};

struct IMoveAction : public InventoryAction, public MoveAction
{
	// count=0 means "everything"
	u16 count = 0;
	bool move_somewhere = false;

	// treat these as private
	// related to movement to somewhere
	bool caused_by_move_somewhere = false;
	u32 move_count = 0;

	IMoveAction() = default;

	IMoveAction(std::istream &is, bool somewhere);

	IAction getType() const
	{
		return IAction::Move;
	}

	void serialize(std::ostream &os) const
	{
		if (!move_somewhere)
			os << "Move ";
		else
			os << "MoveSomewhere ";
		os << count << " ";
		os << from_inv.dump() << " ";
		os << from_list << " ";
		os << from_i << " ";
		os << to_inv.dump() << " ";
		os << to_list;
		if (!move_somewhere)
			os << " " << to_i;
	}

	void apply(InventoryManager *mgr, ServerActiveObject *player, IGameDef *gamedef);

	void clientApply(InventoryManager *mgr, IGameDef *gamedef);

	void swapDirections();

	void onTake(const ItemStack &src_item, ServerActiveObject *player) const;
	void onPut(const ItemStack &src_item, ServerActiveObject *player) const;

	void onMove(int count, ServerActiveObject *player) const;

	int allowPut(const ItemStack &dst_item, ServerActiveObject *player) const;

	int allowTake(const ItemStack &src_item, ServerActiveObject *player) const;

	int allowMove(int try_take_count, ServerActiveObject *player) const;
};

struct IDropAction : public InventoryAction, public MoveAction
{
	// count=0 means "everything"
	u16 count = 0;

	IDropAction() = default;

	IDropAction(std::istream &is);

	IAction getType() const
	{
		return IAction::Drop;
	}

	void serialize(std::ostream &os) const
	{
		os<<"Drop ";
		os<<count<<" ";
		os<<from_inv.dump()<<" ";
		os<<from_list<<" ";
		os<<from_i;
	}

	void apply(InventoryManager *mgr, ServerActiveObject *player, IGameDef *gamedef);

	void clientApply(InventoryManager *mgr, IGameDef *gamedef);
};

struct ICraftAction : public InventoryAction
{
	// count=0 means "everything"
	u16 count = 0;
	InventoryLocation craft_inv;

	ICraftAction() = default;

	ICraftAction(std::istream &is);

	IAction getType() const
	{
		return IAction::Craft;
	}

	void serialize(std::ostream &os) const
	{
		os<<"Craft ";
		os<<count<<" ";
		os<<craft_inv.dump()<<" ";
	}

	void apply(InventoryManager *mgr, ServerActiveObject *player, IGameDef *gamedef);

	void clientApply(InventoryManager *mgr, IGameDef *gamedef);
};

// Crafting helper
bool getCraftingResult(Inventory *inv, ItemStack &result,
		std::vector<ItemStack> &output_replacements,
		bool decrementInput, IGameDef *gamedef);
