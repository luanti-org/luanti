// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#pragma once

#include <array>
#include <unordered_set>
#include "nodedef.h"
#include "tile.h"

class Client;
struct PreLoadedTextures;
namespace scene
{
	class IMeshManipulator;
	struct SMesh;
}

// Used when choosing which face is drawn
constexpr std::array<u8, NodeDrawType_END> NDT_solidness = [] {
	std::array<u8, NodeDrawType_END> solidness{};
	solidness[NDT_NORMAL] = 2;
	solidness[NDT_LIQUID] = 1;
	solidness[NDT_PLANTLIKE_ROOTED] = 2;
	return solidness;
}();

// When solidness=0, this tells how it looks like
constexpr std::array<u8, NodeDrawType_END> NDT_visual_solidness = [] {
	std::array<u8, NodeDrawType_END> visual_solidness{};
	visual_solidness[NDT_GLASSLIKE] = 1;
	visual_solidness[NDT_ALLFACES] = 1;
	visual_solidness[NDT_ALLFACES_OPTIONAL] = 1;
	visual_solidness[NDT_GLASSLIKE_FRAMED] = 1;
	visual_solidness[NDT_GLASSLIKE_FRAMED_OPTIONAL] = 1;
	return visual_solidness;
}();


// Stores client only data needed to draw nodes, like textures and meshes
// Contained in ContentFeatures

struct NodeVisuals
{
	// 0     1     2     3     4     5
	// up    down  right left  back  front
	TileSpec tiles[6];
	// Special tiles
	TileSpec special_tiles[CF_SPECIAL_COUNT];
	scene::SMesh *mesh_ptr = nullptr; // mesh in case of mesh node
	video::SColor minimap_color;
	std::vector<video::SColor> *palette = nullptr;

	// alpha stays in ContentFeatures due to compatibility code that is necessary,
	// because it was part of the node definition table in the past.

	NodeVisuals() = default;
	~NodeVisuals();

	// Get color from palette or content features
	video::SColor getColor(const ContentFeatures &f, u8 param2) const;

	/*!
	 * Creates NodeVisuals for every content feature in the passed NodeDefManager.
	 * @param ndef the NodeDefManager.
	 * @param client the Client.
	 * @param progress_cbk called each time a node is loaded. Arguments:
	 * `progress_cbk_args`, number of loaded ContentFeatures, number of
	 * total ContentFeatures.
	 * @param progress_cbk_args passed to the callback function
	 */
	static void fillNodeVisuals(NodeDefManager *ndef, Client *client,
			void *progress_callback_args);

	DISABLE_CLASS_COPY(NodeVisuals);

private:

	// Functions needed for initialisation
	void preUpdateTextures(const ContentFeatures &f, ITextureSource *tsrc,
			std::unordered_set<std::string> &pool, const TextureSettings &tsettings);

	// May override the alpha and drawtype of the content features
	void updateTextures(ContentFeatures &f, ITextureSource *tsrc,
			IShaderSource *shdsrc, Client *client, PreLoadedTextures *texture_pool,
			const TextureSettings &tsettings);

	void updateMesh(const std::string &mesh, float visual_scale, Client *client,
			const TextureSettings &tsettings);
	void collectMaterials(std::vector<u32> &leaves_materials);
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
