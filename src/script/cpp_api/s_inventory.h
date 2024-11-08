// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"

struct MoveAction;
struct ItemStack;

class ScriptApiDetached
		: virtual public ScriptApiBase
{
public:
	/* Detached inventory callbacks */
	// Return number of accepted items to be moved
	int detached_inventory_AllowMove(
			const MoveAction &ma, int count,
			ServerActiveObject *player);
	// Return number of accepted items to be put
	int detached_inventory_AllowPut(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
	// Return number of accepted items to be taken
	int detached_inventory_AllowTake(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
	// Report moved items
	void detached_inventory_OnMove(
			const MoveAction &ma, int count,
			ServerActiveObject *player);
	// Report put items
	void detached_inventory_OnPut(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
	// Report taken items
	void detached_inventory_OnTake(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
private:
	bool getDetachedInventoryCallback(
			const std::string &name, const char *callbackname);
};
