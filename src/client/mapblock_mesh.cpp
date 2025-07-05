// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "mapblock_mesh.h"
#include "CMeshBuffer.h"
#include "client.h"
#include "mapblock.h"
#include "map.h"
#include "noise.h"
#include "profiler.h"
#include "shader.h"
#include "mesh.h"
#include "minimap.h"
#include "content_mapblock.h"
#include "util/directiontables.h"
#include "util/tracy_wrapper.h"
#include "client/meshgen/collector.h"
#include "client/renderingengine.h"
#include <array>
#include <algorithm>
#include <cmath>
#include "client/texturesource.h"
#include <SMesh.h>
#include <IMeshBuffer.h>
#include <CMeshBuffer.h>

/*
	MeshMakeData
*/

MeshMakeData::MeshMakeData(const NodeDefManager *ndef,
		u16 side_length, MeshGrid mesh_grid) :
	m_side_length(side_length),
	m_mesh_grid(mesh_grid),
	m_nodedef(ndef)
{
	assert(m_side_length > 0);
}

void MeshMakeData::fillBlockDataBegin(const v3s16 &blockpos)
{
	m_blockpos = blockpos;

	v3s16 blockpos_nodes = m_blockpos*MAP_BLOCKSIZE;

	m_vmanip.clear();
	// extra 1 block thick layer around the mesh
	VoxelArea voxel_area(blockpos_nodes - v3s16(1,1,1) * MAP_BLOCKSIZE,
			blockpos_nodes + v3s16(1,1,1) * (m_side_length + MAP_BLOCKSIZE) - v3s16(1,1,1));
	m_vmanip.addArea(voxel_area);
}

void MeshMakeData::fillSingleNode(MapNode data, MapNode padding)
{
	m_blockpos = {0, 0, 0};

	m_vmanip.clear();
	// area around 0,0,0 so that this positon has neighbors
	const s16 sz = 3;
	m_vmanip.addArea({v3s16(-sz), v3s16(sz)});

	u32 count = m_vmanip.m_area.getVolume();
	for (u32 i = 0; i < count; i++) {
		m_vmanip.m_data[i] = padding;
		m_vmanip.m_flags[i] &= ~VOXELFLAG_NO_DATA;
	}

	m_vmanip.setNodeNoEmerge({0, 0, 0}, data);
}

void MeshMakeData::setCrack(int crack_level, v3s16 crack_pos)
{
	if (crack_level >= 0)
		m_crack_pos_relative = crack_pos - m_blockpos*MAP_BLOCKSIZE;
}

/*
	Light and vertex color functions
*/

/*
	Calculate non-smooth lighting at interior of node.
	Single light bank.
*/
static u8 getInteriorLight(enum LightBank bank, MapNode n, s32 increment,
	const NodeDefManager *ndef)
{
	u8 light = n.getLight(bank, ndef->getLightingFlags(n));
	light = rangelim(light + increment, 0, LIGHT_SUN);
	return decode_light(light);
}

/*
	Calculate non-smooth lighting at interior of node.
	Both light banks.
*/
u16 getInteriorLight(MapNode n, s32 increment, const NodeDefManager *ndef)
{
	u16 day = getInteriorLight(LIGHTBANK_DAY, n, increment, ndef);
	u16 night = getInteriorLight(LIGHTBANK_NIGHT, n, increment, ndef);
	return day | (night << 8);
}

/*
	Calculate non-smooth lighting at face of node.
	Single light bank.
*/
static u8 getFaceLight(enum LightBank bank, MapNode n, MapNode n2, const NodeDefManager *ndef)
{
	ContentLightingFlags f1 = ndef->getLightingFlags(n);
	ContentLightingFlags f2 = ndef->getLightingFlags(n2);

	u8 light;
	u8 l1 = n.getLight(bank, f1);
	u8 l2 = n2.getLight(bank, f2);
	if(l1 > l2)
		light = l1;
	else
		light = l2;

	// Boost light level for light sources
	u8 light_source = MYMAX(f1.light_source, f2.light_source);
	if(light_source > light)
		light = light_source;

	return decode_light(light);
}

/*
	Calculate non-smooth lighting at face of node.
	Both light banks.
*/
u16 getFaceLight(MapNode n, MapNode n2, const NodeDefManager *ndef)
{
	u16 day = getFaceLight(LIGHTBANK_DAY, n, n2, ndef);
	u16 night = getFaceLight(LIGHTBANK_NIGHT, n, n2, ndef);
	return day | (night << 8);
}

