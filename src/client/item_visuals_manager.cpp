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
	if (wield_mesh.mesh)
		wield_mesh.mesh->drop();
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
	auto cc = std::make_unique<ItemVisuals>();

	cc->inventory_texture = NULL;
	if (!inventory_image.empty())
		cc->inventory_texture = tsrc->getTexture(inventory_image);
	getItemMesh(client, item, &(cc->wield_mesh));

	cc->palette = tsrc->getPalette(def.palette_image);

	// Put in cache
	ItemVisuals *ptr = cc.get();
	m_cached_item_visuals[cache_key] = std::move(cc);
	return ptr;
}

video::ITexture* ItemVisualsManager::getInventoryTexture(const ItemStack &item,
		Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;
	return iv->inventory_texture;
}

ItemMesh* ItemVisualsManager::getWieldMesh(const ItemStack &item, Client *client) const
{
	ItemVisuals *iv = createItemVisuals(item, client);
	if (!iv)
		return nullptr;
	return &(iv->wield_mesh);
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

