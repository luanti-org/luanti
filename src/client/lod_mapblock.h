// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "nodedef.h"
#include <bitset>
#include <map>
#include <unordered_map>

#include "tile.h"

struct MeshMakeData;
struct MeshCollector;

struct NodeKey {
	content_t content;
	LightPair light;

	bool operator==(const NodeKey& other) const {
		return content == other.content && light == other.light;
	}
};

template<>
struct std::hash<NodeKey> {
	std::size_t operator()(const NodeKey& k) const {
		return std::hash<content_t>()(k.content) ^ (std::hash<u16>()(k.light) << 1);
	}
};

class LodMeshGenerator
{
public:
	LodMeshGenerator(MeshMakeData *input, MeshCollector *output, bool is_textureless, u32 solid_shader_id);
	void generate(u8 lod);

private:
	static constexpr core::vector3df s_normals[6] = {
		core::vector3df(0, 1, 0), core::vector3df(0, -1, 0),
		core::vector3df(1, 0, 0), core::vector3df(-1, 0, 0),
		core::vector3df(0, 0, 1), core::vector3df(0, 0, -1)
	};

	static constexpr v3s16 tile_dirs[6] = {
		{0, 1, 0}, {0, -1, 0},
		{1, 0, 0}, {-1, 0, 0},
		{0, 0, 1}, {0, 0, -1}
	};

	MeshMakeData *const m_data;
	MeshCollector *const m_collector;
	const NodeDefManager *const m_nodedef;
	const v3s16 m_blockpos_nodes;
	const bool m_is_textureless;

	u8 m_node_width;

	TileSpec m_solid_tile = [] {
		TileSpec tile;
		TileLayer layer;
		layer.shader_id = -1;
		tile.layers[0] = layer;
		tile.layers[0].color = {255, 255, 255, 255};
		return tile;
	}();

	struct {
		v3s16 p; // relative to blockpos_nodes
		MapNode n;
		const ContentFeatures *f;
	} m_cur_node;

	void drawMeshNode();
	void drawSolidNode();
	void drawNode();
};
