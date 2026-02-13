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
	MeshMakeData *const m_data;
	MeshCollector *const m_collector;
	const NodeDefManager *const m_nodedef;
	const v3s16 m_blockpos_nodes;
	const bool m_is_textureless;

	using bitset = u64;
	static constexpr bitset U62_MAX = U64_MAX >> 2;

	// max bits the fit in a bitset
	static constexpr s16 BITSET_MAX = 64;
	// max bits the fit in a bitset squared
	static constexpr s16 BITSET_MAX2 = BITSET_MAX * BITSET_MAX;
	// max bits the fit in a bitset without padding nodes
	static constexpr s16 BITSET_MAX_NOPAD = 62;
	// max bits the fit in a bitset without padding nodes squared
	static constexpr s16 BITSET_MAX_NOPAD2 = BITSET_MAX_NOPAD * BITSET_MAX_NOPAD;

	u8 m_node_width;

	// LOD mesh gen uses bit shifts for gredy meshing, so we have to split the volume into segments that fit in a u64
	v3s16 m_seg_start;
	v3s16 m_seg_size;

	std::array<bitset, 3 * BITSET_MAX2> m_all_set_solid_nodes;
	std::array<bitset, 3 * BITSET_MAX2> m_all_set_transparent_nodes;

	std::bitset<NodeDrawType_END> m_solid_set;
	std::bitset<NodeDrawType_END> m_transparent_set;

	bitset m_nodes_faces[6 * BITSET_MAX_NOPAD2];
	bitset m_slices[6 * BITSET_MAX_NOPAD2];

	static constexpr core::vector3df s_normals[6] = {
		core::vector3df(0, 1, 0), core::vector3df(0, -1, 0),
		core::vector3df(1, 0, 0), core::vector3df(-1, 0, 0),
		core::vector3df(0, 0, 1), core::vector3df(0, 0, -1)
	};
	TileSpec m_solid_tile = [] {
		TileSpec tile;
		TileLayer layer;
		layer.shader_id = -1;
		tile.layers[0] = layer;
		tile.layers[0].color = {255, 255, 255, 255};
		return tile;
	}();

	void drawMeshNode(v3s16 pos, MapNode n, const ContentFeatures *f) const;
	void generateGreedyLod();
	void generateBitsetMesh(MapNode n, v3s16 seg_start, video::SColor color_in);
	void processNodeGroup(const std::array<bitset, 3 * BITSET_MAX2> &all_set_nodes,
		std::unordered_map<NodeKey, std::array<bitset, 3 * BITSET_MAX2>> &subset_nodes,
		std::map<content_t, MapNode> &node_types);
	LightPair computeMaxFaceLight(MapNode n, v3s16 p, v3s16 dir);
	static void setBitIndex(std::array<bitset, 3 * BITSET_MAX2> &vol, u8 x, u8 y, u8 z);
	void generateLodChunks();
};