/*
	Calculate smooth lighting at the XYZ- corner of p.
	Both light banks
*/
static u16 getSmoothLightCombined(const v3s16 &p,
	const std::array<v3s16,8> &dirs, MeshMakeData *data)
{
	const NodeDefManager *ndef = data->m_nodedef;

	u16 ambient_occlusion = 0;
	u16 light_count = 0;
	u8 light_source_max = 0;
	u16 light_day = 0;
	u16 light_night = 0;
	bool direct_sunlight = false;

	auto add_node = [&] (u8 i, bool obstructed = false) -> bool {
		if (obstructed) {
			ambient_occlusion++;
			return false;
		}
		MapNode n = data->m_vmanip.getNodeNoExNoEmerge(p + dirs[i]);
		if (n.getContent() == CONTENT_IGNORE)
			return true;
		const ContentFeatures &f = ndef->get(n);
		if (f.light_source > light_source_max)
			light_source_max = f.light_source;
		// Check f.solidness because fast-style leaves look better this way
		if (f.param_type == CPT_LIGHT && f.solidness != 2) {
			u8 light_level_day = n.getLight(LIGHTBANK_DAY, f.getLightingFlags());
			u8 light_level_night = n.getLight(LIGHTBANK_NIGHT, f.getLightingFlags());
			if (light_level_day == LIGHT_SUN)
				direct_sunlight = true;
			light_day += decode_light(light_level_day);
			light_night += decode_light(light_level_night);
			light_count++;
		} else {
			ambient_occlusion++;
		}
		return f.light_propagates;
	};

	bool obstructed[4] = { true, true, true, true };
	add_node(0);
	bool opaque1 = !add_node(1);
	bool opaque2 = !add_node(2);
	bool opaque3 = !add_node(3);
	obstructed[0] = opaque1 && opaque2;
	obstructed[1] = opaque1 && opaque3;
	obstructed[2] = opaque2 && opaque3;
	for (u8 k = 0; k < 3; ++k)
		if (add_node(k + 4, obstructed[k]))
			obstructed[3] = false;
	if (add_node(7, obstructed[3])) { // wrap light around nodes
		ambient_occlusion -= 3;
		for (u8 k = 0; k < 3; ++k)
			add_node(k + 4, !obstructed[k]);
	}

	if (light_count == 0) {
		light_day = light_night = 0;
	} else {
		light_day /= light_count;
		light_night /= light_count;
	}

	// boost direct sunlight, if any
	if (direct_sunlight)
		light_day = 0xFF;

	// Boost brightness around light sources
	bool skip_ambient_occlusion_day = false;
	if (decode_light(light_source_max) >= light_day) {
		light_day = decode_light(light_source_max);
		skip_ambient_occlusion_day = true;
	}

	bool skip_ambient_occlusion_night = false;
	if(decode_light(light_source_max) >= light_night) {
		light_night = decode_light(light_source_max);
		skip_ambient_occlusion_night = true;
	}

	if (ambient_occlusion > 4) {
		static thread_local const float ao_gamma = rangelim(
			g_settings->getFloat("ambient_occlusion_gamma"), 0.25, 4.0);

		// Table of gamma space multiply factors.
		static thread_local const float light_amount[3] = {
			powf(0.75, 1.0 / ao_gamma),
			powf(0.5,  1.0 / ao_gamma),
			powf(0.25, 1.0 / ao_gamma)
		};

		//calculate table index for gamma space multiplier
		ambient_occlusion -= 5;

		if (!skip_ambient_occlusion_day)
			light_day = rangelim(core::round32(
					light_day * light_amount[ambient_occlusion]), 0, 255);
		if (!skip_ambient_occlusion_night)
			light_night = rangelim(core::round32(
					light_night * light_amount[ambient_occlusion]), 0, 255);
	}

	return light_day | (light_night << 8);
}

/*
	Calculate smooth lighting at the given corner of p.
	Both light banks.
	Node at p is solid, and thus the lighting is face-dependent.
*/
u16 getSmoothLightSolid(const v3s16 &p, const v3s16 &face_dir, const v3s16 &corner, MeshMakeData *data)
{
	return getSmoothLightTransparent(p + face_dir, corner - 2 * face_dir, data);
}

