// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "inventorymanager.h"
#include "debug.h"
#include "log.h"
#include "serverenvironment.h"
#include "scripting_server.h"
#include "server/serveractiveobject.h"
#include "settings.h"
#include "craftdef.h"
#include "rollback_interface.h"
#include "util/strfnd.h"
#include "util/basic_macros.h"
#include "inventory.h"

#define PLAYER_TO_SA(p)   p->getEnv()->getScriptIface()

/*
	InventoryLocation
*/

std::string InventoryLocation::dump() const
{
	std::ostringstream os(std::ios::binary);
	serialize(os);
	return os.str();
}

void InventoryLocation::serialize(std::ostream &os) const
{
	switch (type) {
	case InventoryLocation::UNDEFINED:
		os<<"undefined";
		break;
	case InventoryLocation::CURRENT_PLAYER:
		os<<"current_player";
		break;
	case InventoryLocation::PLAYER:
		os<<"player:"<<name;
		break;
	case InventoryLocation::NODEMETA:
		os<<"nodemeta:"<<p.X<<","<<p.Y<<","<<p.Z;
		break;
	case InventoryLocation::DETACHED:
		os<<"detached:"<<name;
		break;
	default:
		FATAL_ERROR("Unhandled inventory location type");
	}
}

void InventoryLocation::deSerialize(std::istream &is)
{
	std::string tname;
	std::getline(is, tname, ':');
	if (tname == "undefined") {
		type = InventoryLocation::UNDEFINED;
	} else if (tname == "current_player") {
		type = InventoryLocation::CURRENT_PLAYER;
	} else if (tname == "player") {
		type = InventoryLocation::PLAYER;
		std::getline(is, name, '\n');
	} else if (tname == "nodemeta") {
		type = InventoryLocation::NODEMETA;
		std::string pos;
		std::getline(is, pos, '\n');
		Strfnd fn(pos);
		p.X = stoi(fn.next(","));
		p.Y = stoi(fn.next(","));
		p.Z = stoi(fn.next(","));
	} else if (tname == "detached") {
		type = InventoryLocation::DETACHED;
		std::getline(is, name, '\n');
	} else {
		infostream<<"Unknown InventoryLocation type=\""<<tname<<"\""<<std::endl;
		throw SerializationError("Unknown InventoryLocation type");
	}
}

void InventoryLocation::deSerialize(const std::string &s)
{
	std::istringstream is(s, std::ios::binary);
	deSerialize(is);
}

/*
	InventoryAction
*/

InventoryAction *InventoryAction::deSerialize(std::istream &is)
{
	std::string type;
	std::getline(is, type, ' ');

	InventoryAction *a = nullptr;

	if (type == "Move") {
		a = new IMoveAction(is, false);
	} else if (type == "MoveSomewhere") {
		a = new IMoveAction(is, true);
	} else if (type == "Drop") {
		a = new IDropAction(is);
	} else if (type == "Craft") {
		a = new ICraftAction(is);
	}

	return a;
}

/*
	IMoveAction
*/

IMoveAction::IMoveAction(std::istream &is, bool somewhere) :
		move_somewhere(somewhere)
{
	std::string ts;

	std::getline(is, ts, ' ');
	count = stoi(ts);

	std::getline(is, ts, ' ');
	from_inv.deSerialize(ts);

	std::getline(is, from_list, ' ');

	std::getline(is, ts, ' ');
	from_i = stoi(ts);

	std::getline(is, ts, ' ');
	to_inv.deSerialize(ts);

	std::getline(is, to_list, ' ');

	if (!somewhere) {
		std::getline(is, ts, ' ');
		to_i = stoi(ts);
	}
}

void IMoveAction::swapDirections()
{
	std::swap(from_inv, to_inv);
	std::swap(from_list, to_list);
	std::swap(from_i, to_i);
}

void IMoveAction::onTake(const ItemStack &src_item, ServerActiveObject *player) const
{
	ServerScripting *sa = PLAYER_TO_SA(player);
	if (from_inv.type == InventoryLocation::DETACHED)
		sa->detached_inventory_OnTake(*this, src_item, player);
	else if (from_inv.type == InventoryLocation::NODEMETA)
		sa->nodemeta_inventory_OnTake(*this, src_item, player);
	else if (from_inv.type == InventoryLocation::PLAYER)
		sa->player_inventory_OnTake(*this, src_item, player);
	else
		assert(false);
}

