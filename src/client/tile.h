// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <ITexture.h>
#include <vector>
#include <SMaterial.h>

enum MaterialType{
	TILE_MATERIAL_BASIC,
	TILE_MATERIAL_ALPHA,
	TILE_MATERIAL_LIQUID_TRANSPARENT,
	TILE_MATERIAL_LIQUID_OPAQUE,
	TILE_MATERIAL_WAVING_LEAVES,
	TILE_MATERIAL_WAVING_PLANTS,
	TILE_MATERIAL_OPAQUE,
	TILE_MATERIAL_WAVING_LIQUID_BASIC,
	TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT,
	TILE_MATERIAL_WAVING_LIQUID_OPAQUE,
	TILE_MATERIAL_PLAIN,
	TILE_MATERIAL_PLAIN_ALPHA
};

// Material flags
// Should backface culling be enabled?
#define MATERIAL_FLAG_BACKFACE_CULLING 0x01
// Should a crack be drawn?
#define MATERIAL_FLAG_CRACK 0x02
// Should the crack be drawn on transparent pixels (unset) or not (set)?
// Ignored if MATERIAL_FLAG_CRACK is not set.
#define MATERIAL_FLAG_CRACK_OVERLAY 0x04
#define MATERIAL_FLAG_ANIMATION 0x08
//#define MATERIAL_FLAG_HIGHLIGHTED 0x10
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

#define MAX_TILE_LAYERS 2

//! Defines a layer of a tile.
struct TileLayer
{
	TileLayer() = default;

	/*!
	 * Two layers are equal if they can be merged.
	 */
	bool operator==(const TileLayer &other) const
	{
		return
			texture_id == other.texture_id &&
			material_type == other.material_type &&
			material_flags == other.material_flags &&
			has_color == other.has_color &&
			color == other.color &&
			scale == other.scale;
	}

	/*!
	 * Two tiles are not equal if they must have different vertices.
	 */
	bool operator!=(const TileLayer &other) const
	{
		return !(*this == other);
	}

	// Sets everything else except the texture in the material
	void applyMaterialOptions(video::SMaterial &material) const;

	void applyMaterialOptionsWithShaders(video::SMaterial &material) const;

	bool isTransparent() const
	{
		switch (material_type) {
		case TILE_MATERIAL_ALPHA:
		case TILE_MATERIAL_LIQUID_TRANSPARENT:
		case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			return true;
		}
		return false;
	}

	// Ordered for size, please do not reorder

	video::ITexture *texture = nullptr;

	u32 shader_id = 0;

	u32 texture_id = 0;

	u16 animation_frame_length_ms = 0;
	u16 animation_frame_count = 1;

	u8 material_type = TILE_MATERIAL_BASIC;
	u8 material_flags =
		//0 // <- DEBUG, Use the one below
		MATERIAL_FLAG_BACKFACE_CULLING |
		MATERIAL_FLAG_TILEABLE_HORIZONTAL|
		MATERIAL_FLAG_TILEABLE_VERTICAL;

	//! If true, the tile has its own color.
	bool has_color = false;

	std::vector<FrameSpec> *frames = nullptr;

	/*!
	 * The color of the tile, or if the tile does not own
	 * a color then the color of the node owning this tile.
	 */
	video::SColor color = video::SColor(0, 0, 0, 0);

	u8 scale = 1;
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

	//! If true, the tile rotation is ignored.
	bool world_aligned = false;
	//! Tile rotation.
	TileRotation rotation = TileRotation::None;
	//! This much light does the tile emit.
	u8 emissive_light = 0;
	//! The first is base texture, the second is overlay.
	TileLayer layers[MAX_TILE_LAYERS];
};