/*
	Calculate smooth lighting at the given corner of p.
	Both light banks.
	Node at p is not solid, and the lighting is not face-dependent.
*/
u16 getSmoothLightTransparent(const v3s16 &p, const v3s16 &corner, MeshMakeData *data)
{
	const std::array<v3s16,8> dirs = {{
		// Always shine light
		v3s16(0,0,0),
		v3s16(corner.X,0,0),
		v3s16(0,corner.Y,0),
		v3s16(0,0,corner.Z),

		// Can be obstructed
		v3s16(corner.X,corner.Y,0),
		v3s16(corner.X,0,corner.Z),
		v3s16(0,corner.Y,corner.Z),
		v3s16(corner.X,corner.Y,corner.Z)
	}};
	return getSmoothLightCombined(p, dirs, data);
}

void get_sunlight_color(video::SColorf *sunlight, u32 daynight_ratio)
{
	f32 rg = daynight_ratio / 1000.0f - 0.04f;
	f32 b = (0.98f * daynight_ratio) / 1000.0f + 0.078f;
	sunlight->r = rg;
	sunlight->g = rg;
	sunlight->b = b;
}

void final_color_blend(video::SColor *result,
		u16 light, u32 daynight_ratio)
{
	video::SColorf dayLight;
	get_sunlight_color(&dayLight, daynight_ratio);
	final_color_blend(result,
		encode_light(light, 0), dayLight);
}

