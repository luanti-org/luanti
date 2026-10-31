// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "tiledef.h"
#include <ITexture.h>
#include <vector>
#include <SMaterial.h>

enum MaterialType : u8 {
	TILE_MATERIAL_NORMAL,
	TILE_MATERIAL_LIQUID,
	// Unshaded entities.
	TILE_MATERIAL_PLAIN,
	MaterialType_END,
};

// Vertex animation is independent of the material's alpha behavior.
enum WavingType : u8 {
	WAVING_NONE,
	WAVING_PLANTS,
	WAVING_LEAVES,
	WAVING_LIQUID,
	WavingType_END,
};

/**
 * @brief change mode so it has at least simple transparency
 */
inline AlphaMode alpha_mode_with_transparency(AlphaMode alpha)
{
	if (alpha == ALPHAMODE_OPAQUE)
		return ALPHAMODE_CLIP;
	else
		return alpha;
}

// Material flags
// Should backface culling be enabled?
#define MATERIAL_FLAG_BACKFACE_CULLING 0x01
// Should a crack be drawn?
#define MATERIAL_FLAG_CRACK 0x02
// Does this layer have texture animation?
#define MATERIAL_FLAG_ANIMATION 0x08
#define MATERIAL_FLAG_TILEABLE_HORIZONTAL 0x20
#define MATERIAL_FLAG_TILEABLE_VERTICAL 0x40

/*
	This fully defines the looks of a tile.
	The SMaterial of a tile is constructed according to this.
*/

struct FrameSpec
{
	FrameSpec() = default;

	u32 texture_id = 0;
	video::ITexture *texture = nullptr;
};

/**
 * We have two tile layers:
 * layer 0 = base
 * layer 1 = overlay
 */
#define MAX_TILE_LAYERS 2

//! Defines a layer of a tile.
struct TileLayer
{
	TileLayer() = default;

	/*!
	 * Two layers are equal if they can be merged (same material).
	 */
	bool operator==(const TileLayer &other) const
	{
		return
			texture_id == other.texture_id &&
			shader_id == other.shader_id &&
			waving == other.waving &&
			alpha == other.alpha &&
			material_flags == other.material_flags &&
			has_color == other.has_color &&
			color == other.color &&
			need_polygon_offset == other.need_polygon_offset;
		// texture_layer_idx, scale and world_aligned are baked into vertex data
	}

	/*!
	 * Two layers are not equal if they must have different vertices.
	 */
	bool operator!=(const TileLayer &other) const
	{
		return !(*this == other);
	}

	/**
	 * Set some material parameters accordingly.
	 * @note does not set `MaterialType`!
	 * @param material material to modify
	 * @param layer index of this layer in the `TileSpec`
	 */
	void applyMaterialOptions(video::SMaterial &material, int layer) const;

	/// @return is this layer uninitialized?
	bool empty() const
	{
		return !shader_id && !texture_id;
	}

	/// @return is this layer semi-transparent?
	bool isTransparent() const
	{
		return alpha == ALPHAMODE_BLEND;
	}

	// Ordered for size, please do not reorder

	video::ITexture *texture = nullptr;

	u32 shader_id = 0;

	u32 texture_id = 0;

	u16 animation_frame_length_ms = 0;
	u16 animation_frame_count = 1;

	/// Layer index to use, if the texture is an array texture
	u16 texture_layer_idx = 0;

	MaterialType material_type = TILE_MATERIAL_NORMAL;
	AlphaMode alpha = ALPHAMODE_CLIP;
	WavingType waving = WAVING_NONE;
	u8 material_flags =
		MATERIAL_FLAG_BACKFACE_CULLING |
		MATERIAL_FLAG_TILEABLE_HORIZONTAL|
		MATERIAL_FLAG_TILEABLE_VERTICAL;

	/// Texture scale in both directions (used for world-align)
	u8 scale = 1;

	/// does this tile need to have a positive polygon offset set?
	/// @see TileLayer::applyMaterialOptions
	bool need_polygon_offset = false;

	/// @note not owned by this struct
	std::vector<FrameSpec> *frames = nullptr;

	/*!
	 * The color of the tile, or if the tile does not own
	 * a color then the color of the node owning this tile.
	 */
	video::SColor color;

	//! If true, the tile has its own color.
	bool has_color = false;

	//! If true, the tile rotation is ignored.
	bool world_aligned = false;
};

template<>
struct std::hash<TileLayer>
{
	// All layers equal according to TileLayer::operator== will have the same
	// hash value according to this function.
	std::size_t operator()(const TileLayer &l) const noexcept
	{
		std::size_t ret = 0;
		for (auto h : { l.texture_id, l.shader_id, (u32)l.material_flags, (u32)l.waving,
				(u32)l.alpha }) {
			ret += h;
			ret ^= (ret << 6) + (ret >> 2); // distribute bits
		}
		return ret;
	}
};


// Stores information for drawing an animated tile
struct AnimationInfo {

	AnimationInfo() = default;

	AnimationInfo(const TileLayer &tile) :
			m_frame_length_ms(tile.animation_frame_length_ms),
			m_frame_count(tile.animation_frame_count),
			m_frames(tile.frames)
	{}

	AnimationInfo(std::vector<FrameSpec> *frames, u16 frame_length_ms) :
			m_frame_length_ms(frame_length_ms),
			m_frame_count(frames->size()),
			m_frames(frames)
	{}

	size_t getFrameCount() const
	{
		return m_frames ? m_frame_count : 0;
	}

	void updateTexture(video::SMaterial &material, float animation_time);

	// Returns nullptr if texture did not change since last time
	video::ITexture *getTexture(float animation_time) const;

private:
	u16 m_frame_length_ms = 0;
	u16 m_frame_count = 1;

	/// @note by default not owned by this struct
	std::vector<FrameSpec> *m_frames = nullptr;
};

enum class TileRotation: u8 {
	None,
	R90,
	R180,
	R270,
};

/*!
 * Defines a face of a node. May have up to two layers.
 */
struct TileSpec
{
	TileSpec() = default;

	//! Tile rotation.
	TileRotation rotation = TileRotation::None;
	//! The first is base texture, the second is overlay.
	TileLayer layers[MAX_TILE_LAYERS];
};
