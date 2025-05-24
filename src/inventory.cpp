// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "inventory.h"
#include "serialization.h"
#include "debug.h"
#include <algorithm>
#include <sstream>
#include "log.h"
#include "util/strfnd.h"
#include "content_mapnode.h" // For loading legacy MaterialItems
#include "nameidmapping.h" // For loading legacy MaterialItems
#include "util/serialize.h"
#include "util/string.h"

/*
	ItemStack
*/

static content_t content_translate_from_19_to_internal(content_t c_from)
{
	for (const auto &tt : trans_table_19) {
		if(tt[1] == c_from) {
			return tt[0];
		}
	}
	return c_from;
}

ItemStack::ItemStack(const std::string &name_, u16 count_,
		u16 wear_, IItemDefManager *itemdef) :
	name(itemdef->getAlias(name_)),
	count(count_),
	wear(wear_)
{
	if (name.empty() || count == 0)
		clear();
	else if (itemdef->get(name).type == ITEM_TOOL)
		count = 1;
}

void ItemStack::serialize(std::ostream &os, bool serialize_meta) const
{
	if (empty())
		return;

	// Check how many parts of the itemstring are needed
	int parts = 1;
	if (!metadata.empty())
		parts = 4;
	else if (wear != 0)
		parts = 3;
	else if (count != 1)
		parts = 2;

	os << serializeJsonStringIfNeeded(name);
	if (parts >= 2)
		os << " " << count;
	if (parts >= 3)
		os << " " << wear;
	if (parts >= 4) {
		os << " ";
		if (serialize_meta)
			metadata.serialize(os);
		else
			os << "<metadata size=" << metadata.size() << ">";
	}
}

void ItemStack::deSerialize(std::istream &is, IItemDefManager *itemdef)
{
	clear();

	// Read name
	name = deSerializeJsonStringIfNeeded(is);

	// Skip space
	std::string tmp;
	std::getline(is, tmp, ' ');
	if(!tmp.empty())
		throw SerializationError("Unexpected text after item name");

	if(name == "MaterialItem")
	{
		// Obsoleted on 2011-07-30

		u16 material;
		is>>material;
		u16 materialcount;
		is>>materialcount;
		// Convert old materials
		if(material <= 0xff)
			material = content_translate_from_19_to_internal(material);
		if(material > 0xfff)
			throw SerializationError("Too large material number");
		// Convert old id to name
		NameIdMapping legacy_nimap;
		content_mapnode_get_name_id_mapping(&legacy_nimap);
		legacy_nimap.getName(material, name);
		if(name.empty())
			name = "unknown_block";
		if (itemdef)
			name = itemdef->getAlias(name);
		count = materialcount;
	}
	else if(name == "MaterialItem2")
	{
		// Obsoleted on 2011-11-16

		u16 material;
		is>>material;
		u16 materialcount;
		is>>materialcount;
		if(material > 0xfff)
			throw SerializationError("Too large material number");
		// Convert old id to name
		NameIdMapping legacy_nimap;
		content_mapnode_get_name_id_mapping(&legacy_nimap);
		legacy_nimap.getName(material, name);
		if(name.empty())
			name = "unknown_block";
		if (itemdef)
			name = itemdef->getAlias(name);
		count = materialcount;
	}
	else if(name == "node" || name == "NodeItem" || name == "MaterialItem3"
			|| name == "craft" || name == "CraftItem")
	{
		// Obsoleted on 2012-01-07

		std::string all;
		std::getline(is, all, '\n');
		// First attempt to read inside ""
		Strfnd fnd(all);
		fnd.next("\"");
		// If didn't skip to end, we have ""s
		if(!fnd.at_end()){
			name = fnd.next("\"");
		} else { // No luck, just read a word then
			fnd.start(all);
			name = fnd.next(" ");
		}
		fnd.skip_over(" ");
		if (itemdef)
			name = itemdef->getAlias(name);
		count = stoi(trim(fnd.next("")));
		if(count == 0)
			count = 1;
	}
	else if(name == "MBOItem")
	{
		// Obsoleted on 2011-10-14
		throw SerializationError("MBOItem not supported anymore");
	}
	else if(name == "tool" || name == "ToolItem")
	{
		// Obsoleted on 2012-01-07

		std::string all;
		std::getline(is, all, '\n');
		// First attempt to read inside ""
		Strfnd fnd(all);
		fnd.next("\"");
		// If didn't skip to end, we have ""s
		if(!fnd.at_end()){
			name = fnd.next("\"");
		} else { // No luck, just read a word then
			fnd.start(all);
			name = fnd.next(" ");
		}
		count = 1;
		// Then read wear
		fnd.skip_over(" ");
		if (itemdef)
			name = itemdef->getAlias(name);
		wear = stoi(trim(fnd.next("")));
	}
	else
	{
		do  // This loop is just to allow "break;"
		{
			// The real thing

			// Apply item aliases
			if (itemdef)
				name = itemdef->getAlias(name);

			// Read the count
			std::string count_str;
			std::getline(is, count_str, ' ');
			if (count_str.empty()) {
				count = 1;
				break;
			}

			count = stoi(count_str);

			// Read the wear
			std::string wear_str;
			std::getline(is, wear_str, ' ');
			if(wear_str.empty())
				break;

			wear = stoi(wear_str);

			// Read metadata
			metadata.deSerialize(is);

			// In case fields are added after metadata, skip space here:
			//std::getline(is, tmp, ' ');
			//if(!tmp.empty())
			//	throw SerializationError("Unexpected text after metadata");

		} while(false);
	}

	if (name.empty() || count == 0)
		clear();
	else if (itemdef && itemdef->get(name).type == ITEM_TOOL)
		count = 1;
}

