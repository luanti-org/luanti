// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#pragma once

#include <string>
#include <map>
#include <thread>
#include <unordered_map>
#include "wieldmesh.h" // ItemMesh
#include "util/basic_macros.h"

class Client;
struct ItemStack;
typedef std::vector<video::SColor> Palette; // copied from src/client/texturesource.h
namespace video { class ITexture; }

// Caches data needed to draw an itemstack

struct ItemVisualsManager
{
	ItemVisualsManager();
	~ItemVisualsManager();

	/// Clears the cached visuals
	void clear();

	// Get item inventory texture
	video::ITexture* getInventoryTexture(const ItemStack &item, Client *client) const;

	// Get item inventory overlay texture
	video::ITexture* getInventoryOverlayTexture(const ItemStack &item, Client *client) const;

	// Get item inventory animation
	// returns nullptr if it is not animated
	AnimationInfo *getInventoryAnimation(const ItemStack &item, Client *client) const;

	// Get item inventory overlay animation
	// returns nullptr if it is not animated
	AnimationInfo *getInventoryOverlayAnimation(const ItemStack &item, Client *client) const;

	// Get item mesh
	ItemMesh *getItemMesh(const ItemStack &item, Client *client) const;

	// Get item palette
	Palette* getPalette(const ItemStack &item, Client *client) const;

	// Returns the base color of an item stack: the color of all
	// tiles that do not define their own color.
	video::SColor getItemstackColor(const ItemStack &stack, Client *client) const;

private:
	struct ItemVisuals;

	// The id of the thread that is allowed to use irrlicht directly
	std::thread::id m_main_thread;
	// Cached textures and meshes
	mutable std::unordered_map<std::string, std::unique_ptr<ItemVisuals>> m_cached_item_visuals;

	ItemVisuals* createItemVisuals(const ItemStack &item, Client *client) const;
};
