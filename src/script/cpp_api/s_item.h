// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "irr_v3d.h"
#include <optional>

struct PointedThing;
struct ItemStack;
class ServerActiveObject;
class LuaItemStack;
class ModApiItem;
class InventoryList;
struct InventoryLocation;

class ScriptApiItem
: virtual public ScriptApiBase
{
public:
	/*
	 * Functions with std::optional<ItemStack> are for callbacks where Lua may
	 * want to prevent the engine from modifying the inventory after it's done.
	 * This has a longer backstory where on_use may need to empty the player's
	 * inventory without the engine interfering (see issue #6546).
	 */

	bool item_OnDrop(ItemStack &item,
			ServerActiveObject *dropper, v3f pos);
	bool item_OnPlace(std::optional<ItemStack> &item,
			ServerActiveObject *placer, const PointedThing &pointed);
	bool item_OnUse(std::optional<ItemStack> &item,
			ServerActiveObject *user, const PointedThing &pointed);
	bool item_OnSecondaryUse(std::optional<ItemStack> &item,
			ServerActiveObject *user, const PointedThing &pointed);
	bool item_OnCraft(ItemStack &item, ServerActiveObject *user,
			const InventoryList *old_craft_grid, const InventoryLocation &craft_inv);
	bool item_CraftPredict(ItemStack &item, ServerActiveObject *user,
			const InventoryList *old_craft_grid, const InventoryLocation &craft_inv);

protected:
	friend class LuaItemStack;
	friend class ModApiItem;

	bool getItemCallback(const char *name, const char *callbackname, const v3s16 *p = nullptr);
	/*!
	 * Pushes a `pointed_thing` tabe to the stack.
	 * \param hitpoint If true, the exact pointing location is also pushed
	 */
	void pushPointedThing(const PointedThing &pointed, bool hitpoint = false);

};