void IMoveAction::onPut(const ItemStack &src_item, ServerActiveObject *player) const
{
	ServerScripting *sa = PLAYER_TO_SA(player);
	if (to_inv.type == InventoryLocation::DETACHED)
		sa->detached_inventory_OnPut(*this, src_item, player);
	else if (to_inv.type == InventoryLocation::NODEMETA)
		sa->nodemeta_inventory_OnPut(*this, src_item, player);
	else if (to_inv.type == InventoryLocation::PLAYER)
		sa->player_inventory_OnPut(*this, src_item, player);
	else
		assert(false);
}

void IMoveAction::onMove(int count, ServerActiveObject *player) const
{
	ServerScripting *sa = PLAYER_TO_SA(player);
	if (from_inv.type == InventoryLocation::DETACHED)
		sa->detached_inventory_OnMove(*this, count, player);
	else if (from_inv.type == InventoryLocation::NODEMETA)
		sa->nodemeta_inventory_OnMove(*this, count, player);
	else if (from_inv.type == InventoryLocation::PLAYER)
		sa->player_inventory_OnMove(*this, count, player);
	else
		assert(false);
}

int IMoveAction::allowPut(const ItemStack &dst_item, ServerActiveObject *player) const
{
	ServerScripting *sa = PLAYER_TO_SA(player);
	int dst_can_put_count = 0xffff;
	if (to_inv.type == InventoryLocation::DETACHED)
		dst_can_put_count = sa->detached_inventory_AllowPut(*this, dst_item, player);
	else if (to_inv.type == InventoryLocation::NODEMETA)
		dst_can_put_count = sa->nodemeta_inventory_AllowPut(*this, dst_item, player);
	else if (to_inv.type == InventoryLocation::PLAYER)
		dst_can_put_count = sa->player_inventory_AllowPut(*this, dst_item, player);
	else
		assert(false);
	return dst_can_put_count;
}

int IMoveAction::allowTake(const ItemStack &src_item, ServerActiveObject *player) const
{
	ServerScripting *sa = PLAYER_TO_SA(player);
	int src_can_take_count = 0xffff;
	if (from_inv.type == InventoryLocation::DETACHED)
		src_can_take_count = sa->detached_inventory_AllowTake(*this, src_item, player);
	else if (from_inv.type == InventoryLocation::NODEMETA)
		src_can_take_count = sa->nodemeta_inventory_AllowTake(*this, src_item, player);
	else if (from_inv.type == InventoryLocation::PLAYER)
		src_can_take_count = sa->player_inventory_AllowTake(*this, src_item, player);
	else
		assert(false);
	return src_can_take_count;
}

int IMoveAction::allowMove(int try_take_count, ServerActiveObject *player) const
{
	ServerScripting *sa = PLAYER_TO_SA(player);
	int src_can_take_count = 0xffff;
	if (from_inv.type == InventoryLocation::DETACHED)
		src_can_take_count = sa->detached_inventory_AllowMove(*this, try_take_count, player);
	else if (from_inv.type == InventoryLocation::NODEMETA)
		src_can_take_count = sa->nodemeta_inventory_AllowMove(*this, try_take_count, player);
	else if (from_inv.type == InventoryLocation::PLAYER)
		src_can_take_count = sa->player_inventory_AllowMove(*this, try_take_count, player);
	else
		assert(false);
	return src_can_take_count;
}