void ItemStack::deSerialize(const std::string &str, IItemDefManager *itemdef)
{
	std::istringstream is(str, std::ios::binary);
	deSerialize(is, itemdef);
}

std::string ItemStack::getItemString(bool include_meta) const
{
	std::ostringstream os(std::ios::binary);
	serialize(os, include_meta);
	return os.str();
}

std::string ItemStack::getDescription(const IItemDefManager *itemdef) const
{
	std::string desc = metadata.getString("description");
	if (desc.empty())
		desc = getDefinition(itemdef).description;
	return desc.empty() ? name : desc;
}

std::string ItemStack::getShortDescription(const IItemDefManager *itemdef) const
{
	std::string desc = metadata.getString("short_description");
	if (desc.empty())
		desc = getDefinition(itemdef).short_description;
	if (!desc.empty())
		return desc;
	// no short_description because of old server version or modified builtin
	// return first line of description
	std::stringstream sstr(getDescription(itemdef));
	std::getline(sstr, desc, '\n');
	return desc;
}

std::string ItemStack::getInventoryImage(const IItemDefManager *itemdef) const
{
	std::string texture = metadata.getString("inventory_image");
	if (texture.empty())
		texture = getDefinition(itemdef).inventory_image;

	return texture;
}

std::string ItemStack::getInventoryOverlay(const IItemDefManager *itemdef) const
{
	std::string texture = metadata.getString("inventory_overlay");
	if (texture.empty())
		texture = getDefinition(itemdef).inventory_overlay;

	return texture;
}

std::string ItemStack::getWieldImage(const IItemDefManager *itemdef) const
{
	std::string texture = metadata.getString("wield_image");
	if (texture.empty())
		texture = getDefinition(itemdef).wield_image;

	return texture;
}

std::string ItemStack::getWieldOverlay(const IItemDefManager *itemdef) const
{
	std::string texture = metadata.getString("wield_overlay");
	if (texture.empty())
		texture = getDefinition(itemdef).wield_overlay;

	return texture;
}

v3f ItemStack::getWieldScale(const IItemDefManager *itemdef) const
{
	std::string scale = metadata.getString("wield_scale");

	return str_to_v3f(scale).value_or(getDefinition(itemdef).wield_scale);
}

