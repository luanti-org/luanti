// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "itemdef.h"
#include "irrlichttypes.h"
#include "itemstackmetadata.h"
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <cassert>

struct ToolCapabilities;

struct ItemStack
{
	ItemStack() = default;

	ItemStack(const std::string &name_, u16 count_,
			u16 wear, IItemDefManager *itemdef);

	~ItemStack() = default;

	// Serialization
	void serialize(std::ostream &os, bool serialize_meta = true) const;
	// Deserialization. Pass itemdef unless you don't want aliases resolved.
	void deSerialize(std::istream &is, IItemDefManager *itemdef = NULL);
	void deSerialize(const std::string &s, IItemDefManager *itemdef = NULL);

	// Returns the string used for inventory
	std::string getItemString(bool include_meta = true) const;
	// Returns the tooltip
	std::string getDescription(const IItemDefManager *itemdef) const;
	std::string getShortDescription(const IItemDefManager *itemdef) const;

	std::string getInventoryImage(const IItemDefManager *itemdef) const;
	std::string getInventoryOverlay(const IItemDefManager *itemdef) const;
	std::string getWieldImage(const IItemDefManager *itemdef) const;
	std::string getWieldOverlay(const IItemDefManager *itemdef) const;
	v3f getWieldScale(const IItemDefManager *itemdef) const;

	/*
		Quantity methods
	*/

	bool empty() const
	{
		return count == 0;
	}

	void clear()
	{
		name = "";
		count = 0;
		wear = 0;
		metadata.clear();
	}

	void add(u16 n)
	{
		count += n;
	}

	void remove(u16 n)
	{
		assert(count >= n); // Pre-condition
		count -= n;
		if(count == 0)
			clear(); // reset name, wear and metadata too
	}

	// Maximum size of a stack
	u16 getStackMax(const IItemDefManager *itemdef) const
	{
		return itemdef->get(name).stack_max;
	}

	// Number of items that can be added to this stack
	u16 freeSpace(const IItemDefManager *itemdef) const
	{
		u16 max = getStackMax(itemdef);
		if (count >= max)
			return 0;
		return max - count;
	}

	// Returns false if item is not known and cannot be used
	bool isKnown(const IItemDefManager *itemdef) const
	{
		return itemdef->isKnown(name);
	}

	// Returns a pointer to the item definition struct,
	// or a fallback one (name="unknown") if the item is unknown.
	const ItemDefinition& getDefinition(
			const IItemDefManager *itemdef) const
	{
		return itemdef->get(name);
	}

	// Get tool digging properties, or those of the hand if not a tool
	// If not hand assumes default hand ""
	const ToolCapabilities& getToolCapabilities(
			const IItemDefManager *itemdef, const ItemStack *hand = nullptr) const
	{
		const ToolCapabilities *item_cap = itemdef->get(name).tool_capabilities;

		if (item_cap) {
			return metadata.getToolCapabilities(*item_cap); // Check for override
		}

		// Fall back to the hand's tool capabilities
		if (hand) {
			item_cap = itemdef->get(hand->name).tool_capabilities;
			if (item_cap) {
				return hand->metadata.getToolCapabilities(*item_cap);
			}
		}

		item_cap = itemdef->get("").tool_capabilities;
		assert(item_cap);
		return *item_cap;
	}

	const std::optional<WearBarParams> &getWearBarParams(
			const IItemDefManager *itemdef) const
	{
		auto &meta_override = metadata.getWearBarParamOverride();
		if (meta_override.has_value())
			return meta_override;
		return itemdef->get(name).wear_bar_params;
	}

	// Wear out (only tools)
	// Returns true if the item is (was) a tool
	bool addWear(s32 amount, const IItemDefManager *itemdef)
	{
		if(getDefinition(itemdef).type == ITEM_TOOL)
		{
			if(amount > 65535 - wear)
				clear();
			else if(amount < -wear)
				wear = 0;
			else
				wear += amount;
			return true;
		}

		return false;
	}

	// If possible, adds newitem to this item.
	// If cannot be added at all, returns the item back.
	// If can be added partly, decremented item is returned back.
	// If can be added fully, empty item is returned.
	ItemStack addItem(ItemStack newitem, IItemDefManager *itemdef);

	// Checks whether newitem could be added.
	// If restitem is non-NULL, it receives the part of newitem that
	// would be left over after adding.
	bool itemFits(ItemStack newitem,
			ItemStack *restitem,  // may be NULL
			IItemDefManager *itemdef) const;

	// Checks if another itemstack would stack with this one.
	// Does not check if the item actually fits in the stack.
	bool stacksWith(const ItemStack &other) const;

	// Takes some items.
	// If there are not enough, takes as many as it can.
	// Returns empty item if couldn't take any.
	ItemStack takeItem(u32 takecount);

	// Similar to takeItem, but keeps this ItemStack intact.
	ItemStack peekItem(u32 peekcount) const;

	bool operator ==(const ItemStack &s) const
	{
		return (this->name     == s.name &&
				this->count    == s.count &&
				this->wear     == s.wear &&
				this->metadata == s.metadata);
	}

	bool operator !=(const ItemStack &s) const
	{
		return !(*this == s);
	}

	/*
		Properties
	*/
	std::string name = "";
	u16 count = 0;
	u16 wear = 0;
	ItemStackMetadata metadata;
};

class InventoryList
{
public:
	InventoryList(const std::string &name, u32 size, IItemDefManager *itemdef);
	~InventoryList() = default;
	void clearItems();
	void setSize(u32 newsize);
	void setWidth(u32 newWidth);
	void setName(const std::string &name);
	void serialize(std::ostream &os, bool incremental) const;
	void deSerialize(std::istream &is);

