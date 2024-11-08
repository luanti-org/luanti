// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "cpp_api/s_item.h"
#include "irr_v3d.h"

struct MoveAction;
struct ItemStack;

class ScriptApiNodemeta
		: virtual public ScriptApiBase,
		  public ScriptApiItem
{
public:
	ScriptApiNodemeta() = default;
	virtual ~ScriptApiNodemeta() = default;

	// Return number of accepted items to be moved
	int nodemeta_inventory_AllowMove(
			const MoveAction &ma, int count,
			ServerActiveObject *player);
	// Return number of accepted items to be put
	int nodemeta_inventory_AllowPut(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
	// Return number of accepted items to be taken
	int nodemeta_inventory_AllowTake(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
	// Report moved items
	void nodemeta_inventory_OnMove(
			const MoveAction &ma, int count,
			ServerActiveObject *player);
	// Report put items
	void nodemeta_inventory_OnPut(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
	// Report taken items
	void nodemeta_inventory_OnTake(
			const MoveAction &ma, const ItemStack &stack,
			ServerActiveObject *player);
private:

};