ItemStack ItemStack::addItem(ItemStack newitem, IItemDefManager *itemdef)
{
	// If the item is empty or the position invalid, bail out
	if(newitem.empty())
	{
		// nothing can be added trivially
	}
	// If this is an empty item, it's an easy job.
	else if(empty())
	{
		*this = newitem;
		newitem.clear();
	}
	// If item name or metadata differs, bail out
	else if (name != newitem.name
		|| metadata != newitem.metadata)
	{
		// cannot be added
	}
	// If the item fits fully, add counter and delete it
	else if(newitem.count <= freeSpace(itemdef))
	{
		add(newitem.count);
		newitem.clear();
	}
	// Else the item does not fit fully. Add all that fits and return
	// the rest.
	else
	{
		u16 freespace = freeSpace(itemdef);
		add(freespace);
		newitem.remove(freespace);
	}

	return newitem;
}

bool ItemStack::itemFits(ItemStack newitem,
		ItemStack *restitem,
		IItemDefManager *itemdef) const
{

	// If the item is empty or the position invalid, bail out
	if(newitem.empty())
	{
		// nothing can be added trivially
	}
	// If this is an empty item, it's an easy job.
	else if(empty())
	{
		newitem.clear();
	}
	// If item name or metadata differs, bail out
	else if (name != newitem.name
		|| metadata != newitem.metadata)
	{
		// cannot be added
	}
	// If the item fits fully, delete it
	else if(newitem.count <= freeSpace(itemdef))
	{
		newitem.clear();
	}
	// Else the item does not fit fully. Return the rest.
	else
	{
		u16 freespace = freeSpace(itemdef);
		newitem.remove(freespace);
	}

	if(restitem)
		*restitem = newitem;

	return newitem.empty();
}

bool ItemStack::stacksWith(const ItemStack &other) const
{
	return (this->name == other.name &&
			this->wear == other.wear &&
			this->metadata == other.metadata);
}

ItemStack ItemStack::takeItem(u32 takecount)
{
	if(takecount == 0 || count == 0)
		return ItemStack();

	ItemStack result = *this;
	if(takecount >= count)
	{
		// Take all
		clear();
	}
	else
	{
		// Take part
		remove(takecount);
		result.count = takecount;
	}
	return result;
}

ItemStack ItemStack::peekItem(u32 peekcount) const
{
	if(peekcount == 0 || count == 0)
		return ItemStack();

	ItemStack result = *this;
	if(peekcount < count)
		result.count = peekcount;
	return result;
}

/*
	Inventory
*/

InventoryList::InventoryList(const std::string &name, u32 size, IItemDefManager *itemdef):
	m_name(name),
	m_size(size),
	m_itemdef(itemdef)
{
	clearItems();
}

void InventoryList::clearItems()
{
	m_items.clear();

	for (u32 i=0; i < m_size; i++) {
		m_items.emplace_back();
	}

	setModified();
}

void InventoryList::setSize(u32 newsize)
{
	if (newsize == m_items.size())
		return;

	if (newsize < m_items.size())
		checkResizeLock();

	m_items.resize(newsize);
	m_size = newsize;
	setModified();
}

void InventoryList::setWidth(u32 newwidth)
{
	m_width = newwidth;
	setModified();
}

void InventoryList::setName(const std::string &name)
{
	m_name = name;
	setModified();
}

void InventoryList::serialize(std::ostream &os, bool incremental) const
{
	//os.imbue(std::locale("C"));

	os<<"Width "<<m_width<<"\n";

	for (const auto &item : m_items) {
		if (item.empty()) {
			os<<"Empty";
		} else {
			os<<"Item ";
			item.serialize(os);
		}
		// TODO: Implement this:
		// if (!incremental || item.checkModified())
		// os << "Keep";
		os<<"\n";
	}

	os<<"EndInventoryList\n";
}