	InventoryList(const InventoryList &other) { *this = other; }
	InventoryList & operator = (const InventoryList &other);
	bool operator == (const InventoryList &other) const;
	bool operator != (const InventoryList &other) const
	{
		return !(*this == other);
	}

	const std::string &getName() const { return m_name; }
	u32 getSize() const { return static_cast<u32>(m_items.size()); }
	u32 getWidth() const { return m_width; }
	// Count used slots
	u32 getUsedSlots() const;

	// Get reference to item
	const ItemStack &getItem(u32 i) const
	{
		assert(i < m_size); // Pre-condition
		return m_items[i];
	}
	ItemStack &getItem(u32 i)
	{
		assert(i < m_size); // Pre-condition
		return m_items[i];
	}
	// Get reference to all items
	const std::vector<ItemStack> &getItems() const { return m_items; }
	// Returns old item. Parameter can be an empty item.
	ItemStack changeItem(u32 i, const ItemStack &newitem);
	// Delete item
	void deleteItem(u32 i);

	// Adds an item to a suitable place. Returns leftover item (possibly empty).
	ItemStack addItem(const ItemStack &newitem);

	// If possible, adds item to given slot.
	// If cannot be added at all, returns the item back.
	// If can be added partly, decremented item is returned back.
	// If can be added fully, empty item is returned.
	ItemStack addItem(u32 i, const ItemStack &newitem);

	// Checks whether the item could be added to the given slot
	// If restitem is non-NULL, it receives the part of newitem that
	// would be left over after adding.
	bool itemFits(const u32 i, const ItemStack &newitem,
			ItemStack *restitem = NULL) const;

	// Checks whether there is room for a given item
	bool roomForItem(const ItemStack &item) const;

	// Checks whether the given count of the given item
	// exists in this inventory list.
	// If match_meta is false, only the items' names are compared.
	bool containsItem(const ItemStack &item, bool match_meta) const;

	// Removes the given count of the given item name from
	// this inventory list. Walks the list in reverse order.
	// If not as many items exist as requested, removes as
	// many as possible.
	// Returns the items that were actually removed.
	ItemStack removeItem(const ItemStack &item, bool match_meta);

	// Takes some items from a slot.
	// If there are not enough, takes as many as it can.
	// Returns empty item if couldn't take any.
	ItemStack takeItem(u32 i, u32 takecount);

	// Move an item to a different list (or a different stack in the same list)
	// count is the maximum number of items to move (0 for everything)
	// returns the moved stack
	ItemStack moveItem(u32 i, InventoryList *dest, u32 dest_i,
		u32 count = 0, bool swap_if_needed = true, bool *did_swap = NULL);

	// like moveItem, but without a fixed destination index
	// also with optional rollback recording
	void moveItemSomewhere(u32 i, InventoryList *dest, u32 count);

	inline bool checkModified() const { return m_dirty; }
	inline void setModified(bool dirty = true) { m_dirty = dirty; }

	// Problem: C++ keeps references to InventoryList and ItemStack indices
	// until a better solution is found, this serves as a guard to prevent side-effects
	struct ResizeUnlocker {
		void operator()(InventoryList *invlist)
		{
			invlist->m_resize_locks -= 1;
		}
	};
	using ResizeLocked = std::unique_ptr<InventoryList, ResizeUnlocker>;

	void checkResizeLock();

	inline ResizeLocked resizeLock()
	{
		m_resize_locks += 1;
		return ResizeLocked(this);
	}

private:
	std::vector<ItemStack> m_items;
	std::string m_name;
	u32 m_size; // always the same as m_items.size()
	u32 m_width = 0;
	IItemDefManager *m_itemdef;
	bool m_dirty = true;
	int m_resize_locks = 0; // Lua callback sanity
};

class Inventory
{
public:
	~Inventory();

	void clear();

	Inventory(IItemDefManager *itemdef);
	Inventory(const Inventory &other);
	Inventory & operator = (const Inventory &other);
	bool operator == (const Inventory &other) const;
	bool operator != (const Inventory &other) const
	{
		return !(*this == other);
	}

	// Never ever serialize to disk using "incremental"!
	void serialize(std::ostream &os, bool incremental = false) const;
	void deSerialize(std::istream &is);

	// Creates a new list if none exists or truncates existing lists
	InventoryList * addList(const std::string &name, u32 size);
	InventoryList * getList(const std::string &name);
	const InventoryList * getList(const std::string &name) const;
	const std::vector<InventoryList *> &getLists() const { return m_lists; }
	bool deleteList(const std::string &name);
	// A shorthand for adding items. Returns leftover item (possibly empty).
	ItemStack addItem(const std::string &listname, const ItemStack &newitem)
	{
		InventoryList *list = getList(listname);
		if(list == NULL)
			return newitem;
		return list->addItem(newitem);
	}

	inline bool checkModified() const
	{
		if (m_dirty)
			return true;

		for (const auto &list : m_lists)
			if (list->checkModified())
				return true;

		return false;
	}

	inline void setModified(bool dirty = true)
	{
		m_dirty = dirty;
		// Set all as handled
		if (!dirty) {
			for (const auto &list : m_lists)
				list->setModified(dirty);
		}
	}
private:
	// -1 if not found
	s32 getListIndex(const std::string &name) const;

	std::vector<InventoryList*> m_lists;
	IItemDefManager *m_itemdef;
	bool m_dirty = true;
};