void final_color_blend(video::SColor *result,
		const video::SColor &data, const video::SColorf &dayLight)
{
	static const video::SColorf artificialColor(1.04f, 1.04f, 1.04f);

	video::SColorf c(data);
	f32 n = 1 - c.a;

	f32 r = c.r * (c.a * dayLight.r + n * artificialColor.r) * 2.0f;
	f32 g = c.g * (c.a * dayLight.g + n * artificialColor.g) * 2.0f;
	f32 b = c.b * (c.a * dayLight.b + n * artificialColor.b) * 2.0f;

	// Emphase blue a bit in darker places
	// Each entry of this array represents a range of 8 blue levels
	static const u8 emphase_blue_when_dark[32] = {
		1, 4, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	b += emphase_blue_when_dark[core::clamp((s32) ((r + g + b) / 3 * 255),
		0, 255) / 8] / 255.0f;

	result->setRed(core::clamp((s32) (r * 255.0f), 0, 255));
	result->setGreen(core::clamp((s32) (g * 255.0f), 0, 255));
	result->setBlue(core::clamp((s32) (b * 255.0f), 0, 255));
}

/*
	Mesh generation helpers
*/

/*
	Gets nth node tile (0 <= n <= 5).
*/
void getNodeTileN(MapNode mn, const v3s16 &p, u8 tileindex, MeshMakeData *data, TileSpec &tile)
{
	const NodeDefManager *ndef = data->m_nodedef;
	const ContentFeatures &f = ndef->get(mn);
	tile = f.tiles[tileindex];
	bool has_crack = p == data->m_crack_pos_relative;
	for (TileLayer &layer : tile.layers) {
		if (layer.empty())
			continue;
		if (!layer.has_color)
			mn.getColor(f, &(layer.color));
		// Apply temporary crack
		if (has_crack)
			layer.material_flags |= MATERIAL_FLAG_CRACK;
	}
}

/*
	Gets node tile given a face direction.
*/
void getNodeTile(MapNode mn, const v3s16 &p, const v3s16 &dir, MeshMakeData *data, TileSpec &tile)
{
	const NodeDefManager *ndef = data->m_nodedef;

	// Direction must be (1,0,0), (-1,0,0), (0,1,0), (0,-1,0),
	// (0,0,1), (0,0,-1) or (0,0,0)
	assert(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z <= 1);

	// Convert direction to single integer for table lookup
	//  0 = (0,0,0)
	//  1 = (1,0,0)
	//  2 = (0,1,0)
	//  3 = (0,0,1)
	//  4 = invalid, treat as (0,0,0)
	//  5 = (0,0,-1)
	//  6 = (0,-1,0)
	//  7 = (-1,0,0)
	u8 dir_i = (dir.X + 2 * dir.Y + 3 * dir.Z) & 7;

	// Get rotation for things like chests
	u8 facedir = mn.getFaceDir(ndef, true);

	static constexpr auto
		R0 = TileRotation::None,
		R1 = TileRotation::R90,
		R2 = TileRotation::R180,
		R3 = TileRotation::R270;
	static const struct {
		u8 tile;
		TileRotation rotation;
	} dir_to_tile[24][8] = {
		//  0        +X      +Y      +Z                -Z      -Y      -X      ->   value=tile,rotation
		   {{0,R0},  {2,R0}, {0,R0}, {4,R0},  {0,R0},  {5,R0}, {1,R0}, {3,R0}},  // rotate around y+ 0 - 3
		   {{0,R0},  {4,R0}, {0,R3}, {3,R0},  {0,R0},  {2,R0}, {1,R1}, {5,R0}},
		   {{0,R0},  {3,R0}, {0,R2}, {5,R0},  {0,R0},  {4,R0}, {1,R2}, {2,R0}},
		   {{0,R0},  {5,R0}, {0,R1}, {2,R0},  {0,R0},  {3,R0}, {1,R3}, {4,R0}},

		   {{0,R0},  {2,R3}, {5,R0}, {0,R2},  {0,R0},  {1,R0}, {4,R2}, {3,R1}},  // rotate around z+ 4 - 7
		   {{0,R0},  {4,R3}, {2,R0}, {0,R1},  {0,R0},  {1,R1}, {3,R2}, {5,R1}},
		   {{0,R0},  {3,R3}, {4,R0}, {0,R0},  {0,R0},  {1,R2}, {5,R2}, {2,R1}},
		   {{0,R0},  {5,R3}, {3,R0}, {0,R3},  {0,R0},  {1,R3}, {2,R2}, {4,R1}},

		   {{0,R0},  {2,R1}, {4,R2}, {1,R2},  {0,R0},  {0,R0}, {5,R0}, {3,R3}},  // rotate around z- 8 - 11
		   {{0,R0},  {4,R1}, {3,R2}, {1,R3},  {0,R0},  {0,R3}, {2,R0}, {5,R3}},
		   {{0,R0},  {3,R1}, {5,R2}, {1,R0},  {0,R0},  {0,R2}, {4,R0}, {2,R3}},
		   {{0,R0},  {5,R1}, {2,R2}, {1,R1},  {0,R0},  {0,R1}, {3,R0}, {4,R3}},

		   {{0,R0},  {0,R3}, {3,R3}, {4,R1},  {0,R0},  {5,R3}, {2,R3}, {1,R3}},  // rotate around x+ 12 - 15
		   {{0,R0},  {0,R2}, {5,R3}, {3,R1},  {0,R0},  {2,R3}, {4,R3}, {1,R0}},
		   {{0,R0},  {0,R1}, {2,R3}, {5,R1},  {0,R0},  {4,R3}, {3,R3}, {1,R1}},
		   {{0,R0},  {0,R0}, {4,R3}, {2,R1},  {0,R0},  {3,R3}, {5,R3}, {1,R2}},

		   {{0,R0},  {1,R1}, {2,R1}, {4,R3},  {0,R0},  {5,R1}, {3,R1}, {0,R1}},  // rotate around x- 16 - 19
		   {{0,R0},  {1,R2}, {4,R1}, {3,R3},  {0,R0},  {2,R1}, {5,R1}, {0,R0}},
		   {{0,R0},  {1,R3}, {3,R1}, {5,R3},  {0,R0},  {4,R1}, {2,R1}, {0,R3}},
		   {{0,R0},  {1,R0}, {5,R1}, {2,R3},  {0,R0},  {3,R1}, {4,R1}, {0,R2}},

		   {{0,R0},  {3,R2}, {1,R2}, {4,R2},  {0,R0},  {5,R2}, {0,R2}, {2,R2}},  // rotate around y- 20 - 23
		   {{0,R0},  {5,R2}, {1,R3}, {3,R2},  {0,R0},  {2,R2}, {0,R1}, {4,R2}},
		   {{0,R0},  {2,R2}, {1,R0}, {5,R2},  {0,R0},  {4,R2}, {0,R0}, {3,R2}},
		   {{0,R0},  {4,R2}, {1,R1}, {2,R2},  {0,R0},  {3,R2}, {0,R3}, {5,R2}}
	};
	getNodeTileN(mn, p, dir_to_tile[facedir][dir_i].tile, data, tile);
	tile.rotation = tile.world_aligned ? TileRotation::None : dir_to_tile[facedir][dir_i].rotation;
}

/*
	MapBlockBspTree
*/

void MapBlockBspTree::buildTree(const std::vector<MeshTriangle> *triangles, u16 side_length)
{
	this->triangles = triangles;

	nodes.clear();

	// assert that triangle index can fit into s32
	assert(triangles->size() <= 0x7FFFFFFFL);
	std::vector<s32> indexes;
	indexes.reserve(triangles->size());
	for (u32 i = 0; i < triangles->size(); i++)
		indexes.push_back(i);

	if (!indexes.empty()) {
		// Start in the center of the block with increment of one quarter in each direction
		root = buildTree(v3f(1, 0, 0), v3f((side_length + 1) * 0.5f * BS), side_length * 0.25f * BS, indexes, 0);
	} else {
		root = -1;
	}
}

/**
 * @brief Find a candidate plane to split a set of triangles in two
 *
 * The candidate plane is represented by one of the triangles from the set.
 *
 * @param list Vector of indexes of the triangles in the set
 * @param triangles Vector of all triangles in the BSP tree
 * @return Address of the triangle that represents the proposed split plane
 */
static const MeshTriangle *findSplitCandidate(const std::vector<s32> &list, const std::vector<MeshTriangle> &triangles)
{
	// find the center of the cluster.
	v3f center(0, 0, 0);
	size_t n = list.size();
	for (s32 i : list) {
		center += triangles[i].centroid / n;
	}

	// find the triangle with the largest area and closest to the center
	const MeshTriangle *candidate_triangle = &triangles[list[0]];
	const MeshTriangle *ith_triangle;
	for (s32 i : list) {
		ith_triangle = &triangles[i];
		if (ith_triangle->areaSQ > candidate_triangle->areaSQ ||
				(ith_triangle->areaSQ == candidate_triangle->areaSQ &&
				ith_triangle->centroid.getDistanceFromSQ(center) < candidate_triangle->centroid.getDistanceFromSQ(center))) {
			candidate_triangle = ith_triangle;
		}
	}
	return candidate_triangle;
}

s32 MapBlockBspTree::buildTree(v3f normal, v3f origin, float delta, const std::vector<s32> &list, u32 depth)
{
	// if the list is empty, don't bother
	if (list.empty())
		return -1;

	// if there is only one triangle, or the delta is insanely small, this is a leaf node
	if (list.size() == 1 || delta < 0.01) {
		nodes.emplace_back(normal, origin, list, -1, -1);
		return nodes.size() - 1;
	}

	std::vector<s32> front_list;
	std::vector<s32> back_list;
	std::vector<s32> node_list;

	// split the list
	for (s32 i : list) {
		const MeshTriangle &triangle = (*triangles)[i];
		float factor = normal.dotProduct(triangle.centroid - origin);
		if (factor == 0)
			node_list.push_back(i);
		else if (factor > 0)
			front_list.push_back(i);
		else
			back_list.push_back(i);
	}

	// define the new split-plane
	v3f candidate_normal(normal.Z, normal.X, normal.Y);
	float candidate_delta = delta;
	if (depth % 3 == 2)
		candidate_delta /= 2;

	s32 front_index = -1;
	s32 back_index = -1;

	if (!front_list.empty()) {
		v3f next_normal = candidate_normal;
		v3f next_origin = origin + delta * normal;
		float next_delta = candidate_delta;
		if (next_delta < 5) {
			const MeshTriangle *candidate = findSplitCandidate(front_list, *triangles);
			next_normal = candidate->getNormal();
			next_origin = candidate->centroid;
		}
		front_index = buildTree(next_normal, next_origin, next_delta, front_list, depth + 1);

		// if there are no other triangles, don't create a new node
		if (back_list.empty() && node_list.empty())
			return front_index;
	}

	if (!back_list.empty()) {
		v3f next_normal = candidate_normal;
		v3f next_origin = origin - delta * normal;
		float next_delta = candidate_delta;
		if (next_delta < 5) {
			const MeshTriangle *candidate = findSplitCandidate(back_list, *triangles);
			next_normal = candidate->getNormal();
			next_origin = candidate->centroid;
		}

		back_index = buildTree(next_normal, next_origin, next_delta, back_list, depth + 1);

		// if there are no other triangles, don't create a new node
		if (front_list.empty() && node_list.empty())
			return back_index;
	}

	nodes.emplace_back(normal, origin, node_list, front_index, back_index);

	return nodes.size() - 1;
}

void MapBlockBspTree::traverse(s32 node, v3f viewpoint, std::vector<s32> &output) const
{
	if (node < 0) return; // recursion break;

	const TreeNode &n = nodes[node];
	float factor = n.normal.dotProduct(viewpoint - n.origin);

	if (factor > 0)
		traverse(n.back_ref, viewpoint, output);
	else
		traverse(n.front_ref, viewpoint, output);

	if (factor != 0)
		for (s32 i : n.triangle_refs)
			output.push_back(i);

	if (factor > 0)
		traverse(n.front_ref, viewpoint, output);
	else
		traverse(n.back_ref, viewpoint, output);
}



/*
	PartialMeshBuffer
*/

void PartialMeshBuffer::draw(video::IVideoDriver *driver) const
{
	const auto pType = m_buffer->getPrimitiveType();
	driver->drawBuffers(m_buffer->getVertexBuffer(), m_indices.get(),
		m_indices->getPrimitiveCount(pType), pType);
}

/*
	MapBlockMesh
*/

MapBlockMesh::MapBlockMesh(Client *client, MeshMakeData *data):
	m_tsrc(client->getTextureSource()),
	m_shdrsrc(client->getShaderSource()),
	m_bounding_sphere_center((data->m_side_length * 0.5f - 0.5f) * BS),
	m_animation_force_timer(0), // force initial animation
	m_last_crack(-1)
{
	ZoneScoped;

	for (auto &m : m_mesh)
		m = make_irr<scene::SMesh>();

	auto mesh_grid = data->m_mesh_grid;
	v3s16 bp = data->m_blockpos;
	// Only generate minimap mapblocks at grid aligned coordinates.
	// FIXME: ^ doesn't really make sense. and in practice, bp is always aligned
	if (mesh_grid.isMeshPos(bp) && data->m_generate_minimap) {
		// meshgen area always fits into a grid cell
		m_minimap_mapblocks.resize(mesh_grid.getCellVolume(), nullptr);
		v3s16 ofs;

		// See also client.cpp for the code that reads the array of minimap blocks.
		for (ofs.Z = 0; ofs.Z < mesh_grid.cell_size; ofs.Z++)
		for (ofs.Y = 0; ofs.Y < mesh_grid.cell_size; ofs.Y++)
		for (ofs.X = 0; ofs.X < mesh_grid.cell_size; ofs.X++) {
			v3s16 p = (bp + ofs) * MAP_BLOCKSIZE;
			if (data->m_vmanip.getNodeNoEx(p).getContent() != CONTENT_IGNORE) {
				MinimapMapblock *block = new MinimapMapblock;
				m_minimap_mapblocks[mesh_grid.getOffsetIndex(ofs)] = block;
				block->getMinimapNodes(&data->m_vmanip, data->m_nodedef, p);
			}
		}
	}

	// algin vertices to mesh grid, not meshgen area
	v3f offset = intToFloat((data->m_blockpos - mesh_grid.getMeshPos(data->m_blockpos)) * MAP_BLOCKSIZE, BS);

	MeshCollector collector(m_bounding_sphere_center, offset);

	{
		// Generate everything
		MapblockMeshGenerator(data, &collector).generate();
	}

	/*
		Convert MeshCollector to SMesh
	*/

	m_bounding_radius = std::sqrt(collector.m_bounding_radius_sq);

	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
		scene::SMesh *mesh = static_cast<scene::SMesh *>(m_mesh[layer].get());

		for(u32 i = 0; i < collector.prebuffers[layer].size(); i++)
		{
			PreMeshBuffer &p = collector.prebuffers[layer][i];

			p.applyTileColor();

			// Generate animation data
			// - Cracks
			if (p.layer.material_flags & MATERIAL_FLAG_CRACK) {
				// Find the texture name plus ^[crack:N:
				std::ostringstream os(std::ios::binary);
				os << m_tsrc->getTextureName(p.layer.texture_id) << "^[crack";
				if (p.layer.material_flags & MATERIAL_FLAG_CRACK_OVERLAY)
					os << "o";  // use ^[cracko
				u8 tiles = p.layer.scale;
				if (tiles > 1)
					os << ":" << (u32)tiles;
				os << ":" << (u32)p.layer.animation_frame_count << ":";
				m_crack_materials.insert(std::make_pair(
						std::pair<u8, u32>(layer, i), os.str()));
				// Replace tile texture with the cracked one
				p.layer.texture = m_tsrc->getTextureForMesh(
						os.str() + "0",
						&p.layer.texture_id);
			}
			// - Texture animation
			if (p.layer.material_flags & MATERIAL_FLAG_ANIMATION) {
				// Add to MapBlockMesh in order to animate these tiles
				m_animation_info.emplace(std::make_pair(layer, i), AnimationInfo(p.layer));
				// Replace tile texture with the first animation frame
				p.layer.texture = (*p.layer.frames)[0].texture;
			}

			// Create material
			video::SMaterial material;
			material.FogEnable = true;
			material.forEachTexture([] (auto &tex) {
				tex.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
				tex.MagFilter = video::ETMAGF_NEAREST;
			});

			{
				material.MaterialType = m_shdrsrc->getShaderInfo(
						p.layer.shader_id).material;
				p.layer.applyMaterialOptions(material, layer);
			}

			scene::SMeshBuffer *buf = new scene::SMeshBuffer();
			buf->Material = material;
			if (p.layer.isTransparent()) {
				buf->append(&p.vertices[0], p.vertices.size(), nullptr, 0);

				MeshTriangle t;
				t.buffer = buf;
				m_transparent_triangles.reserve(p.indices.size() / 3);
				for (u32 i = 0; i < p.indices.size(); i += 3) {
					t.p1 = p.indices[i];
					t.p2 = p.indices[i + 1];
					t.p3 = p.indices[i + 2];
					t.updateAttributes();
					m_transparent_triangles.push_back(t);
				}
			} else {
				buf->append(&p.vertices[0], p.vertices.size(),
					&p.indices[0], p.indices.size());
			}
			mesh->addMeshBuffer(buf);
			buf->drop();
		}

		if (mesh) {
			// Use VBO for mesh (this just would set this for every buffer)
			mesh->setHardwareMappingHint(scene::EHM_STATIC);
		}
	}

	m_bsp_tree.buildTree(&m_transparent_triangles, data->m_side_length);

	// Check if animation is required for this mesh
	m_has_animation =
		!m_crack_materials.empty() ||
		!m_animation_info.empty();
}

