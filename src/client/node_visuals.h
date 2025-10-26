// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#pragma once

#include <unordered_set>
#include "nodedef.h" // CF_SPECIAL_COUNT
#include "tile.h"

class Client;
struct PreLoadedTextures;
namespace scene
{
	class IMeshManipulator;
	struct SMesh;
}

// Stores client only data needed to draw nodes, like textures and meshes
// Contained in ContentFeatures

struct NodeVisuals
{
	// 0     1     2     3     4     5
	// up    down  right left  back  front
	TileSpec tiles[6];
	// Special tiles
	TileSpec special_tiles[CF_SPECIAL_COUNT];
	u8 solidness = 2; // Used when choosing which face is drawn
	u8 visual_solidness = 0; // When solidness=0, this tells how it looks like
	bool backface_culling = true;
	scene::SMesh *mesh_ptr = nullptr; // mesh in case of mesh node
	video::SColor minimap_color = video::SColor(0, 0, 0, 0);
	std::vector<video::SColor> *palette = nullptr;

	// alpha stays in ContentFeatures due to compatibility code that is necessary,
	// because it was part of the node definition table in the past.

	ContentFeatures *f;

	NodeVisuals(ContentFeatures *features) : f{features} {}
	~NodeVisuals();

	// Get color from palette or content features
	void getColor(u8 param2, video::SColor *color) const;

private:
	// Functions needed for initialisation
	void preUpdateTextures(ITextureSource *tsrc,
			std::unordered_set<std::string> &pool, const TextureSettings &tsettings);
	// May override the alpha and drawtype of the content features
	void updateTextures(ITextureSource *tsrc, IShaderSource *shdsrc, Client *client,
			PreLoadedTextures *texture_pool, const TextureSettings &tsettings);
	void updateMesh(Client *client, const TextureSettings &tsettings);
	void collectMaterials(std::vector<u32> &leaves_materials);

	friend void fillNodeVisuals(NodeDefManager *ndef, Client *client,
			void *progress_cbk_args);

	DISABLE_CLASS_COPY(NodeVisuals);
};

/**
 * @brief get fitting material type for an alpha mode
 */
static inline MaterialType alpha_mode_to_material_type(AlphaMode mode)
{
	switch (mode) {
	case ALPHAMODE_BLEND:
		return TILE_MATERIAL_ALPHA;
	case ALPHAMODE_OPAQUE:
		return TILE_MATERIAL_OPAQUE;
	case ALPHAMODE_CLIP:
	default:
		return TILE_MATERIAL_BASIC;
	}
}

/*!
 * Loads textures and shaders required for rendering nodes, into the NodeDefManager.
 * Must be cleared with clearNodeVisuals.
 * @param the NodeDefManager.
 * @param the Client.
 * @param progress_cbk called each time a node is loaded. Arguments:
 * `progress_callback_args`, number of loaded ContentFeatures, number of
 * total ContentFeatures.
 * @param progress_callback_args passed to the callback function
 */
void fillNodeVisuals(NodeDefManager *ndef, Client *client, void *progress_callback_args);
