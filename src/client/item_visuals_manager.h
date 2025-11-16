// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#pragma once

#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct AnimationInfo;
class Client;
struct ItemStack;
struct ItemMesh;
namespace video { class ITexture; class SColor; }
typedef std::vector<video::SColor> Palette; // copied from src/client/texturesource.h

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
