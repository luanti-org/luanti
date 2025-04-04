// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#include "item_visuals_manager.h"

#include "mesh.h"
#include "client.h"
#include "texturesource.h"
#include "itemdef.h"
#include "inventory.h"

struct ItemVisualsManager::ItemVisuals
{
	ItemMesh item_mesh;
	Palette *palette;

	AnimationInfo inventory_normal;
	AnimationInfo inventory_overlay;

	ItemVisuals() :
		palette(nullptr)
	{}

	~ItemVisuals()
	{
		inventory_normal.freeFrames();
		inventory_overlay.freeFrames();
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

	// Create new ItemVisuals
	auto iv = std::make_unique<ItemVisuals>();

	auto populate_texture_and_animation = [tsrc](
			const ItemImageDef &image,
			AnimationInfo &animation)
	{
		int frame_length_ms = 0;
		auto frames = std::make_unique<std::vector<FrameSpec>>();
		if (image.name.empty()) {
			// no-op
		} else if (image.animation.type == TileAnimationType::TAT_NONE) {
			frames->push_back({0, tsrc->getTexture(image.name)});
		} else {
			// Animated
			// Get inventory texture frames
			*frames = createAnimationFrames(tsrc, image.name, image.animation, frame_length_ms);
		}
		animation = AnimationInfo(frames.release(), frame_length_ms);
		// `frames` are freed in `ItemVisuals::~ItemVisuals`
	};

	populate_texture_and_animation(inventory_image, iv->inventory_normal);
	populate_texture_and_animation(inventory_overlay, iv->inventory_overlay);

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