void IMoveAction::apply(InventoryManager *mgr, ServerActiveObject *player, IGameDef *gamedef)
{
	/// Necessary for executing Lua callbacks which may manipulate the inventory,
	/// hence invalidate pointers needed by IMoveAction::apply
	auto get_borrow_checked_invlist = [mgr](const InventoryLocation &invloc,
			const std::string &listname) -> InventoryList::ResizeLocked
	{
		Inventory *inv = mgr->getInventory(invloc);
		if (!inv)
			return nullptr;
		InventoryList *list = inv->getList(listname);
		if (!list)
			return nullptr;
		return list->resizeLock();
	};

	auto list_from = get_borrow_checked_invlist(from_inv, from_list);
	auto list_to = get_borrow_checked_invlist(to_inv, to_list);

	if (!list_from) {
		infostream << "IMoveAction::apply(): FAIL: source inventory or list not found: "
				<< "from_inv=\"" << from_inv.dump() << "\""
				<< ", from_list=\"" << from_list << "\""
				<< ", to_inv=\"" << to_inv.dump() << "\""
				<< ", to_list=\"" << to_list << "\""
				<< std::endl;
		return;
	}
	if (!list_to) {
		infostream << "IMoveAction::apply(): FAIL: destination inventory or list not found: "
				<< "from_inv=\"" << from_inv.dump() << "\""
				<< ", from_list=\"" << from_list << "\""
				<< ", to_inv=\"" << to_inv.dump() << "\""
				<< ", to_list=\"" << to_list << "\""
				<< std::endl;
		return;
	}

	if (move_somewhere) {
		list_from.reset();

		s16 old_to_i = to_i;
		u16 old_count = count;
		caused_by_move_somewhere = true;
		move_somewhere = false;

		infostream << "IMoveAction::apply(): moving item somewhere"
			<< " msom=" << move_somewhere
			<< " count=" << count
			<< " from inv=\"" << from_inv.dump() << "\""
			<< " list=\"" << from_list << "\""
			<< " i=" << from_i
			<< " to inv=\"" << to_inv.dump() << "\""
			<< " list=\"" << to_list << "\""
			<< std::endl;

		// Try to add the item to destination list
		s16 dest_size = list_to->getSize();
		auto add_to = [&](s16 dest_i) {
			// release resize lock while the callbacks are happening
			list_to.reset();

			to_i = dest_i;
			apply(mgr, player, gamedef);
			assert(move_count <= count);
			count -= move_count;

			list_to = get_borrow_checked_invlist(to_inv, to_list);
			if (!list_to) {
				// list_to was removed. simulate an empty list
				dest_size = 0;
				return;
			}
			dest_size = list_to->getSize();
		};
		// First try all the non-empty slots
		for (s16 dest_i = 0; dest_i < dest_size && count > 0; dest_i++) {
			if (!list_to->getItem(dest_i).empty())
				add_to(dest_i);
		}
		// Then try all the empty ones
		for (s16 dest_i = 0; dest_i < dest_size && count > 0; dest_i++) {
			if (list_to->getItem(dest_i).empty())
				add_to(dest_i);
		}

		to_i = old_to_i;
		count = old_count;
		caused_by_move_somewhere = false;
		move_somewhere = true;
		return;
	}

	if (from_i < 0 || list_from->getSize() <= (u32) from_i) {
		infostream << "IMoveAction::apply(): FAIL: source index out of bounds: "
			<< "size of from_list=\"" << list_from->getSize() << "\""
			<< ", from_index=\"" << from_i << "\"" << std::endl;
		return;
	}

	if (to_i < 0 || list_to->getSize() <= (u32) to_i) {
		infostream << "IMoveAction::apply(): FAIL: destination index out of bounds: "
			<< "size of to_list=\"" << list_to->getSize() << "\""
			<< ", to_index=\"" << to_i << "\"" << std::endl;
		return;
	}

	if (list_from.get() == list_to.get() && from_i == to_i)
		return; // Same slot

	/*
		Do not handle rollback if both inventories are that of the same player
	*/
	bool ignore_rollback = (
		from_inv.type == InventoryLocation::PLAYER &&
		from_inv == to_inv);

	/*
		Collect information of endpoints
	*/

	ItemStack src_item = list_from->getItem(from_i);
	if (count > 0 && count < src_item.count)
		src_item.count = count;
	if (src_item.empty())
		return;

	int src_can_take_count = 0xffff;
	int dst_can_put_count = 0xffff;

	// this is needed for swapping items inside one inventory to work
	ItemStack restitem;
	bool allow_swap = !list_to->itemFits(to_i, src_item, &restitem)
		&& restitem.count == src_item.count
		&& !caused_by_move_somewhere;
	// move_count : How many items that were moved at the end
	// count      : Total items "in the queue" of being moved. Do not touch.
	move_count = src_item.count - restitem.count;

	// Shift-click: Cannot fill this stack, proceed with next slot
	if (caused_by_move_somewhere && move_count == 0) {
		return;
	}

	if (allow_swap) {
		// Swap will affect the entire stack (= count) if it can performed.
		src_item = list_from->getItem(from_i);
		move_count = src_item.count;
	}
	src_item.count = move_count; // Temporary movement stack

	if (from_inv == to_inv) {
		// Move action within the same inventory
		src_can_take_count = allowMove(src_item.count, player);

		bool swap_expected = allow_swap;
		allow_swap = allow_swap
			&& (src_can_take_count == -1 || src_can_take_count >= src_item.count);
		if (allow_swap) {
			int try_put_count = list_to->getItem(to_i).count;
			swapDirections();
			dst_can_put_count = allowMove(try_put_count, player);
			allow_swap = allow_swap
				&& (dst_can_put_count == -1 || dst_can_put_count >= try_put_count);
			swapDirections();
		} else {
			dst_can_put_count = src_can_take_count;
		}
		if (swap_expected != allow_swap)
			src_can_take_count = dst_can_put_count = 0;
	} else {
		// Take from one inventory, put into another
		dst_can_put_count = allowPut(src_item, player);
		src_can_take_count = allowTake(src_item, player);

		bool swap_expected = allow_swap;
		allow_swap = allow_swap
			&& (src_can_take_count == -1 || src_can_take_count >= src_item.count)
			&& (dst_can_put_count == -1 || dst_can_put_count >= src_item.count);
		// A swap is expected, which means that we have to
		// run the "allow" callbacks a second time with swapped inventories
		if (allow_swap) {
			ItemStack dst_item = list_to->getItem(to_i);
			swapDirections();

			int src_can_take = allowPut(dst_item, player);
			int dst_can_put = allowTake(dst_item, player);
			allow_swap = allow_swap
				&& (src_can_take == -1 || src_can_take >= dst_item.count)
				&& (dst_can_put == -1 || dst_can_put >= dst_item.count);
			swapDirections();
		}
		if (swap_expected != allow_swap)
			src_can_take_count = dst_can_put_count = 0;
	}

	int old_move_count = move_count;

	// Apply limits given by allow_* callbacks
	if (src_can_take_count != -1)
		move_count = (u32)std::min<s32>(src_can_take_count, move_count);
	if (dst_can_put_count != -1)
		move_count = (u32)std::min<s32>(dst_can_put_count, move_count);

	// allow_* callbacks should not modify the stack - but if they do - handle that.
	if (move_count > list_from->getItem(from_i).count)
		move_count = list_from->getItem(from_i).count;

	/* If no items will be moved, don't go further */
	if (move_count == 0) {
		// Undo client prediction. See 'clientApply'
		if (from_inv.type == InventoryLocation::PLAYER)
			list_from->setModified();

		if (to_inv.type == InventoryLocation::PLAYER)
			list_to->setModified();

		infostream<<"IMoveAction::apply(): move was completely disallowed:"
				<<" move_count="<<old_move_count
				<<" from inv=\""<<from_inv.dump()<<"\""
				<<" list=\""<<from_list<<"\""
				<<" i="<<from_i
				<<" to inv=\""<<to_inv.dump()<<"\""
				<<" list=\""<<to_list<<"\""
				<<" i="<<to_i
				<<std::endl;

		return;
	}

	// Backups stacks for infinite sources
	ItemStack from_stack_was = list_from->getItem(from_i);
	ItemStack to_stack_was = list_to->getItem(to_i);

	/*
		Perform actual move

		If something is wrong (source item is empty, destination is the
		same as source), nothing happens
	*/
	bool did_swap = false;
	src_item = list_from->moveItem(from_i,
		list_to.get(), to_i, move_count, allow_swap, &did_swap);
	move_count = src_item.count;

	assert(allow_swap == did_swap);

	// If source is infinite, reset its stack
	if (src_can_take_count == -1) {
		// For the caused_by_move_somewhere == true case we didn't force-put the item,
		// which guarantees there is no leftover, and code below would duplicate the
		// (not replaced) to_stack_was item.
		if (!caused_by_move_somewhere) {
			// If destination stack is of different type and there are leftover
			// items, attempt to put the leftover items to a different place in the
			// destination inventory.
			// The client-side GUI will try to guess if this happens.
			if (from_stack_was.name != to_stack_was.name) {
				for (u32 i = 0; i < list_to->getSize(); i++) {
					if (list_to->getItem(i).empty()) {
						list_to->changeItem(i, to_stack_was);
						break;
					}
				}
			}
		}
		if (move_count > 0 || did_swap) {
			list_from->changeItem(from_i, from_stack_was);
		}
	}
	// If destination is infinite, reset its stack and take count from source
	if (dst_can_put_count == -1) {
		list_to->changeItem(to_i, to_stack_was);
		if (did_swap) {
			// Undo swap result: set the expected stack + size
			list_from->changeItem(from_i, from_stack_was);
			list_from->takeItem(from_i, move_count);
		}
	}

	infostream << "IMoveAction::apply(): moved"
			<< " msom=" << move_somewhere
			<< " caused=" << caused_by_move_somewhere
			<< " move_count=" << move_count
			<< " from inv=\"" << from_inv.dump() << "\""
			<< " list=\"" << from_list << "\""
			<< " i=" << from_i
			<< " to inv=\"" << to_inv.dump() << "\""
			<< " list=\"" << to_list << "\""
			<< " i=" << to_i
			<< std::endl;

	// If we are inside the move somewhere loop, we don't need to report
	// anything if nothing happened
	if (caused_by_move_somewhere && move_count == 0)
		return;

	/*
		Record rollback information
	*/
	if (!ignore_rollback && gamedef->rollback()) {
		IRollbackManager *rollback = gamedef->rollback();

		// If source is not infinite, record item take
		if (src_can_take_count != -1) {
			RollbackAction action;
			std::string loc;
			{
				std::ostringstream os(std::ios::binary);
				from_inv.serialize(os);
				loc = os.str();
			}
			action.setModifyInventoryStack(loc, from_list, from_i, false,
					src_item);
			rollback->reportAction(action);
		}
		// If destination is not infinite, record item put
		if (dst_can_put_count != -1) {
			RollbackAction action;
			std::string loc;
			{
				std::ostringstream os(std::ios::binary);
				to_inv.serialize(os);
				loc = os.str();
			}
			action.setModifyInventoryStack(loc, to_list, to_i, true,
					src_item);
			rollback->reportAction(action);
		}
	}

	/*
		Report move to endpoints
	*/
	list_to.reset();
	list_from.reset();

	// Source = destination => move
	if (from_inv == to_inv) {
		onMove(move_count, player);
		if (did_swap) {
			// Already swapped. The other stack is now placed in "from" list
			list_from = get_borrow_checked_invlist(from_inv, from_list);
			if (list_from) {
				src_item = list_from->getItem(from_i);
				list_from.reset();
				swapDirections();
				onMove(src_item.count, player);
				swapDirections();
			}
		}
		mgr->setInventoryModified(from_inv);
	} else {
		ItemStack swap_item;
		if (did_swap) {
			// Already swapped. The other stack is now placed in "from" list
			list_from = get_borrow_checked_invlist(from_inv, from_list);
			if (list_from) {
				swap_item = list_from->getItem(from_i);
				list_from.reset();
			}
		}

		// 1. Take the ItemStack (visually: freely detached)
		onTake(src_item, player);
		if (!swap_item.empty() && get_borrow_checked_invlist(to_inv, to_list)) {
			swapDirections();
			onTake(swap_item, player);
			swapDirections();
		}

		// 2. Put the ItemStack
		if (get_borrow_checked_invlist(to_inv, to_list))
			onPut(src_item, player);

		if (!swap_item.empty() && get_borrow_checked_invlist(to_inv, to_list)) {
			swapDirections();
			onPut(swap_item, player);
			swapDirections();
		}

		mgr->setInventoryModified(to_inv);
		mgr->setInventoryModified(from_inv);
	}
}