void InventoryList::deSerialize(std::istream &is)
{
	//is.imbue(std::locale("C"));
	setModified();

	u32 item_i = 0;
	m_width = 0;

	while (is.good()) {
		std::string line;
		std::getline(is, line, '\n');

		std::istringstream iss(line);

		std::string name;
		std::getline(iss, name, ' ');

		if (name == "EndInventoryList" || name == "end") {
			// If partial incremental: Clear leftover items (should not happen!)
			for (size_t i = item_i; i < m_items.size(); ++i)
				m_items[i].clear();
			return;
		}

		if (name == "Width") {
			iss >> m_width;
			if (iss.fail())
				throw SerializationError("incorrect width property");
		}
		else if(name == "Item")
		{
			if(item_i > getSize() - 1)
				throw SerializationError("too many items");
			ItemStack item;
			item.deSerialize(iss, m_itemdef);
			m_items[item_i++] = item;
		}
		else if(name == "Empty")
		{
			if(item_i > getSize() - 1)
				throw SerializationError("too many items");
			m_items[item_i++].clear();
		} else if (name == "Keep") {
			++item_i; // Unmodified item
		}
	}

	// Contents given to deSerialize() were not terminated properly: throw error.

	std::ostringstream ss;
	ss << "Malformatted inventory list. list="
		<< m_name << ", read " << item_i << " of " << getSize()
		<< " ItemStacks." << std::endl;
	throw SerializationError(ss.str());
}

InventoryList & InventoryList::operator = (const InventoryList &other)
{
	checkResizeLock();

	m_items = other.m_items;
	m_size = other.m_size;
	m_width = other.m_width;
	m_name = other.m_name;
	m_itemdef = other.m_itemdef;
	//setDirty(true);

	return *this;
}

bool InventoryList::operator == (const InventoryList &other) const
{
	if(m_size != other.m_size)
		return false;
	if(m_width != other.m_width)
		return false;
	if(m_name != other.m_name)
		return false;
	for (u32 i = 0; i < m_items.size(); i++)
		if (m_items[i] != other.m_items[i])
			return false;

	return true;
}

u32 InventoryList::getUsedSlots() const
{
	u32 num = 0;
	for (const auto &m_item : m_items) {
		if (!m_item.empty())
			num++;
	}
	return num;
}

ItemStack InventoryList::changeItem(u32 i, const ItemStack &newitem)
{
	if(i >= m_items.size())
		return newitem;

	ItemStack olditem = m_items[i];
	if (olditem != newitem) {
		m_items[i] = newitem;
		setModified();
	}
	return olditem;
}

void InventoryList::deleteItem(u32 i)
{
	assert(i < m_items.size()); // Pre-condition
	m_items[i].clear();
	setModified();
}

ItemStack InventoryList::addItem(const ItemStack &newitem_)
{
	ItemStack newitem = newitem_;

	if(newitem.empty())
		return newitem;

	/*
		First try to find if it could be added to some existing items
	*/
	for(u32 i=0; i<m_items.size(); i++)
	{
		// Ignore empty slots
		if(m_items[i].empty())
			continue;
		// Try adding
		newitem = addItem(i, newitem);
		if(newitem.empty())
			return newitem; // All was eaten
	}

	/*
		Then try to add it to empty slots
	*/
	for(u32 i=0; i<m_items.size(); i++)
	{
		// Ignore unempty slots
		if(!m_items[i].empty())
			continue;
		// Try adding
		newitem = addItem(i, newitem);
		if(newitem.empty())
			return newitem; // All was eaten
	}

	// Return leftover
	return newitem;
}

ItemStack InventoryList::addItem(u32 i, const ItemStack &newitem)
{
	if(i >= m_items.size())
		return newitem;

	ItemStack leftover = m_items[i].addItem(newitem, m_itemdef);
	if (leftover != newitem)
		setModified();
	return leftover;
}

bool InventoryList::itemFits(const u32 i, const ItemStack &newitem,
		ItemStack *restitem) const
{
	if(i >= m_items.size())
	{
		if(restitem)
			*restitem = newitem;
		return false;
	}

	return m_items[i].itemFits(newitem, restitem, m_itemdef);
}

bool InventoryList::roomForItem(const ItemStack &item_) const
{
	ItemStack item = item_;
	ItemStack leftover;
	for(u32 i=0; i<m_items.size(); i++)
	{
		if(itemFits(i, item, &leftover))
			return true;
		item = leftover;
	}
	return false;
}

bool InventoryList::containsItem(const ItemStack &item, bool match_meta) const
{
	u32 count = item.count;
	if (count == 0)
		return true;

	for (auto i = m_items.rbegin(); i != m_items.rend(); ++i) {
		if (count == 0)
			break;
		if (i->name == item.name && (!match_meta || (i->metadata == item.metadata))) {
			if (i->count >= count)
				return true;

			count -= i->count;
		}
	}
	return false;
}

