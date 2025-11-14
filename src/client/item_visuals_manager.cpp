// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#include "item_visuals_manager.h"

#include "wieldmesh.h"
#include "client.h"
#include "texturesource.h"
#include "itemdef.h"
#include "inventory.h"
#include <IMesh.h>

struct ItemVisualsManager::ItemVisuals
{
	ItemMesh item_mesh;
	Palette *palette;

	AnimationInfo inventory_normal;
	AnimationInfo inventory_overlay;

	// ItemVisuals owns the frames and AnimationInfo points to them
	std::vector<FrameSpec> frames_normal;
	std::vector<FrameSpec> frames_overlay;

	ItemVisuals() :
		palette(nullptr)
	{}

	~ItemVisuals()
	{
		if (item_mesh.mesh)
			item_mesh.mesh->drop();
	}

	DISABLE_CLASS_COPY(ItemVisuals);
};

ItemVisualsManager::ItemVisuals *ItemVisualsManager::createItemVisuals( const ItemStack &item,
		Client *client) const
{
	// This is not thread-safe
	sanity_check(std::this_thread::get_id() == m_main_thread);

	IItemDefManager *idef = client->idef();

	const ItemDefinition &def = item.getDefinition(idef);
	ItemImageDef inventory_image = item.getInventoryImage(idef);
	ItemImageDef inventory_overlay = item.getInventoryOverlay(idef);

	// Key only consists of item name + image name,
	// because animation currently cannot be overridden by meta
	std::ostringstream os(def.name);
	if (!inventory_image.name.empty())
		os << "/" << inventory_image.name;
	if (!inventory_overlay.name.empty())
		os << ":" << inventory_overlay.name;
	std::string cache_key = os.str();


	// Skip if already in cache
	auto it = m_cached_item_visuals.find(cache_key);
	if (it != m_cached_item_visuals.end())
		return it->second.get();

	infostream << "Lazily creating item texture and mesh for \""
			<< cache_key << "\"" << std::endl;

	ITextureSource *tsrc = client->getTextureSource();

	auto iv = std::make_unique<ItemVisuals>();

	// Create inventory image textures
	int frame_length = 0;
	iv->frames_normal = createAnimationFrames(tsrc, inventory_image.name,
			inventory_image.animation, frame_length);
	iv->inventory_normal = AnimationInfo(&iv->frames_normal, frame_length);

	// Create inventory overlay textures
	iv->frames_overlay = createAnimationFrames(tsrc, inventory_overlay.name,
			inventory_overlay.animation, frame_length);
	iv->inventory_overlay = AnimationInfo(&iv->frames_overlay, frame_length);

	createItemMesh(client, def,
			iv->inventory_normal,
			iv->inventory_overlay,
			&(iv->item_mesh));

	iv->palette = tsrc->getPalette(def.palette_image);

	// Put in cache
	ItemVisuals *ptr = iv.get();
	m_cached_item_visuals[cache_key] = std::move(iv);
	return ptr;
}

// Needed because `ItemVisuals` is not known in the header.
ItemVisualsManager::ItemVisualsManager()
{
	m_main_thread = std::this_thread::get_id();
}

ItemVisualsManager::~ItemVisualsManager()
{
}

void ItemVisualsManager::clear()
{
	m_cached_item_visuals.clear();
}


video::ITexture *ItemVisualsManager::getInventoryTexture(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;

	// Texture animation update (if >1 frame)
	return iv->inventory_normal.getTexture(client->getAnimationTime());
}

video::ITexture *ItemVisualsManager::getInventoryOverlayTexture(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;

	// Texture animation update (if >1 frame)
	return iv->inventory_overlay.getTexture(client->getAnimationTime());
}

ItemMesh *ItemVisualsManager::getItemMesh(const ItemStack &item, Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	return iv ? &(iv->item_mesh) : nullptr;
}

AnimationInfo *ItemVisualsManager::getInventoryAnimation(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv || iv->inventory_normal.getFrameCount() <= 1)
		return nullptr;
	return &iv->inventory_normal;
}

// Get item inventory overlay animation
// returns nullptr if it is not animated
AnimationInfo *ItemVisualsManager::getInventoryOverlayAnimation(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv || iv->inventory_overlay.getFrameCount() <= 1)
		return nullptr;
	return &iv->inventory_overlay;
}

Palette* ItemVisualsManager::getPalette(const ItemStack &item, Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;
	return iv->palette;
}

video::SColor ItemVisualsManager::getItemstackColor(const ItemStack &stack,
	Client *client) const
{
	// Look for direct color definition
	const std::string &colorstring = stack.metadata.getString("color", 0);
	video::SColor directcolor;
	if (!colorstring.empty() && parseColorString(colorstring, directcolor, true))
		return directcolor;
	// See if there is a palette
	Palette *palette = getPalette(stack, client);
	const std::string &index = stack.metadata.getString("palette_index", 0);
	if (palette && !index.empty())
		return (*palette)[mystoi(index, 0, 255)];
	// Fallback color
	return client->idef()->get(stack.name).color;
}