void IMoveAction::clientApply(InventoryManager *mgr, IGameDef *gamedef)
{
	// Optional InventoryAction operation that is run on the client
	// to make lag less apparent.

	Inventory *inv_from = mgr->getInventory(from_inv);
	Inventory *inv_to = mgr->getInventory(to_inv);
	if (!inv_from || !inv_to)
		return;

	InventoryLocation current_player;
	current_player.setCurrentPlayer();
	Inventory *inv_player = mgr->getInventory(current_player);
	if (inv_from != inv_player || inv_to != inv_player)
		return;

	InventoryList *list_from = inv_from->getList(from_list);
	InventoryList *list_to = inv_to->getList(to_list);
	if (!list_from || !list_to)
		return;

	if (!move_somewhere)
		list_from->moveItem(from_i, list_to, to_i, count);
	else
		list_from->moveItemSomewhere(from_i, list_to, count);

	mgr->setInventoryModified(from_inv);
	if (inv_from != inv_to)
		mgr->setInventoryModified(to_inv);
}

/*
	IDropAction
*/

IDropAction::IDropAction(std::istream &is)
{
	std::string ts;

	std::getline(is, ts, ' ');
	count = stoi(ts);

	std::getline(is, ts, ' ');
	from_inv.deSerialize(ts);

	std::getline(is, from_list, ' ');

	std::getline(is, ts, ' ');
	from_i = stoi(ts);
}

