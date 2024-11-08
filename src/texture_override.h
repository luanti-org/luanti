// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2020 Hugues Ross <hugues.ross@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <string>
#include <vector>

typedef u16 override_t;

//! Bitmask enum specifying what a texture override should apply to
enum class OverrideTarget : override_t
{
	INVALID = 0,
	TOP = 1 << 0,
	BOTTOM = 1 << 1,
	LEFT = 1 << 2,
	RIGHT = 1 << 3,
	FRONT = 1 << 4,
	BACK = 1 << 5,
	INVENTORY = 1 << 6,
	WIELD = 1 << 7,
	SPECIAL_1 = 1 << 8,
	SPECIAL_2 = 1 << 9,
	SPECIAL_3 = 1 << 10,
	SPECIAL_4 = 1 << 11,
	SPECIAL_5 = 1 << 12,
	SPECIAL_6 = 1 << 13,

	SIDES = LEFT | RIGHT | FRONT | BACK,
	ALL_FACES = TOP | BOTTOM | SIDES,
	ALL_SPECIAL = SPECIAL_1 | SPECIAL_2 | SPECIAL_3 | SPECIAL_4 | SPECIAL_5 | SPECIAL_6,
	NODE_TARGETS = ALL_FACES | ALL_SPECIAL,
	ITEM_TARGETS = INVENTORY | WIELD,
};

struct TextureOverride
{
	std::string id;
	std::string texture;
	override_t target = 0;
	u8 world_scale = 0;

	// Helper function for checking if an OverrideTarget is found in
	// a TextureOverride without casting
	inline bool hasTarget(OverrideTarget overrideTarget) const
	{
		return (target & static_cast<override_t>(overrideTarget)) != 0;
	}
};

//! Class that provides texture override information from a texture pack
class TextureOverrideSource
{
public:
	TextureOverrideSource(const std::string &filepath);

	//! Get all overrides that apply to item definitions
	std::vector<TextureOverride> getItemTextureOverrides() const;

	//! Get all overrides that apply to node definitions
	std::vector<TextureOverride> getNodeTileOverrides() const;

private:
	std::vector<TextureOverride> m_overrides;
};
