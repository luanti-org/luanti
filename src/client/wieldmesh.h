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

namespace irr::scene
{
	class ISceneManager;
	class IMesh;
	struct SMesh;
}

using namespace irr;

struct ItemStack;
class Client;
class ITextureSource;
struct ContentFeatures;
struct ItemDefinition;
struct TileAnimationParams;
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

	ItemMeshBufferInfo() = default;

	ItemMeshBufferInfo(bool override, video::SColor color) :
		override_color(color), override_color_set(override)
	{}

	ItemMeshBufferInfo(const TileLayer &layer);

	ItemMeshBufferInfo(AnimationInfo *animation,
			bool override_c = false, video::SColor color = {}) :
		override_color(color), override_color_set(override_c),
		animation_info(animation ? std::make_unique<AnimationInfo>(*animation) : nullptr)
	{}

	ItemMeshBufferInfo(std::vector<FrameSpec> *frames, u16 frame_length_ms,
			bool override_c = false, video::SColor color = {}) :
		override_color(color), override_color_set(override_c),
		animation_info(std::make_unique<AnimationInfo>(frames, frame_length_ms))
	{}


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

	void setExtruded(video::ITexture *texture, video::ITexture *overlay_texture,
			v3f wield_scale);
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
	void changeToMesh(scene::IMesh *mesh);

	// Child scene node with the current wield mesh
	scene::IMeshSceneNode *m_meshnode = nullptr;
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
	aabb3f m_bounding_box{{0, 0, 0}};

	ShadowRenderer *m_shadow;
};

// Returns the animation frames and frame length in milliseconds
std::vector<FrameSpec> createAnimationFrames(ITextureSource *tsrc,
		const std::string &image_name, const TileAnimationParams &animation,
		int& result_frame_length_ms);

scene::SMesh *getExtrudedMesh(video::ITexture *texture,
	video::ITexture *overlay_texture = nullptr);

// This is only used to initially generate an ItemMesh
// To get the mesh use IItemDefManager::getWieldMesh(item, client) instead
void createItemMesh(Client *client, const ItemDefinition &def,
		video::ITexture *inventory_texture, AnimationInfo* inventory_animation,
		video::ITexture *inventory_overlay_texture, AnimationInfo* inventory_overlay_animation,
		ItemMesh *result);