MapBlockMesh::~MapBlockMesh()
{
	size_t sz = 0;
	for (auto &&m : m_mesh) {
		for (u32 i = 0; i < m->getMeshBufferCount(); i++)
			sz += m->getMeshBuffer(i)->getSize();
		m.reset();
	}
	for (MinimapMapblock *block : m_minimap_mapblocks)
		delete block;

	porting::TrackFreedMemory(sz);
}

bool MapBlockMesh::animate(bool faraway, float time, int crack,
	u32 daynight_ratio)
{
	if (!m_has_animation) {
		m_animation_force_timer = 100000;
		return false;
	}

	m_animation_force_timer = myrand_range(5, 100);

	// Cracks
	if (crack != m_last_crack) {
		for (auto &crack_material : m_crack_materials) {

			// TODO crack on animated tiles does not work
			auto anim_it = m_animation_info.find(crack_material.first);
			if (anim_it != m_animation_info.end())
				continue;

			scene::IMeshBuffer *buf = m_mesh[crack_material.first.first]->
				getMeshBuffer(crack_material.first.second);

			// Create new texture name from original
			std::string s = crack_material.second + itos(crack);
			u32 new_texture_id = 0;
			video::ITexture *new_texture =
					m_tsrc->getTextureForMesh(s, &new_texture_id);
			buf->getMaterial().setTexture(0, new_texture);
		}

		m_last_crack = crack;
	}

	// Texture animation
	for (auto &it : m_animation_info) {
		scene::IMeshBuffer *buf = m_mesh[it.first.first]->getMeshBuffer(it.first.second);
		video::SMaterial &material = buf->getMaterial();
		it.second.updateTexture(material, time);
	}

	return true;
}

