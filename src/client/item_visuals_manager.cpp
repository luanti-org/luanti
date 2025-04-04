// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#include "item_visuals_manager.h"

#include "mesh.h"
#include "client.h"
#include "texturesource.h"
#include "itemdef.h"
#include "inventory.h"

ItemVisualsManager::ItemVisuals::~ItemVisuals() {
	if (item_mesh.mesh)
		item_mesh.mesh->drop();
}

ItemVisualsManager::ItemVisuals *ItemVisualsManager::createItemVisuals( const ItemStack &item,
		Client *client) const
{
	// This is not thread-safe
	sanity_check(std::this_thread::get_id() == m_main_thread);

	IItemDefManager *idef = client->idef();

	const ItemDefinition &def = item.getDefinition(idef);
	std::string inventory_image = item.getInventoryImage(idef);
	std::string inventory_overlay = item.getInventoryOverlay(idef);
	std::string cache_key = def.name;
	if (!inventory_image.empty())
		cache_key += "/" + inventory_image;
	if (!inventory_overlay.empty())
		cache_key += ":" + inventory_overlay;

	// Skip if already in cache
	auto it = m_cached_item_visuals.find(cache_key);
	if (it != m_cached_item_visuals.end())
		return it->second.get();

	infostream << "Lazily creating item texture and mesh for \""
			<< cache_key << "\"" << std::endl;

	ITextureSource *tsrc = client->getTextureSource();

	// Create new ItemVisuals
	auto iv = std::make_unique<ItemVisuals>();

	auto populate_texture_and_animation = [&](
			const std::string &image_name, const TileAnimationParams &animation,
			video::ITexture *&texture, ItemVisuals::OwnedAnimationInfo &owned_animation)
	{
		texture = nullptr;
		if (!image_name.empty()) {
			texture = tsrc->getTexture(image_name);

			// Get inventory texture frames
			if (animation.type != TileAnimationType::TAT_NONE && texture) {
				int frame_length_ms;
				owned_animation = std::make_unique<std::pair<AnimationInfo,
						std::vector<FrameSpec>>>(AnimationInfo(),
						createAnimationFrames(tsrc, image_name, animation, frame_length_ms));
				owned_animation->first = AnimationInfo(&(owned_animation->second),
						frame_length_ms);

				// Use first frame for static texture
				texture = owned_animation->second[0].texture;
			}
		}
	};

	populate_texture_and_animation(inventory_image, def.inventory_image_animation,
			iv->inventory_texture, iv->inventory_animation);

	populate_texture_and_animation(inventory_overlay, def.inventory_image_animation,
			iv->inventory_overlay_texture, iv->inventory_overlay_animation);

	createItemMesh(client, def, iv->inventory_texture,
			iv->inventory_animation ? &(iv->inventory_animation->first) : nullptr,
			iv->inventory_overlay_texture,
			iv->inventory_overlay_animation ? &(iv->inventory_overlay_animation->first) :
				nullptr,
			&(iv->item_mesh));

	iv->palette = tsrc->getPalette(def.palette_image);

	// Put in cache
	ItemVisuals *ptr = iv.get();
	m_cached_item_visuals[cache_key] = std::move(iv);
	return ptr;
}

video::ITexture* ItemVisualsManager::getInventoryTexture(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;

	if (iv->inventory_animation) {
		// Texture animation update
		video::ITexture *texture = iv->inventory_animation->first.getTexture(
				client->getAnimationTime());
		if (texture) {
			iv->inventory_texture = texture;
		}
	}

	return iv->inventory_texture;
}

video::ITexture* ItemVisualsManager::getInventoryOverlayTexture(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;

	if (iv->inventory_overlay_animation) {
		// Texture animation update
		video::ITexture *texture = iv->inventory_overlay_animation->first.getTexture(
				client->getAnimationTime());
		if (texture) {
			iv->inventory_overlay_texture = texture;
		}
	}

	return iv->inventory_overlay_texture;
}

ItemMesh* ItemVisualsManager::getItemMesh(const ItemStack &item, Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;
	return &(iv->item_mesh);
}

AnimationInfo *ItemVisualsManager::getInventoryAnimation(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv || !iv->inventory_animation)
		return nullptr;
	return &(iv->inventory_animation->first);
}

// Get item inventory overlay animation
// returns nullptr if it is not animated
AnimationInfo *ItemVisualsManager::getInventoryOverlayAnimation(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv || !iv->inventory_overlay_animation)
		return nullptr;
	return &(iv->inventory_overlay_animation->first);
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

