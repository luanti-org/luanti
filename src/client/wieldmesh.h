// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <vector>
#include "irr_aabb3d.h"
#include "irr_v3d.h"
#include <EMaterialTypes.h>
#include <IMeshSceneNode.h>
#include <SColor.h>
#include <memory>
#include "tile.h"
#include "nodedef.h"

namespace scene
{
	class ISceneManager;
	class IMesh;
	struct SMesh;
}


struct ItemStack;
struct TileDef;
class Client;
class ITextureSource;
struct ItemDefinition;
struct TileAnimationParams;
class IShaderSource;
class ShadowRenderer;

/*
 * Holds information of an item mesh's buffer.
 * Used for coloring and animation.
 */
class ItemMeshBufferInfo
{
	/*
	 * Optional color that overrides the global base color.
	 */
	video::SColor override_color;
	/*
	 * Stores the last color this mesh buffer was colorized as.
	 */
	video::SColor last_colorized;

	// saves some bytes compared to two std::optionals
	bool override_color_set = false;
	bool last_colorized_set = false;

public:

	ItemMeshBufferInfo(int layer) : layer(layer) {}

	ItemMeshBufferInfo(int layer, bool override, video::SColor color) :
		override_color(color), override_color_set(override),
		layer(layer)
	{}

	ItemMeshBufferInfo(int layer, const AnimationInfo *animation,
			bool override_c = false, video::SColor color = {}) :
		override_color(color), override_color_set(override_c), layer(layer)
	{
		if (animation)
			animation_info = std::make_unique<AnimationInfo>(*animation);
	}

	ItemMeshBufferInfo(int layer_num, const TileLayer &layer);

	void applyOverride(video::SColor &dest) const {
		if (override_color_set)
			dest = override_color;
	}

	bool needColorize(video::SColor target) {
		if (last_colorized_set && target == last_colorized)
			return false;
		last_colorized_set = true;
		last_colorized = target;
		return true;
	}

	// Index of the tile layer this mesh buffer belongs to
	u8 layer;

	// Null for no animated parts
	std::unique_ptr<AnimationInfo> animation_info;
};

struct ItemMesh
{
	scene::IMesh *mesh = nullptr;
	/*
	 * Stores draw information of each mesh buffer.
	 */
	std::vector<ItemMeshBufferInfo> buffer_info;
	/*
	 * If false, all faces of the item should have the same brightness.
	 * Disables shading based on normal vectors.
	 */
	bool needs_shading = true;

	ItemMesh() = default;
};

/*
	Wield item scene node, renders the wield mesh of some item
*/
class WieldMeshSceneNode : public scene::ISceneNode
{
public:
	WieldMeshSceneNode(scene::ISceneManager *mgr, s32 id = -1);
	virtual ~WieldMeshSceneNode();

	// Set appearance from node def
	// d0, l0 = base tile
	// d1, l1 = overlay tile
	void setExtruded(const TileDef &d0, const TileLayer &l0,
			const TileDef &d1, const TileLayer &l1,
			v3f wield_scale, ITextureSource *tsrc);

	void setItem(const ItemStack &item, Client *client,
			bool check_wield_image = true);

	// Sets the vertex color of the wield mesh.
	// Must only be used if the constructor was called with lighting = false
	void setColor(video::SColor color);

	void setLightColorAndAnimation(video::SColor color, float animation_time);

	scene::IMesh *getMesh() { return m_meshnode->getMesh(); }

	virtual void render();

	virtual const aabb3f &getBoundingBox() const { return m_bounding_box; }

private:
	void setExtruded(video::ITexture *base, video::ITexture *overlay,
			v3f wield_scale);

	void changeToMesh(scene::IMesh *mesh);

	// Child scene node with the current wield mesh
	scene::IMeshSceneNode *m_meshnode = nullptr;
	// Material types used as fallback
	video::E_MATERIAL_TYPE m_material_type;

	bool m_anisotropic_filter;
	bool m_bilinear_filter;
	bool m_trilinear_filter;
	/*!
	 * Stores the colors and animation data of the mesh's mesh buffers.
	 * This does not include lighting.
	 */
	std::vector<ItemMeshBufferInfo> m_buffer_info;
	/*!
	 * The base color of this mesh. This is the default
	 * for all mesh buffers.
	 */
	video::SColor m_base_color;

	// Empty if wield image is empty or not animated
	// Owned by this class to get AnimationInfo for the mesh buffer info
	std::vector<FrameSpec> m_wield_image_frames;
	std::vector<FrameSpec> m_wield_overlay_frames;

	// Bounding box culling is disabled for this type of scene node,
	// so this variable is just required so we can implement
	// getBoundingBox() and is set to an empty box.
	const aabb3f m_bounding_box{{0, 0, 0}};

	ShadowRenderer *m_shadow;
};

std::vector<FrameSpec> createAnimationFrames(ITextureSource *tsrc,
		const std::string &image_name, const TileAnimationParams &animation,
		int& result_frame_length_ms);

scene::SMesh *getExtrudedMesh(video::ITexture *texture,
	video::ITexture *overlay_texture = nullptr);

/**
 * Replace the material's shader with a custom one while respecting the usual
 * things expected of node rendering (texture type, alpha mode, overlay).
 * Call this after `TileLayer::applyMaterialOptions`.
 * @param mat material to modify
 * @param shdsrc shader source
 * @param shader name of shader
 * @param mode alpha mode from nodedef
 * @param layer index of this layer
 */
void getAdHocNodeShader(video::SMaterial &mat, IShaderSource *shdsrc,
		const char *shader, AlphaMode mode, int layer);

/**
 * NOTE: The item mesh is only suitable for inventory rendering (due to its
 * material types). In-world rendering of items must go through WieldMeshSceneNode.
 */
// This is only used to initially generate an ItemMesh
// To get the mesh, use ItemVisualsManager::getItemMesh(item, client) instead
void createItemMesh(Client *client, const ItemDefinition &def,
		const AnimationInfo &animation_normal,
		const AnimationInfo &animation_overlay,
		ItemMesh *result);