void MapBlockMesh::updateTransparentBuffers(v3f camera_pos, v3s16 block_pos,
		bool group_by_buffers)
{
	// nothing to do if the entire block is opaque
	if (m_transparent_triangles.empty())
		return;

	v3f block_posf = intToFloat(block_pos * MAP_BLOCKSIZE, BS);
	v3f rel_camera_pos = camera_pos - block_posf;

	std::vector<s32> triangle_refs;
	m_bsp_tree.traverse(rel_camera_pos, triangle_refs);

	// arrange index sequences into partial buffers
	m_transparent_buffers_consolidated = false;
	m_transparent_buffers.clear();

	std::vector<std::pair<scene::SMeshBuffer *, std::vector<u16>>> ordered_strains;
	std::unordered_map<scene::SMeshBuffer *, size_t> strain_idxs;

	if (group_by_buffers) {
		// find (reversed) order for strains, by iterating front-to-back
		// (if a buffer A has a triangle nearer than all triangles of another
		// buffer B, A should be drawn in front of (=after) B)
		scene::SMeshBuffer *current_buffer = nullptr;
		for (auto it = triangle_refs.rbegin(); it != triangle_refs.rend(); ++it) {
			const auto &t = m_transparent_triangles[*it];
			if (current_buffer == t.buffer)
				continue;
			current_buffer = t.buffer;
			auto [_it2, is_new] =
				strain_idxs.emplace(current_buffer, ordered_strains.size());
			if (is_new)
				ordered_strains.emplace_back(current_buffer, std::vector<u16>{});
		}
	}

	// find order for triangles, by iterating back-to-front
	scene::SMeshBuffer *current_buffer = nullptr;
	std::vector<u16> *current_strain = nullptr;
	for (auto i : triangle_refs) {
		const auto &t = m_transparent_triangles[i];
		if (current_buffer != t.buffer) {
			current_buffer = t.buffer;
			if (group_by_buffers) {
				auto it = strain_idxs.find(current_buffer);
				assert(it != strain_idxs.end());
				current_strain = &ordered_strains[it->second].second;
			} else {
				ordered_strains.emplace_back(current_buffer, std::vector<u16>{});
				current_strain = &ordered_strains.back().second;
			}
		}
		current_strain->push_back(t.p1);
		current_strain->push_back(t.p2);
		current_strain->push_back(t.p3);
	}

	m_transparent_buffers.reserve(ordered_strains.size());
	if (group_by_buffers) {
		// the order was reversed
		for (auto it = ordered_strains.rbegin(); it != ordered_strains.rend(); ++it)
			m_transparent_buffers.emplace_back(it->first, std::move(it->second));
	} else {
		for (auto it = ordered_strains.begin(); it != ordered_strains.end(); ++it)
			m_transparent_buffers.emplace_back(it->first, std::move(it->second));
	}
}