void IDropAction::apply(InventoryManager *mgr, ServerActiveObject *player, IGameDef *gamedef)
{
	Inventory *inv_from = mgr->getInventory(from_inv);

	if (!inv_from) {
		infostream<<"IDropAction::apply(): FAIL: source inventory not found: "
				<<"from_inv=\""<<from_inv.dump()<<"\""<<std::endl;
		return;
	}

	InventoryList *list_from = inv_from->getList(from_list);

	/*
		If a list doesn't exist or the source item doesn't exist
	*/
	if (!list_from) {
		infostream<<"IDropAction::apply(): FAIL: source list not found: "
				<<"from_inv=\""<<from_inv.dump()<<"\""<<std::endl;
		return;
	}
	if (list_from->getItem(from_i).empty()) {
		infostream<<"IDropAction::apply(): FAIL: source item not found: "
				<<"from_inv=\""<<from_inv.dump()<<"\""
				<<", from_list=\""<<from_list<<"\""
				<<" from_i="<<from_i<<std::endl;
		return;
	}

	auto list_from_lock = list_from->resizeLock();

	/*
		Do not handle rollback if inventory is player's
	*/
	bool ignore_src_rollback = (from_inv.type == InventoryLocation::PLAYER);

	/*
		Collect information of endpoints
	*/

	int take_count = list_from->getItem(from_i).count;
	if (count != 0 && count < take_count)
		take_count = count;
	int src_can_take_count = take_count;

	ItemStack src_item = list_from->getItem(from_i);
	src_item.count = take_count;

	// Run callbacks depending on source inventory
	switch (from_inv.type) {
	case InventoryLocation::DETACHED:
		src_can_take_count = PLAYER_TO_SA(player)->detached_inventory_AllowTake(
			*this, src_item, player);
		break;
	case InventoryLocation::NODEMETA:
		src_can_take_count = PLAYER_TO_SA(player)->nodemeta_inventory_AllowTake(
			*this, src_item, player);
		break;
	case InventoryLocation::PLAYER:
		src_can_take_count = PLAYER_TO_SA(player)->player_inventory_AllowTake(
			*this, src_item, player);
		break;
	default:
		break;
	}

	if (src_can_take_count != -1 && src_can_take_count < take_count)
		take_count = src_can_take_count;

	// Update item due executed callbacks
	src_item = list_from->getItem(from_i);

	// Drop the item
	ItemStack item1 = list_from->getItem(from_i);
	item1.count = take_count;
	if(PLAYER_TO_SA(player)->item_OnDrop(item1, player,
				player->getBasePosition())) {
		int actually_dropped_count = take_count - item1.count;

		if (actually_dropped_count == 0) {
			infostream<<"Actually dropped no items"<<std::endl;

			// Revert client prediction. See 'clientApply'
			if (from_inv.type == InventoryLocation::PLAYER)
				list_from->setModified();
			return;
		}

		// If source isn't infinite
		if (src_can_take_count != -1) {
			// Take item from source list
			ItemStack item2 = list_from->takeItem(from_i, actually_dropped_count);

			if (item2.count != actually_dropped_count)
				errorstream<<"Could not take dropped count of items"<<std::endl;
		}

		src_item.count = actually_dropped_count;
		mgr->setInventoryModified(from_inv);
	}

	infostream<<"IDropAction::apply(): dropped "
			<<" from inv=\""<<from_inv.dump()<<"\""
			<<" list=\""<<from_list<<"\""
			<<" i="<<from_i
			<<std::endl;


	/*
		Report drop to endpoints
	*/
	list_from_lock.reset();

	switch (from_inv.type) {
	case InventoryLocation::DETACHED:
		PLAYER_TO_SA(player)->detached_inventory_OnTake(
			*this, src_item, player);
		break;
	case InventoryLocation::NODEMETA:
		PLAYER_TO_SA(player)->nodemeta_inventory_OnTake(
			*this, src_item, player);
		break;
	case InventoryLocation::PLAYER:
		PLAYER_TO_SA(player)->player_inventory_OnTake(
			*this, src_item, player);
		break;
	default:
		break;
	}

	/*
		Record rollback information
	*/
	if (!ignore_src_rollback && gamedef->rollback()) {
		IRollbackManager *rollback = gamedef->rollback();

		// If source is not infinite, record item take
		if (src_can_take_count != -1) {
			RollbackAction action;
			std::string loc;
			{
				std::ostringstream os(std::ios::binary);
				from_inv.serialize(os);
				loc = os.str();
			}
			action.setModifyInventoryStack(loc, from_list, from_i,
					false, src_item);
			rollback->reportAction(action);
		}
	}
}

