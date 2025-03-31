// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#pragma once

#include <string>
#include <map>
#include <thread>
#include "wieldmesh.h" // ItemMesh
#include "util/basic_macros.h"

class Client;
class IItemDefManager;
class ItemStack;
typedef std::vector<video::SColor> Palette;

namespace irr {
	namespace video {
		class ITexture;
	}
}

// Caches data needed to draw an itemstack

struct ItemVisualsManager
{
	ItemVisualsManager(IItemDefManager *idef) : m_idef(idef)
	{
		m_main_thread = std::this_thread::get_id();
	}

	void clear() {
		m_cached_item_visuals.clear();
	}

	// Get item inventory texture
	video::ITexture* getInventoryTexture(const ItemStack &item, Client *client) const;

	// Get item wield mesh
	// Once said to return nullptr if there is an inventory image, but this is wrong
	ItemMesh* getWieldMesh(const ItemStack &item, Client *client) const;

	// Get item palette
	Palette* getPalette(const ItemStack &item, Client *client) const;

	// Returns the base color of an item stack: the color of all
	// tiles that do not define their own color.
	video::SColor getItemstackColor(const ItemStack &stack, Client *client) const;

private:
	struct ItemVisuals
	{
		video::ITexture *inventory_texture;
		ItemMesh wield_mesh;
		Palette *palette;

		ItemVisuals():
			inventory_texture(nullptr),
			palette(nullptr)
		{}

		~ItemVisuals();

		DISABLE_CLASS_COPY(ItemVisuals);
	};

	// The id of the thread that is allowed to use irrlicht directly
	std::thread::id m_main_thread;
	// Cached textures and meshes
	mutable std::unordered_map<std::string, std::unique_ptr<ItemVisuals>> m_cached_item_visuals;

	IItemDefManager *m_idef;

	ItemVisuals* createItemVisuals(const ItemStack &item, Client *client) const;
};
