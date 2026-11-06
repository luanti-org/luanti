// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2020 Luanti core development team

#pragma once

#include "inventorymanager.h"
#include <cassert>
#include <functional>
#include <memory>
#include <unordered_map>

class IItemDefManager;
class ServerEnvironment;

class ServerInventoryManager : public InventoryManager
{
public:
	ServerInventoryManager();
	virtual ~ServerInventoryManager() = default;

	void setEnv(ServerEnvironment *env)
	{
		assert(!m_env);
		m_env = env;
	}

	// virtual: Overwritten by MockInventoryManager for the unittests
	virtual Inventory *getInventory(const InventoryLocation &loc);
	virtual void setInventoryModified(const InventoryLocation &loc);

	// Creates or resets inventory
	Inventory *createDetachedInventory(const std::string &name, const IItemDefManager *idef,
			const std::string &player = "");
	bool removeDetachedInventory(const std::string &name);
	bool checkDetachedInventoryAccess(const InventoryLocation &loc, const std::string &player) const;

	// args: name, inventory, owner
	using SendCallbackFn = std::function<void(const std::string&, Inventory*, const std::string &)>;
	void sendDetachedInventories(const std::string &filter_owner, bool incremental,
		const SendCallbackFn &apply_cb);

protected:
	struct DetachedInventory
	{
		std::unique_ptr<Inventory> inventory;
		std::string owner;
	};

	ServerEnvironment *m_env = nullptr;

	std::unordered_map<std::string, DetachedInventory> m_detached_inventories;
};