void IDropAction::clientApply(InventoryManager *mgr, IGameDef *gamedef)
{
	// Optional InventoryAction operation that is run on the client
	// to make lag less apparent.

	Inventory *inv_from = mgr->getInventory(from_inv);
	if (!inv_from)
		return;

	InventoryLocation current_player;
	current_player.setCurrentPlayer();
	Inventory *inv_player = mgr->getInventory(current_player);
	if (inv_from != inv_player)
		return;

	InventoryList *list_from = inv_from->getList(from_list);
	if (!list_from)
		return;

	if (count == 0)
		list_from->changeItem(from_i, ItemStack());
	else
		list_from->takeItem(from_i, count);

	mgr->setInventoryModified(from_inv);
}

/*
	ICraftAction
*/

ICraftAction::ICraftAction(std::istream &is)
{
	std::string ts;

	std::getline(is, ts, ' ');
	count = stoi(ts);

	std::getline(is, ts, ' ');
	craft_inv.deSerialize(ts);
}

void ICraftAction::apply(InventoryManager *mgr,
	ServerActiveObject *player, IGameDef *gamedef)
{
	Inventory *inv_craft = mgr->getInventory(craft_inv);

	if (!inv_craft) {
		infostream << "ICraftAction::apply(): FAIL: inventory not found: "
				<< "craft_inv=\"" << craft_inv.dump() << "\"" << std::endl;
		return;
	}

	InventoryList *list_craft = inv_craft->getList("craft");
	InventoryList *list_craftresult = inv_craft->getList("craftresult");
	InventoryList *list_main = inv_craft->getList("main");

	/*
		If a list doesn't exist or the source item doesn't exist
	*/
	if (!list_craft) {
		infostream << "ICraftAction::apply(): FAIL: craft list not found: "
				<< "craft_inv=\"" << craft_inv.dump() << "\"" << std::endl;
		return;
	}
	if (!list_craftresult) {
		infostream << "ICraftAction::apply(): FAIL: craftresult list not found: "
				<< "craft_inv=\"" << craft_inv.dump() << "\"" << std::endl;
		return;
	}
	if (list_craftresult->getSize() < 1) {
		infostream << "ICraftAction::apply(): FAIL: craftresult list too short: "
				<< "craft_inv=\"" << craft_inv.dump() << "\"" << std::endl;
		return;
	}

	auto list_craft_lock       = list_craft->resizeLock();
	auto list_craftresult_lock = list_craftresult->resizeLock();
	auto list_main_lock        = list_main->resizeLock();

	ItemStack crafted;
	ItemStack craftresultitem;
	int count_remaining = count;
	std::vector<ItemStack> output_replacements;
	getCraftingResult(inv_craft, crafted, output_replacements, false, gamedef);
	PLAYER_TO_SA(player)->item_CraftPredict(crafted, player, list_craft, craft_inv);
	bool found = !crafted.empty();

	while (found && list_craftresult->itemFits(0, crafted)) {
		InventoryList saved_craft_list = *list_craft;

		std::vector<ItemStack> temp;
		// Decrement input and add crafting output
		getCraftingResult(inv_craft, crafted, temp, true, gamedef);
		PLAYER_TO_SA(player)->item_OnCraft(crafted, player, &saved_craft_list, craft_inv);
		list_craftresult->addItem(0, crafted);
		mgr->setInventoryModified(craft_inv);

		// Add the new replacements to the list
		IItemDefManager *itemdef = gamedef->getItemDefManager();
		for (auto &itemstack : temp) {
			for (auto &output_replacement : output_replacements) {
				if (itemstack.name == output_replacement.name) {
					itemstack = output_replacement.addItem(itemstack, itemdef);
					if (itemstack.empty())
						continue;
				}
			}
			output_replacements.push_back(itemstack);
		}

		actionstream << player->getDescription()
				<< " crafts "
				<< crafted.getItemString()
				<< std::endl;

		// Decrement counter
		if (count_remaining == 1)
			break;

		if (count_remaining > 1)
			count_remaining--;

		// Get next crafting result
		getCraftingResult(inv_craft, crafted, temp, false, gamedef);
		PLAYER_TO_SA(player)->item_CraftPredict(crafted, player, list_craft, craft_inv);
		found = !crafted.empty();
	}

	// Put the replacements in the inventory or drop them on the floor, if
	// the inventory is full
	for (auto &output_replacement : output_replacements) {
		if (list_main)
			output_replacement = list_main->addItem(output_replacement);
		if (output_replacement.empty())
			continue;
		u16 count = output_replacement.count;
		do {
			PLAYER_TO_SA(player)->item_OnDrop(output_replacement, player,
				player->getBasePosition());
			if (count <= output_replacement.count) {
				errorstream << "Couldn't drop replacement stack " <<
					output_replacement.getItemString() << " because drop loop didn't "
					"decrease count." << std::endl;

				break;
			}
		} while (!output_replacement.empty());
	}

	infostream<<"ICraftAction::apply(): crafted "
			<<" craft_inv=\""<<craft_inv.dump()<<"\""
			<<std::endl;
}