ItemStack InventoryList::removeItem(const ItemStack &item, bool match_meta)
{
	ItemStack removed;
	for (auto i = m_items.rbegin(); i != m_items.rend(); ++i) {
		if (i->name == item.name && (!match_meta || i->metadata == item.metadata)) {
			u32 still_to_remove = item.count - removed.count;
			ItemStack leftover = removed.addItem(i->takeItem(still_to_remove),
					m_itemdef);
			// Allow oversized stacks
			removed.count += leftover.count;

			if (removed.count == item.count)
				break;
		}
	}
	if (!removed.empty())
		setModified();
	return removed;
}

ItemStack InventoryList::takeItem(u32 i, u32 takecount)
{
	if(i >= m_items.size())
		return ItemStack();

	ItemStack taken = m_items[i].takeItem(takecount);
	if (!taken.empty())
		setModified();
	return taken;
}

void InventoryList::moveItemSomewhere(u32 i, InventoryList *dest, u32 count)
{
	// Take item from source list
	ItemStack item1;
	if (count == 0)
		item1 = changeItem(i, ItemStack());
	else
		item1 = takeItem(i, count);

	if (item1.empty())
		return;

	ItemStack leftover;
	leftover = dest->addItem(item1);

	if (!leftover.empty()) {
		// Add the remaining part back to the source item
		// do NOT use addItem to allow oversized stacks!
		leftover.add(getItem(i).count);
		changeItem(i, leftover);
	}
}

ItemStack InventoryList::moveItem(u32 i, InventoryList *dest, u32 dest_i,
		u32 count, bool swap_if_needed, bool *did_swap)
{
	ItemStack moved;
	if (this == dest && i == dest_i)
		return moved;

	// Take item from source list
	if (count == 0)
		moved = changeItem(i, ItemStack());
	else
		moved = takeItem(i, count);

	// Try to add the item to destination list
	ItemStack leftover = dest->addItem(dest_i, moved);

	// If something is returned, the item was not fully added
	if (!leftover.empty()) {
		// Keep track of how many we actually moved
		moved.remove(leftover.count);

		// Add any leftover stack back to the source stack.
		leftover.add(getItem(i).count); // leftover + source count
		changeItem(i, leftover); // do NOT use addItem to allow oversized stacks!
		leftover.clear();

		// Swap if no items could be moved
		if (moved.empty() && swap_if_needed) {
			// Tell that we swapped
			if (did_swap != NULL) {
				*did_swap = true;
			}
			// Take item from source list
			moved = changeItem(i, ItemStack());
			// Adding was not possible, swap the items.
			ItemStack item2 = dest->changeItem(dest_i, moved);
			// Put item from destination list to the source list
			changeItem(i, item2);
		}
	}

	return moved;
}

void InventoryList::checkResizeLock()
{
	if (m_resize_locks == 0)
		return; // OK

	throw BaseException("InventoryList '" + m_name
			+ "' is currently in use and cannot be deleted or resized.");
}

/*
	Inventory
*/

Inventory::~Inventory()
{
	clear();
}

void Inventory::clear()
{
	for (auto &m_list : m_lists) {
		// Placing this check within the destructor would be a logical solution
		// but that's generally a bad idea, thus manual calls beforehand:
		m_list->checkResizeLock();
	}

	for (auto &m_list : m_lists) {
		delete m_list;
	}
	m_lists.clear();
	setModified();
}

Inventory::Inventory(IItemDefManager *itemdef)
{
	m_itemdef = itemdef;
	setModified();
}

Inventory::Inventory(const Inventory &other)
{
	*this = other;
}

Inventory & Inventory::operator = (const Inventory &other)
{
	// Gracefully handle self assignment
	if(this != &other)
	{
		clear();
		m_itemdef = other.m_itemdef;
		for (InventoryList *list : other.m_lists) {
			m_lists.push_back(new InventoryList(*list));
		}
		setModified();
	}
	return *this;
}

bool Inventory::operator == (const Inventory &other) const
{
	if(m_lists.size() != other.m_lists.size())
		return false;

	for(u32 i=0; i<m_lists.size(); i++)
	{
		if(*m_lists[i] != *other.m_lists[i])
			return false;
	}
	return true;
}

