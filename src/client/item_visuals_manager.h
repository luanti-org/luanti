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
class ItemStack;
typedef std::vector<video::SColor> Palette; // copied from src/client/texturesource.h
namespace irr::video { class ITexture; }

// Caches data needed to draw an itemstack

struct ItemVisualsManager
{
	ItemVisualsManager()
	{
		m_main_thread = std::this_thread::get_id();
	}

	void clear() {
		m_cached_item_visuals.clear();
	}

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

	// Get item item mesh
	// Once said to return nullptr if there is an inventory image, but this is wrong
	ItemMesh *getItemMesh(const ItemStack &item, Client *client) const;

	// Get item palette
	Palette* getPalette(const ItemStack &item, Client *client) const;

	// Returns the base color of an item stack: the color of all
	// tiles that do not define their own color.
	video::SColor getItemstackColor(const ItemStack &stack, Client *client) const;

private:
	struct ItemVisuals
	{
		video::ITexture *inventory_texture;
		video::ITexture *inventory_overlay_texture;
		ItemMesh item_mesh;
		Palette *palette;

		// Null if non animated
		// ItemVisuals owns the frames vector of the inventory and overlay image,
		// and stores an AnimationInfo to draw animated non-mesh items,
		using OwnedAnimationInfo = std::unique_ptr<std::pair<AnimationInfo,
				std::vector<FrameSpec>>>;
		OwnedAnimationInfo inventory_animation;
		OwnedAnimationInfo inventory_overlay_animation;

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

	ItemVisuals* createItemVisuals(const ItemStack &item, Client *client) const;
};
