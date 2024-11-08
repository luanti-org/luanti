// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2020 Minetest core development team

#include "serverinventorymgr.h"
#include "map.h"
#include "nodemetadata.h"
#include "player_sao.h"
#include "remoteplayer.h"
#include "server.h"
#include "serverenvironment.h"

ServerInventoryManager::ServerInventoryManager() : InventoryManager()
{
}

Inventory *ServerInventoryManager::getInventory(const InventoryLocation &loc)
{
	// No m_env check here: allow creation and modification of detached inventories

	switch (loc.type) {
	case InventoryLocation::UNDEFINED:
	case InventoryLocation::CURRENT_PLAYER:
		break;
	case InventoryLocation::PLAYER: {
		if (!m_env)
			return nullptr;

		RemotePlayer *player = m_env->getPlayer(loc.name.c_str());
		if (!player)
			return nullptr;

		PlayerSAO *playersao = player->getPlayerSAO();
		return playersao ? playersao->getInventory() : nullptr;
	} break;
	case InventoryLocation::NODEMETA: {
		if (!m_env)
			return nullptr;

		NodeMetadata *meta = m_env->getMap().getNodeMetadata(loc.p);
		return meta ? meta->getInventory() : nullptr;
	} break;
	case InventoryLocation::DETACHED: {
		auto it = m_detached_inventories.find(loc.name);
		if (it == m_detached_inventories.end())
			return nullptr;
		return it->second.inventory.get();
	} break;
	default:
		sanity_check(false); // abort
		break;
	}
	return nullptr;
}

void ServerInventoryManager::setInventoryModified(const InventoryLocation &loc)
{
	switch (loc.type) {
	case InventoryLocation::UNDEFINED:
		break;
	case InventoryLocation::PLAYER: {

		RemotePlayer *player = m_env->getPlayer(loc.name.c_str());

		if (!player)
			return;

		player->setModified(true);
		player->inventory.setModified(true);
		// Updates are sent in ServerEnvironment::step()
	} break;
	case InventoryLocation::NODEMETA: {
		MapEditEvent event;
		event.type = MEET_BLOCK_NODE_METADATA_CHANGED;
		event.setPositionModified(loc.p);
		m_env->getMap().dispatchEvent(event);
	} break;
	case InventoryLocation::DETACHED: {
		// Updates are sent in ServerEnvironment::step()
	} break;
	default:
		sanity_check(false); // abort
		break;
	}
}

Inventory *ServerInventoryManager::createDetachedInventory(
		const std::string &name, IItemDefManager *idef, const std::string &player)
{
	if (m_detached_inventories.count(name) > 0) {
		infostream << "Server clearing detached inventory \"" << name << "\""
			   << std::endl;
		m_detached_inventories[name].inventory.reset();
	} else {
		infostream << "Server creating detached inventory \"" << name << "\""
			   << std::endl;
	}

	auto inv_u = std::make_unique<Inventory>(idef);
	auto inv = inv_u.get();
	sanity_check(inv);
	m_detached_inventories[name].inventory = std::move(inv_u);
	if (!player.empty()) {
		m_detached_inventories[name].owner = player;

		if (!m_env)
			return inv; // Mods are not loaded yet, ignore

		RemotePlayer *p = m_env->getPlayer(name.c_str());

		// if player is connected, send him the inventory
		if (p) {
			m_env->getGameDef()->sendDetachedInventory(
					inv, name, p->getPeerId());
		}
	} else {
		if (!m_env)
			return inv; // Mods are not loaded yet, don't send

		// Inventory is for everybody, broadcast
		m_env->getGameDef()->sendDetachedInventory(inv, name, PEER_ID_INEXISTENT);
	}

	return inv;
}

bool ServerInventoryManager::removeDetachedInventory(const std::string &name)
{
	const auto &inv_it = m_detached_inventories.find(name);
	if (inv_it == m_detached_inventories.end())
		return false;

	inv_it->second.inventory.reset();
	const std::string &owner = inv_it->second.owner;

	if (!owner.empty()) {
		if (m_env) {
			RemotePlayer *player = m_env->getPlayer(owner.c_str());

			if (player)
				m_env->getGameDef()->sendDetachedInventory(
						nullptr, name, player->getPeerId());
		}
	} else if (m_env) {
		// Notify all players about the change as soon ServerEnv exists
		m_env->getGameDef()->sendDetachedInventory(
				nullptr, name, PEER_ID_INEXISTENT);
	}

	m_detached_inventories.erase(inv_it);

	return true;
}

bool ServerInventoryManager::checkDetachedInventoryAccess(
		const InventoryLocation &loc, const std::string &player) const
{
	SANITY_CHECK(loc.type == InventoryLocation::DETACHED);

	const auto &inv_it = m_detached_inventories.find(loc.name);
	if (inv_it == m_detached_inventories.end())
		return false;

	return inv_it->second.owner.empty() || inv_it->second.owner == player;
}

void ServerInventoryManager::sendDetachedInventories(const std::string &peer_name,
		bool incremental,
		std::function<void(const std::string &, Inventory *)> apply_cb)
{
	for (const auto &detached_inventory : m_detached_inventories) {
		const DetachedInventory &dinv = detached_inventory.second;
		if (incremental) {
			if (!dinv.inventory || !dinv.inventory->checkModified())
				continue;
		}

		// if we are pushing inventories to a specific player
		// we should filter to send only the right inventories
		if (!peer_name.empty()) {
			const std::string &attached_player = dinv.owner;
			if (!attached_player.empty() && peer_name != attached_player)
				continue;
		}

		apply_cb(detached_inventory.first, detached_inventory.second.inventory.get());
	}
}