void MapBlockMesh::consolidateTransparentBuffers()
{
	if (m_transparent_buffers_consolidated)
		return;
	m_transparent_buffers.clear();

	scene::SMeshBuffer *current_buffer = nullptr;
	std::vector<u16> current_strain;

	// use the fact that m_transparent_triangles is already arranged by buffer
	for (const auto &t : m_transparent_triangles) {
		if (current_buffer != t.buffer) {
			if (current_buffer != nullptr) {
				this->m_transparent_buffers.emplace_back(current_buffer, std::move(current_strain));
				current_strain.clear();
			}
			current_buffer = t.buffer;
		}
		current_strain.push_back(t.p1);
		current_strain.push_back(t.p2);
		current_strain.push_back(t.p3);
	}

	if (!current_strain.empty()) {
		this->m_transparent_buffers.emplace_back(current_buffer, std::move(current_strain));
	}

	m_transparent_buffers_consolidated = true;
}

video::SColor encode_light(u16 light, u8 emissive_light)
{
	// Get components
	u32 day = (light & 0xff);
	u32 night = (light >> 8);
	// Add emissive light
	night += emissive_light * 2.5f;
	if (night > 255)
		night = 255;
	// Since we don't know if the day light is sunlight or
	// artificial light, assume it is artificial when the night
	// light bank is also lit.
	if (day < night)
		day = 0;
	else
		day = day - night;
	u32 sum = day + night;
	// Ratio of sunlight:
	u32 r;
	if (sum > 0)
		r = day * 255 / sum;
	else
		r = 0;
	// Average light:
	float b = (day + night) / 2;
	return video::SColor(r, b, b, b);
}

u8 get_solid_sides(MeshMakeData *data)
{
	v3s16 blockpos_nodes = data->m_blockpos * MAP_BLOCKSIZE;
	const NodeDefManager *ndef = data->m_nodedef;

	const u16 side = data->m_side_length;
	assert(data->m_vmanip.m_area.contains(blockpos_nodes + v3s16(side - 1)));

	u8 result = 0x3F; // all sides solid
	for (s16 i = 0; i < side && result != 0; i++)
	for (s16 j = 0; j < side && result != 0; j++) {
		v3s16 positions[6] = {
			v3s16(0, i, j),
			v3s16(side - 1, i, j),
			v3s16(i, 0, j),
			v3s16(i, side - 1, j),
			v3s16(i, j, 0),
			v3s16(i, j, side - 1)
		};

		for (u8 k = 0; k < 6; k++) {
			const MapNode &top = data->m_vmanip.getNodeRefUnsafe(blockpos_nodes + positions[k]);
			if (ndef->get(top).solidness != 2)
				result &= ~(1 << k);
		}
	}
	return result;
}
