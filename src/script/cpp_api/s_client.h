// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "cpp_api/s_base.h"
#include "mapnode.h"
#include "util/string.h"
#include "util/pointedthing.h"

#ifdef _CRT_MSVCP_CURRENT
#include <cstdint>
#endif

class ClientEnvironment;
struct ItemStack;
class Inventory;
struct ItemDefinition;

class ScriptApiClient : virtual public ScriptApiBase
{
public:
	// Calls when mods are loaded
	void on_mods_loaded();

	// Calls on_shutdown handlers
	void on_shutdown();

	// Chat message handlers
	bool on_sending_message(const std::string &message);
	bool on_receiving_message(const std::string &message);

	void on_damage_taken(int32_t damage_amount);
	void on_hp_modification(int32_t newhp);
	void environment_step(float dtime);

	bool on_dignode(v3s16 p, MapNode node);
	bool on_punchnode(v3s16 p, MapNode node);
	bool on_placenode(const PointedThing &pointed, const ItemDefinition &item);
	bool on_item_use(const ItemStack &item, const PointedThing &pointed);

	bool on_inventory_open(Inventory *inventory);

	void setEnv(ClientEnvironment *env);
};