void ICraftAction::clientApply(InventoryManager *mgr, IGameDef *gamedef)
{
	// Optional InventoryAction operation that is run on the client
	// to make lag less apparent.
}


// Crafting helper
bool getCraftingResult(Inventory *inv, ItemStack &result,
		std::vector<ItemStack> &output_replacements,
		bool decrementInput, IGameDef *gamedef)
{
	result.clear();

	// Get the InventoryList in which we will operate
	InventoryList *clist = inv->getList("craft");
	if (!clist)
		return false;

	// Mangle crafting grid to another format
	CraftInput ci;
	ci.method = CRAFT_METHOD_NORMAL;
	ci.width = clist->getWidth() ? clist->getWidth() : 3;
	for (u16 i=0; i < clist->getSize(); i++)
		ci.items.push_back(clist->getItem(i));

	// Find out what is crafted and add it to result item slot
	CraftOutput co;
	bool found = gamedef->getCraftDefManager()->getCraftResult(
			ci, co, output_replacements, decrementInput, gamedef);
	if (found)
		result.deSerialize(co.item, gamedef->getItemDefManager());

	if (found && decrementInput) {
		// CraftInput has been changed, apply changes in clist
		for (u16 i=0; i < clist->getSize(); i++) {
			clist->changeItem(i, ci.items[i]);
		}
	}

	return found;
}