void Inventory::serialize(std::ostream &os, bool incremental) const
{
	//std::cout << "Serialize " << (int)incremental << ", n=" << m_lists.size() << std::endl;
	for (const InventoryList *list : m_lists) {
		if (!incremental || list->checkModified()) {
			os << "List " << list->getName() << " " << list->getSize() << "\n";
			list->serialize(os, incremental);
		} else {
			os << "KeepList " << list->getName() << "\n";
		}
	}

	os<<"EndInventory\n";
}

void Inventory::deSerialize(std::istream &is)
{
	std::vector<InventoryList *> new_lists;
	new_lists.reserve(m_lists.size());

	while (is.good()) {
		std::string line;
		std::getline(is, line, '\n');

		std::istringstream iss(line);

		std::string name;
		std::getline(iss, name, ' ');

		if (name == "EndInventory" || name == "end") {
			// Remove all lists that were not sent
			for (auto &list : m_lists) {
				if (std::find(new_lists.begin(), new_lists.end(), list) != new_lists.end())
					continue;

				delete list;
				list = nullptr;
				setModified();
			}
			m_lists.erase(std::remove(m_lists.begin(), m_lists.end(),
					nullptr), m_lists.end());
			return;
		}

		if (name == "List") {
			std::string listname;
			u32 listsize;

			std::getline(iss, listname, ' ');
			iss>>listsize;

			InventoryList *list = getList(listname);
			bool create_new = !list;
			if (create_new)
				list = new InventoryList(listname, listsize, m_itemdef);
			else
				list->setSize(listsize);
			list->deSerialize(is);

			new_lists.push_back(list);
			if (create_new)
				m_lists.push_back(list);

		} else if (name == "KeepList") {
			// Incrementally sent list
			std::string listname;
			std::getline(iss, listname, ' ');

			InventoryList *list = getList(listname);
			if (list) {
				new_lists.push_back(list);
			} else {
				errorstream << "Inventory::deSerialize(): Tried to keep list '" <<
					listname << "' which is non-existent." << std::endl;
			}
		}
		// Any additional fields will throw errors when received by a client
		// older than PROTOCOL_VERSION 38
	}

	// Contents given to deSerialize() were not terminated properly: throw error.

	std::ostringstream ss;
	ss << "Malformatted inventory (damaged?). "
		<< m_lists.size() << " lists read." << std::endl;
	throw SerializationError(ss.str());
}

InventoryList * Inventory::addList(const std::string &name, u32 size)
{
	setModified();

	// Reset existing lists instead of re-creating if possible.
	// InventoryAction::apply() largely caches InventoryList pointers which must not be
	// invalidated by Lua API calls (e.g. InvRef:set_list), hence do resize & clear which
	// also include the neccessary resize lock checks.
	s32 i = getListIndex(name);
	if (i != -1) {
		InventoryList *list = m_lists[i];
		list->setSize(size);
		list->clearItems();
		return list;
	}

	//don't create list with invalid name
	if (name.find(' ') != std::string::npos)
		return nullptr;

	InventoryList *list = new InventoryList(name, size, m_itemdef);
	list->setModified();
	m_lists.push_back(list);
	return list;
}

InventoryList * Inventory::getList(const std::string &name)
{
	s32 i = getListIndex(name);
	if(i == -1)
		return nullptr;
	return m_lists[i];
}

bool Inventory::deleteList(const std::string &name)
{
	s32 i = getListIndex(name);
	if(i == -1)
		return false;

	m_lists[i]->checkResizeLock();

	setModified();
	delete m_lists[i];
	m_lists.erase(m_lists.begin() + i);
	return true;
}

const InventoryList *Inventory::getList(const std::string &name) const
{
	s32 i = getListIndex(name);
	if(i == -1)
		return nullptr;
	return m_lists[i];
}

s32 Inventory::getListIndex(const std::string &name) const
{
	for(u32 i=0; i<m_lists.size(); i++)
	{
		if(m_lists[i]->getName() == name)
			return i;
	}
	return -1;
}

//END
