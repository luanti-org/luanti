// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <cmath>
#include "content_mapblock.h"
#include "util/basic_macros.h"
#include "util/numeric.h"
#include "util/directiontables.h"
#include "util/tracy_wrapper.h"
#include "mapblock_mesh.h"
#include "settings.h"
#include "nodedef.h"
#include "client/tile.h"
#include "mesh.h"
#include "client/meshgen/collector.h"
#include "client/renderingengine.h"
#include "client.h"
#include "noise.h"
#include <SMesh.h>
#include <IMeshBuffer.h>

// Distance of light extrapolation (for oversized nodes)
// After this distance, it gives up and considers light level constant
#define SMOOTH_LIGHTING_OVERSIZE 1.0

// Node edge count (for glasslike-framed)
#define FRAMED_EDGE_COUNT 12

// Node neighbor count, including edge-connected, but not vertex-connected
// (for glasslike-framed)
// Corresponding offsets are listed in g_27dirs
#define FRAMED_NEIGHBOR_COUNT 18

// Maps light index to corner direction
static const v3s16 light_dirs[8] = {
	v3s16(-1, -1, -1),
	v3s16(-1, -1,  1),
	v3s16(-1,  1, -1),
	v3s16(-1,  1,  1),
	v3s16( 1, -1, -1),
	v3s16( 1, -1,  1),
	v3s16( 1,  1, -1),
	v3s16( 1,  1,  1),
};

// Maps cuboid face and vertex indices to the corresponding light index
static const u8 light_indices[6][4] = {
	{3, 7, 6, 2},
	{0, 4, 5, 1},
	{6, 7, 5, 4},
	{3, 2, 0, 1},
	{7, 3, 1, 5},
	{2, 6, 4, 0},
};

// Standard index set to make a quad on 4 vertices
static constexpr u16 quad_indices_02[] = {0, 1, 2, 2, 3, 0};
static constexpr u16 quad_indices_13[] = {0, 1, 3, 3, 1, 2};
static const auto &quad_indices = quad_indices_02;

const std::string MapblockMeshGenerator::raillike_groupname = "connect_to_raillike";

MapblockMeshGenerator::MapblockMeshGenerator(MeshMakeData *input, MeshCollector *output):
	data(input),
	collector(output),
	nodedef(data->m_nodedef),
	blockpos_nodes(data->m_blockpos * MAP_BLOCKSIZE)
{
}

void MapblockMeshGenerator::useTile(TileSpec *tile_ret, int index, u8 set_flags,
		u8 reset_flags, bool special)
{
	if (special)
		getSpecialTile(index, tile_ret, cur_node.p == data->m_crack_pos_relative);
	else
		getTile(index, tile_ret);

	for (auto &layer : tile_ret->layers) {
		layer.material_flags |= set_flags;
		layer.material_flags &= ~reset_flags;
	}
}

// Returns a tile, ready for use, non-rotated.
void MapblockMeshGenerator::getTile(int index, TileSpec *tile_ret)
{
	getNodeTileN(cur_node.n, cur_node.p, index, data, *tile_ret);
}

// Returns a tile, ready for use, rotated according to the node facedir.
void MapblockMeshGenerator::getTile(v3s16 direction, TileSpec *tile_ret)
{
	getNodeTile(cur_node.n, cur_node.p, direction, data, *tile_ret);
}

// Returns a special tile, ready for use, non-rotated.
void MapblockMeshGenerator::getSpecialTile(int index, TileSpec *tile_ret, bool apply_crack)
{
	*tile_ret = cur_node.f->special_tiles[index];
	TileLayer *top_layer = nullptr;

	for (auto &layernum : tile_ret->layers) {
		TileLayer *layer = &layernum;
		if (layer->empty())
			continue;
		top_layer = layer;
		if (!layer->has_color)
			cur_node.n.getColor(*cur_node.f, &layer->color);
	}

	if (apply_crack)
		top_layer->material_flags |= MATERIAL_FLAG_CRACK;
}

void MapblockMeshGenerator::drawQuad(const TileSpec &tile, v3f *coords, const v3s16 &normal,
	float vertical_tiling)
{
	const v2f tcoords[4] = {v2f(0.0, 0.0), v2f(1.0, 0.0),
		v2f(1.0, vertical_tiling), v2f(0.0, vertical_tiling)};
	video::S3DVertex vertices[4];
	bool shade_face = !cur_node.f->light_source && (normal != v3s16(0, 0, 0));
	v3f normal2 = v3f::from(normal);
	for (int j = 0; j < 4; j++) {
		vertices[j].Pos = coords[j] + cur_node.origin;
		vertices[j].Normal = normal2;
		if (data->m_smooth_lighting)
			vertices[j].Color = blendLightColor(coords[j]);
		else
			vertices[j].Color = cur_node.lcolor;
		if (shade_face)
			applyFacesShading(vertices[j].Color, normal2);
		vertices[j].TCoords = tcoords[j];
	}
	collector->append(tile, vertices, 4, quad_indices, 6);
}

static std::array<video::S3DVertex, 24> setupCuboidVertices(const aabb3f &box,
		const f32 *txc, const TileSpec *tiles, int tilecount, v3s16 alignment)
{
	v3f min = box.MinEdge;
	v3f max = box.MaxEdge;

	// Texture coords are [0,1] if not specified otherwise
	f32 uniform_txc[24];
	if (!txc) {
		for (int i = 0; i != 24; ++i) {
			uniform_txc[i] = (i % 4 < 2) ? 0.0f : 1.0f;
		}
		txc = uniform_txc;
	}

	std::array<video::S3DVertex, 24> vertices = {{
		// top
		video::S3DVertex(min.X, max.Y, max.Z, 0, 1, 0, {}, txc[0], txc[1]),
		video::S3DVertex(max.X, max.Y, max.Z, 0, 1, 0, {}, txc[2], txc[1]),
		video::S3DVertex(max.X, max.Y, min.Z, 0, 1, 0, {}, txc[2], txc[3]),
		video::S3DVertex(min.X, max.Y, min.Z, 0, 1, 0, {}, txc[0], txc[3]),
		// bottom
		video::S3DVertex(min.X, min.Y, min.Z, 0, -1, 0, {}, txc[4], txc[5]),
		video::S3DVertex(max.X, min.Y, min.Z, 0, -1, 0, {}, txc[6], txc[5]),
		video::S3DVertex(max.X, min.Y, max.Z, 0, -1, 0, {}, txc[6], txc[7]),
		video::S3DVertex(min.X, min.Y, max.Z, 0, -1, 0, {}, txc[4], txc[7]),
		// right
		video::S3DVertex(max.X, max.Y, min.Z, 1, 0, 0, {}, txc[ 8], txc[9]),
		video::S3DVertex(max.X, max.Y, max.Z, 1, 0, 0, {}, txc[10], txc[9]),
		video::S3DVertex(max.X, min.Y, max.Z, 1, 0, 0, {}, txc[10], txc[11]),
		video::S3DVertex(max.X, min.Y, min.Z, 1, 0, 0, {}, txc[ 8], txc[11]),
		// left
		video::S3DVertex(min.X, max.Y, max.Z, -1, 0, 0, {}, txc[12], txc[13]),
		video::S3DVertex(min.X, max.Y, min.Z, -1, 0, 0, {}, txc[14], txc[13]),
		video::S3DVertex(min.X, min.Y, min.Z, -1, 0, 0, {}, txc[14], txc[15]),
		video::S3DVertex(min.X, min.Y, max.Z, -1, 0, 0, {}, txc[12], txc[15]),
		// back
		video::S3DVertex(max.X, max.Y, max.Z, 0, 0, 1, {}, txc[16], txc[17]),
		video::S3DVertex(min.X, max.Y, max.Z, 0, 0, 1, {}, txc[18], txc[17]),
		video::S3DVertex(min.X, min.Y, max.Z, 0, 0, 1, {}, txc[18], txc[19]),
		video::S3DVertex(max.X, min.Y, max.Z, 0, 0, 1, {}, txc[16], txc[19]),
		// front
		video::S3DVertex(min.X, max.Y, min.Z, 0, 0, -1, {}, txc[20], txc[21]),
		video::S3DVertex(max.X, max.Y, min.Z, 0, 0, -1, {}, txc[22], txc[21]),
		video::S3DVertex(max.X, min.Y, min.Z, 0, 0, -1, {}, txc[22], txc[23]),
		video::S3DVertex(min.X, min.Y, min.Z, 0, 0, -1, {}, txc[20], txc[23]),
	}};

	for (int face = 0; face < 6; face++) {
		int tileindex = MYMIN(face, tilecount - 1);
		const TileSpec &tile = tiles[tileindex];
		for (int j = 0; j < 4; j++) {
			video::S3DVertex &vertex = vertices[face * 4 + j];
			v2f &tcoords = vertex.TCoords;
			switch (tile.rotation) {
			case TileRotation::None:
				break;
			case TileRotation::R90:
				tcoords.set(1 - tcoords.Y, tcoords.X);
				break;
			case TileRotation::R180:
				tcoords.set(1 - tcoords.X, 1 - tcoords.Y);
				break;
			case TileRotation::R270:
				tcoords.set(tcoords.Y, 1 - tcoords.X);
				break;
			}

			if (tile.world_aligned) {
				// Maps uv dimension of every face to world dimension xyz
				constexpr int coord_dim[12] = {
					0, 2, // up
					0, 2, // down
					2, 1, // right
					2, 1, // left
					0, 1, // back
					0, 1, // front
				};

				auto scale = tile.layers[0].scale;
				f32 scale_factor = 1.0f / scale;

				float x = alignment[coord_dim[face*2]] % scale;
				float y = alignment[coord_dim[face*2 + 1]] % scale;

				// Faces grow in different directions
				if (face != 1) {
					y = tcoords.Y + ((scale-1)-y);
				} else {
					y = tcoords.Y + y;
				}
				if (face == 3 || face == 4) {
					x = tcoords.X + ((scale-1)-x);
				} else {
					x = tcoords.X + x;
				}

				tcoords.set(x * scale_factor, y * scale_factor);
			}
		}
	}

	return vertices;
}

enum class QuadDiagonal {
	Diag02,
	Diag13,
};

// Create a cuboid with custom lighting.
//  tiles     - the tiles (materials) to use (for all 6 faces)
//  tilecount - number of entries in tiles, 1<=tilecount<=6
//  txc       - texture coordinates - this is a list of texture coordinates
//              for the opposite corners of each face - therefore, there
//              should be (2+2)*6=24 values in the list. The order of
//              the faces in the list is up-down-right-left-back-front
//              if nullptr use standard [0,1] coords
//              (compatible with ContentFeatures).
//  mask      - a bit mask that suppresses drawing of tiles.
//              tile i will not be drawn if mask & (1 << i) is 1
//  face_lighter(int face, video::S3DVertex vertices[4]) -> QuadDiagonal -
//              a callback that will be called for each face drawn to setup vertex colors,
//              and to choose diagonal to split the quad at.
template <typename Fn>
void MapblockMeshGenerator::drawCuboid(const aabb3f &box,
		const TileSpec *tiles, int tilecount, const f32 *txc, u8 mask, Fn &&face_lighter)
{
	assert(tilecount >= 1 && tilecount <= 6); // pre-condition

	auto vertices = setupCuboidVertices(box, txc, tiles, tilecount, cur_node.p);

	for (int k = 0; k < 6; ++k) {
		if (mask & (1 << k))
			continue;
		QuadDiagonal diagonal = face_lighter(k, &vertices[4 * k]);
		const u16 *indices = diagonal == QuadDiagonal::Diag13 ? quad_indices_13 : quad_indices_02;
		int tileindex = MYMIN(k, tilecount - 1);
		collector->append(tiles[tileindex], &vertices[4 * k], 4, indices, 6);
	}
}

// Gets the base lighting values for a node
void MapblockMeshGenerator::getSmoothLightFrame()
{
	for (int k = 0; k < 8; ++k)
		cur_node.lframe.sunlight[k] = false;
	for (int k = 0; k < 8; ++k) {
		LightPair light(getSmoothLightTransparent(blockpos_nodes + cur_node.p, light_dirs[k], data));
		cur_node.lframe.lightsDay[k] = light.lightDay;
		cur_node.lframe.lightsNight[k] = light.lightNight;
		// If there is direct sunlight and no ambient occlusion at some corner,
		// mark the vertical edge (top and bottom corners) containing it.
		if (light.lightDay == 255) {
			cur_node.lframe.sunlight[k] = true;
			cur_node.lframe.sunlight[k ^ 2] = true;
		}
	}
}

// Calculates vertex light level
//  vertex_pos - vertex position in the node (coordinates are clamped to [0.0, 1.0] or so)
LightInfo MapblockMeshGenerator::blendLight(const v3f &vertex_pos)
{
	// Light levels at (logical) node corners are known. Here,
	// trilinear interpolation is used to calculate light level
	// at a given point in the node.
	f32 x = core::clamp(vertex_pos.X / BS + 0.5, 0.0 - SMOOTH_LIGHTING_OVERSIZE, 1.0 + SMOOTH_LIGHTING_OVERSIZE);
	f32 y = core::clamp(vertex_pos.Y / BS + 0.5, 0.0 - SMOOTH_LIGHTING_OVERSIZE, 1.0 + SMOOTH_LIGHTING_OVERSIZE);
	f32 z = core::clamp(vertex_pos.Z / BS + 0.5, 0.0 - SMOOTH_LIGHTING_OVERSIZE, 1.0 + SMOOTH_LIGHTING_OVERSIZE);
	f32 lightDay = 0.0; // daylight
	f32 lightNight = 0.0;
	f32 lightBoosted = 0.0; // daylight + direct sunlight, if any
	for (int k = 0; k < 8; ++k) {
		f32 dx = (k & 4) ? x : 1 - x;
		f32 dy = (k & 2) ? y : 1 - y;
		f32 dz = (k & 1) ? z : 1 - z;
		// Use direct sunlight (255), if any; use daylight otherwise.
		f32 light_boosted = cur_node.lframe.sunlight[k] ? 255 : cur_node.lframe.lightsDay[k];
		lightDay += dx * dy * dz * cur_node.lframe.lightsDay[k];
		lightNight += dx * dy * dz * cur_node.lframe.lightsNight[k];
		lightBoosted += dx * dy * dz * light_boosted;
	}
	return LightInfo{lightDay, lightNight, lightBoosted};
}

// Calculates vertex color to be used in mapblock mesh
//  vertex_pos - vertex position in the node (coordinates are clamped to [0.0, 1.0] or so)
video::SColor MapblockMeshGenerator::blendLightColor(const v3f &vertex_pos)
{
	LightInfo light = blendLight(vertex_pos);
	return encode_light(light.getPair(), cur_node.f->light_source);
}

video::SColor MapblockMeshGenerator::blendLightColor(const v3f &vertex_pos,
	const v3f &vertex_normal)
{
	LightInfo light = blendLight(vertex_pos);
	video::SColor color = encode_light(light.getPair(MYMAX(0.0f, vertex_normal.Y)),
			cur_node.f->light_source);
	if (!cur_node.f->light_source)
		applyFacesShading(color, vertex_normal);
	return color;
}

void MapblockMeshGenerator::generateCuboidTextureCoords(const aabb3f &box, f32 *coords)
{
	// Generate texture coords which are aligned to coords of a solid nodes
	f32 tx1 = (box.MinEdge.X / BS) + 0.5f;
	f32 ty1 = (box.MinEdge.Y / BS) + 0.5f;
	f32 tz1 = (box.MinEdge.Z / BS) + 0.5f;
	f32 tx2 = (box.MaxEdge.X / BS) + 0.5f;
	f32 ty2 = (box.MaxEdge.Y / BS) + 0.5f;
	f32 tz2 = (box.MaxEdge.Z / BS) + 0.5f;
	f32 txc[24] = {
		    tx1, 1 - tz2,     tx2, 1 - tz1, // up
		    tx1,     tz1,     tx2,     tz2, // down
		    tz1, 1 - ty2,     tz2, 1 - ty1, // right
		1 - tz2, 1 - ty2, 1 - tz1, 1 - ty1, // left
		1 - tx2, 1 - ty2, 1 - tx1, 1 - ty1, // back
		    tx1, 1 - ty2,     tx2, 1 - ty1, // front
	};
	for (int i = 0; i != 24; ++i)
		coords[i] = txc[i];
}

static inline int lightDiff(LightPair a, LightPair b)
{
	return abs(a.lightDay - b.lightDay) + abs(a.lightNight - b.lightNight);
}

void MapblockMeshGenerator::drawAutoLightedCuboid(aabb3f box, const TileSpec &tile,
		const f32 *txc, u8 mask)
{
	drawAutoLightedCuboid(box, &tile, 1, txc, mask);
}

void MapblockMeshGenerator::drawAutoLightedCuboid(aabb3f box,
		const TileSpec *tiles, int tile_count, const f32 *txc, u8 mask)
{
	bool scale = std::fabs(cur_node.f->visual_scale - 1.0f) > 1e-3f;
	f32 dx1 = box.MinEdge.X;
	f32 dy1 = box.MinEdge.Y;
	f32 dz1 = box.MinEdge.Z;
	f32 dx2 = box.MaxEdge.X;
	f32 dy2 = box.MaxEdge.Y;
	f32 dz2 = box.MaxEdge.Z;
	if (scale) {
		box.MinEdge *= cur_node.f->visual_scale;
		box.MaxEdge *= cur_node.f->visual_scale;
	}
	box.MinEdge += cur_node.origin;
	box.MaxEdge += cur_node.origin;
	if (data->m_smooth_lighting) {
		LightInfo lights[8];
		for (int j = 0; j < 8; ++j) {
			v3f d;
			d.X = (j & 4) ? dx2 : dx1;
			d.Y = (j & 2) ? dy2 : dy1;
			d.Z = (j & 1) ? dz2 : dz1;
			lights[j] = blendLight(d);
		}

		drawCuboid(box, tiles, tile_count, txc, mask, [&] (int face, video::S3DVertex vertices[4]) {
			LightPair final_lights[4];
			for (int j = 0; j < 4; j++) {
				video::S3DVertex &vertex = vertices[j];
				final_lights[j] = lights[light_indices[face][j]].getPair(MYMAX(0.0f, vertex.Normal.Y));
				vertex.Color = encode_light(final_lights[j], cur_node.f->light_source);
				if (!cur_node.f->light_source)
					applyFacesShading(vertex.Color, vertex.Normal);
			}
			if (lightDiff(final_lights[1], final_lights[3]) < lightDiff(final_lights[0], final_lights[2]))
				return QuadDiagonal::Diag13;
			return QuadDiagonal::Diag02;
		});
	} else {
		drawCuboid(box, tiles, tile_count, txc, mask, [&] (int face, video::S3DVertex vertices[4]) {
			video::SColor color = cur_node.lcolor;
			if (!cur_node.f->light_source)
				applyFacesShading(color, vertices[0].Normal);
			for (int j = 0; j < 4; j++) {
				video::S3DVertex &vertex = vertices[j];
				vertex.Color = color;
			}
			return QuadDiagonal::Diag02;
		});
	}
}

void MapblockMeshGenerator::drawSolidNode()
{
	u8 faces = 0; // k-th bit will be set if k-th face is to be drawn.
	static const v3s16 tile_dirs[6] = {
		v3s16(0, 1, 0),
		v3s16(0, -1, 0),
		v3s16(1, 0, 0),
		v3s16(-1, 0, 0),
		v3s16(0, 0, 1),
		v3s16(0, 0, -1)
	};
	TileSpec tiles[6];
	u16 lights[6];
	content_t n1 = cur_node.n.getContent();
	for (int face = 0; face < 6; face++) {
		v3s16 p2 = blockpos_nodes + cur_node.p + tile_dirs[face];
		MapNode neighbor = data->m_vmanip.getNodeNoEx(p2);
		content_t n2 = neighbor.getContent();
		bool backface_culling = cur_node.f->drawtype == NDT_NORMAL;
		if (n2 == n1)
			continue;
		if (n2 == CONTENT_IGNORE)
			continue;
		if (n2 != CONTENT_AIR) {
			const ContentFeatures &f2 = nodedef->get(n2);
			if (f2.solidness == 2)
				continue;
			if (cur_node.f->drawtype == NDT_LIQUID) {
				if (cur_node.f->sameLiquidRender(f2))
					continue;
				backface_culling = f2.solidness || f2.visual_solidness;
			}
		}
		faces |= 1 << face;
		getTile(tile_dirs[face], &tiles[face]);
		for (auto &layer : tiles[face].layers) {
			if (backface_culling)
				layer.material_flags |= MATERIAL_FLAG_BACKFACE_CULLING;
		}
		if (!data->m_smooth_lighting) {
			lights[face] = getFaceLight(cur_node.n, neighbor, nodedef);
		}
	}
	if (!faces)
		return;
	u8 mask = faces ^ 0b0011'1111; // k-th bit is set if k-th face is to be *omitted*, as expected by cuboid drawing functions.
	auto box = aabb3f(v3f(-0.5 * BS), v3f(0.5 * BS));
	box.MinEdge += cur_node.origin;
	box.MaxEdge += cur_node.origin;
	if (data->m_smooth_lighting) {
		LightPair lights[6][4];
		for (int face = 0; face < 6; ++face) {
			if (mask & (1 << face))
				continue;
			for (int k = 0; k < 4; k++) {
				v3s16 corner = light_dirs[light_indices[face][k]];
				lights[face][k] = LightPair(getSmoothLightSolid(
						blockpos_nodes + cur_node.p, tile_dirs[face], corner, data));
			}
		}

		drawCuboid(box, tiles, 6, nullptr, mask, [&] (int face, video::S3DVertex vertices[4]) {
			auto final_lights = lights[face];
			for (int j = 0; j < 4; j++) {
				video::S3DVertex &vertex = vertices[j];
				vertex.Color = encode_light(final_lights[j], cur_node.f->light_source);
				if (!cur_node.f->light_source)
					applyFacesShading(vertex.Color, vertex.Normal);
			}
			if (lightDiff(final_lights[1], final_lights[3]) < lightDiff(final_lights[0], final_lights[2]))
				return QuadDiagonal::Diag13;
			return QuadDiagonal::Diag02;
		});
	} else {
		drawCuboid(box, tiles, 6, nullptr, mask, [&] (int face, video::S3DVertex vertices[4]) {
			video::SColor color = encode_light(lights[face], cur_node.f->light_source);
			if (!cur_node.f->light_source)
				applyFacesShading(color, vertices[0].Normal);
			for (int j = 0; j < 4; j++) {
				video::S3DVertex &vertex = vertices[j];
				vertex.Color = color;
			}
			return QuadDiagonal::Diag02;
		});
	}
}

u8 MapblockMeshGenerator::getNodeBoxMask(aabb3f box, u8 solid_neighbors, u8 sametype_neighbors) const
{
	const f32 NODE_BOUNDARY = 0.5 * BS;

	// For an oversized nodebox, return immediately
	if (box.MaxEdge.X > NODE_BOUNDARY ||
			box.MinEdge.X < -NODE_BOUNDARY ||
			box.MaxEdge.Y >  NODE_BOUNDARY ||
			box.MinEdge.Y < -NODE_BOUNDARY ||
			box.MaxEdge.Z >  NODE_BOUNDARY ||
			box.MinEdge.Z < -NODE_BOUNDARY)
		return 0;

	// We can skip faces at node boundary if the matching neighbor is solid
	u8 solid_mask =
			(box.MaxEdge.Y == NODE_BOUNDARY  ?  1 : 0) |
			(box.MinEdge.Y == -NODE_BOUNDARY ?  2 : 0) |
			(box.MaxEdge.X == NODE_BOUNDARY  ?  4 : 0) |
			(box.MinEdge.X == -NODE_BOUNDARY ?  8 : 0) |
			(box.MaxEdge.Z == NODE_BOUNDARY  ? 16 : 0) |
			(box.MinEdge.Z == -NODE_BOUNDARY ? 32 : 0);

	u8 sametype_mask = 0;
	if (cur_node.f->alpha == AlphaMode::ALPHAMODE_OPAQUE) {
		// In opaque nodeboxes, faces on opposite sides can cancel
		// each other out if there is a matching neighbor of the same type
		sametype_mask =
				((solid_mask & 3) == 3 ? 3 : 0) |
				((solid_mask & 12) == 12 ? 12 : 0) |
				((solid_mask & 48) == 48 ? 48 : 0);
	}

	// Combine masks with actual neighbors to get the faces to be skipped
	return (solid_mask & solid_neighbors) | (sametype_mask & sametype_neighbors);
}


void MapblockMeshGenerator::prepareLiquidNodeDrawing()
{
	getSpecialTile(0, &cur_liquid.tile_top);
	getSpecialTile(1, &cur_liquid.tile);

	MapNode ntop    = data->m_vmanip.getNodeNoEx(blockpos_nodes + cur_node.p + v3s16(0,  1, 0));
	MapNode nbottom = data->m_vmanip.getNodeNoEx(blockpos_nodes + cur_node.p + v3s16(0, -1, 0));
	cur_liquid.c_flowing = cur_node.f->liquid_alternative_flowing_id;
	cur_liquid.c_source = cur_node.f->liquid_alternative_source_id;
	cur_liquid.top_is_same_liquid = (ntop.getContent() == cur_liquid.c_flowing)
			|| (ntop.getContent() == cur_liquid.c_source);
	cur_liquid.draw_bottom = (nbottom.getContent() != cur_liquid.c_flowing)
			&& (nbottom.getContent() != cur_liquid.c_source);
	if (cur_liquid.draw_bottom) {
		const ContentFeatures &f2 = nodedef->get(nbottom.getContent());
		if (f2.solidness > 1)
			cur_liquid.draw_bottom = false;
	}

	if (data->m_smooth_lighting)
		return; // don't need to pre-compute anything in this case

	auto light = LightPair(getInteriorLight(cur_node.n, 0, nodedef));
	if (cur_node.f->light_source != 0) {
		// If this liquid emits light and doesn't contain light, draw
		// it at what it emits, for an increased effect
		u8 e = decode_light(cur_node.f->light_source);
		light = LightPair(std::max(e, light.lightDay),
				std::max(e, light.lightNight));
	} else if (nodedef->getLightingFlags(ntop).has_light) {
		// Otherwise, use the light of the node on top if possible
		light = LightPair(getInteriorLight(ntop, 0, nodedef));
	}

	cur_liquid.color_top = encode_light(light, cur_node.f->light_source);
	cur_node.lcolor = encode_light(light, cur_node.f->light_source);
}

void MapblockMeshGenerator::getLiquidNeighborhood()
{
	u8 range = rangelim(nodedef->get(cur_liquid.c_flowing).liquid_range, 1, 8);

	for (int w = -1; w <= 1; w++)
	for (int u = -1; u <= 1; u++) {
		LiquidData::NeighborData &neighbor = cur_liquid.neighbors[w + 1][u + 1];
		v3s16 p2 = cur_node.p + v3s16(u, 0, w);
		MapNode n2 = data->m_vmanip.getNodeNoEx(blockpos_nodes + p2);
		neighbor.content = n2.getContent();
		neighbor.level = -0.5f;
		neighbor.is_same_liquid = false;
		neighbor.top_is_same_liquid = false;

		if (neighbor.content == CONTENT_IGNORE)
			continue;

		if (neighbor.content == cur_liquid.c_source) {
			neighbor.is_same_liquid = true;
			neighbor.level = 0.5f;
		} else if (neighbor.content == cur_liquid.c_flowing) {
			neighbor.is_same_liquid = true;
			u8 liquid_level = (n2.param2 & LIQUID_LEVEL_MASK);
			if (liquid_level <= LIQUID_LEVEL_MAX + 1 - range)
				liquid_level = 0;
			else
				liquid_level -= (LIQUID_LEVEL_MAX + 1 - range);
			neighbor.level = (-0.5f + (liquid_level + 0.5f) / range);
		}

		// Check node above neighbor.
		// NOTE: This doesn't get executed if neighbor
		//       doesn't exist
		p2.Y++;
		n2 = data->m_vmanip.getNodeNoEx(blockpos_nodes + p2);
		if (n2.getContent() == cur_liquid.c_source || n2.getContent() == cur_liquid.c_flowing)
			neighbor.top_is_same_liquid = true;
	}
}

void MapblockMeshGenerator::calculateCornerLevels()
{
	for (int k = 0; k < 2; k++)
	for (int i = 0; i < 2; i++)
		cur_liquid.corner_levels[k][i] = getCornerLevel(i, k);
}

f32 MapblockMeshGenerator::getCornerLevel(int i, int k) const
{
	float sum = 0;
	int count = 0;
	int air_count = 0;
	for (int dk = 0; dk < 2; dk++)
	for (int di = 0; di < 2; di++) {
		const LiquidData::NeighborData &neighbor_data = cur_liquid.neighbors[k + dk][i + di];
		content_t content = neighbor_data.content;

		// If top is liquid, draw starting from top of node
		if (neighbor_data.top_is_same_liquid)
			return 0.5f;

		// Source always has the full height
		if (content == cur_liquid.c_source)
			return 0.5f;

		// Flowing liquid has level information
		if (content == cur_liquid.c_flowing) {
			sum += neighbor_data.level;
			count++;
		} else if (content == CONTENT_AIR) {
			air_count++;
		}
	}
	if (air_count >= 2)
		return -0.5f + 0.2f / BS;
	if (count > 0)
		return sum / count;
	return 0;
}

namespace {
	struct LiquidFaceDesc {
		v3s16 dir; // XZ
		v3s16 p[2]; // XZ only; 1 means +, 0 means -
	};
	struct UV {
		int u, v;
	};
	static const LiquidFaceDesc liquid_base_faces[4] = {
		{v3s16( 1, 0,  0), {v3s16(1, 0, 1), v3s16(1, 0, 0)}},
		{v3s16(-1, 0,  0), {v3s16(0, 0, 0), v3s16(0, 0, 1)}},
		{v3s16( 0, 0,  1), {v3s16(0, 0, 1), v3s16(1, 0, 1)}},
		{v3s16( 0, 0, -1), {v3s16(1, 0, 0), v3s16(0, 0, 0)}},
	};
	static const UV liquid_base_vertices[4] = {
		{0, 1},
		{1, 1},
		{1, 0},
		{0, 0}
	};
}

void MapblockMeshGenerator::drawLiquidSides()
{
	for (const auto &face : liquid_base_faces) {
		const LiquidData::NeighborData &neighbor = cur_liquid.neighbors[face.dir.Z + 1][face.dir.X + 1];

		// No face between nodes of the same liquid, unless there is node
		// at the top to which it should be connected. Again, unless the face
		// there would be inside the liquid
		if (neighbor.is_same_liquid) {
			if (!cur_liquid.top_is_same_liquid)
				continue;
			if (neighbor.top_is_same_liquid)
				continue;
		}

		const ContentFeatures &neighbor_features = nodedef->get(neighbor.content);
		// Don't draw face if neighbor is blocking the view
		if (neighbor_features.solidness == 2)
			continue;

		video::S3DVertex vertices[4];
		for (int j = 0; j < 4; j++) {
			const UV &vertex = liquid_base_vertices[j];
			const v3s16 &base = face.p[vertex.u];
			float v = vertex.v;

			v3f pos;
			pos.X = (base.X - 0.5f) * BS;
			pos.Z = (base.Z - 0.5f) * BS;
			if (vertex.v) {
				pos.Y = (neighbor.is_same_liquid ? cur_liquid.corner_levels[base.Z][base.X] : -0.5f) * BS;
			} else if (cur_liquid.top_is_same_liquid) {
				pos.Y = 0.5f * BS;
			} else {
				pos.Y = cur_liquid.corner_levels[base.Z][base.X] * BS;
				v += 0.5f - cur_liquid.corner_levels[base.Z][base.X];
			}

			video::SColor color;
			if (data->m_smooth_lighting)
				color = blendLightColor(pos);
			else
				color = cur_node.lcolor;

			pos += cur_node.origin;

			vertices[j] = video::S3DVertex(pos.X, pos.Y, pos.Z,
					face.dir.X, face.dir.Y, face.dir.Z,
					color,
					vertex.u, v);
		};
		collector->append(cur_liquid.tile, vertices, 4, quad_indices, 6);
	}
}

void MapblockMeshGenerator::drawLiquidTop()
{
	// To get backface culling right, the vertices need to go
	// clockwise around the front of the face. And we happened to
	// calculate corner levels in exact reverse order.
	static const int corner_resolve[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

	video::S3DVertex vertices[4] = {
		video::S3DVertex(-BS / 2, 0,  BS / 2, 0, 0, 0, cur_liquid.color_top, 0, 1),
		video::S3DVertex( BS / 2, 0,  BS / 2, 0, 0, 0, cur_liquid.color_top, 1, 1),
		video::S3DVertex( BS / 2, 0, -BS / 2, 0, 0, 0, cur_liquid.color_top, 1, 0),
		video::S3DVertex(-BS / 2, 0, -BS / 2, 0, 0, 0, cur_liquid.color_top, 0, 0),
	};

	for (int i = 0; i < 4; i++) {
		int u = corner_resolve[i][0];
		int w = corner_resolve[i][1];

		if (data->m_enable_water_reflections) {
			int x = vertices[i].Pos.X > 0;
			int z = vertices[i].Pos.Z > 0;

			f32 dx = 0.5f * (cur_liquid.neighbors[z][x].level - cur_liquid.neighbors[z][x + 1].level +
				cur_liquid.neighbors[z + 1][x].level - cur_liquid.neighbors[z + 1][x + 1].level);
			f32 dz = 0.5f * (cur_liquid.neighbors[z][x].level - cur_liquid.neighbors[z + 1][x].level +
				cur_liquid.neighbors[z][x + 1].level - cur_liquid.neighbors[z + 1][x + 1].level);

			vertices[i].Normal = v3f(dx, 1., dz).normalize();
		}

		vertices[i].Pos.Y += cur_liquid.corner_levels[w][u] * BS;
		if (data->m_smooth_lighting)
			vertices[i].Color = blendLightColor(vertices[i].Pos);
		vertices[i].Pos += cur_node.origin;
	}

	// Default downwards-flowing texture animation goes from
	// -Z towards +Z, thus the direction is +Z.
	// Rotate texture to make animation go in flow direction
	// Positive if liquid moves towards +Z
	f32 dz = (cur_liquid.corner_levels[0][0] + cur_liquid.corner_levels[0][1]) -
	         (cur_liquid.corner_levels[1][0] + cur_liquid.corner_levels[1][1]);
	// Positive if liquid moves towards +X
	f32 dx = (cur_liquid.corner_levels[0][0] + cur_liquid.corner_levels[1][0]) -
	         (cur_liquid.corner_levels[0][1] + cur_liquid.corner_levels[1][1]);
	v2f tcoord_center(0.5, 0.5);
	v2f tcoord_translate(blockpos_nodes.Z + cur_node.p.Z,
			blockpos_nodes.X + cur_node.p.X);
	v2f dir = v2f(dx, dz).normalize();
	if (dir == v2f{0.0f, 0.0f}) // if corners are symmetrical
		dir = v2f{1.0f, 0.0f};

	// Rotate tcoord_translate around the origin. The X axis turns to dir.
	tcoord_translate.set(
		dir.X * tcoord_translate.X - dir.Y * tcoord_translate.Y,
		dir.Y * tcoord_translate.X + dir.X * tcoord_translate.Y);

	tcoord_translate.X -= floor(tcoord_translate.X);
	tcoord_translate.Y -= floor(tcoord_translate.Y);

	for (video::S3DVertex &vertex : vertices) {
		// Rotate vertex.TCoords around tcoord_center. The X axis turns to dir.
		vertex.TCoords -= tcoord_center;
		vertex.TCoords.set(
			dir.X * vertex.TCoords.X - dir.Y * vertex.TCoords.Y,
			dir.Y * vertex.TCoords.X + dir.X * vertex.TCoords.Y);
		vertex.TCoords += tcoord_center;

		vertex.TCoords += tcoord_translate;

		if (!data->m_enable_water_reflections) {
			vertex.Normal = v3f(dx, 1., dz).normalize();
		}
	}

	std::swap(vertices[0].TCoords, vertices[2].TCoords);

	collector->append(cur_liquid.tile_top, vertices, 4, quad_indices, 6);
}

void MapblockMeshGenerator::drawLiquidBottom()
{
	video::S3DVertex vertices[4] = {
		video::S3DVertex(-BS / 2, -BS / 2, -BS / 2, 0, 0, 0, cur_liquid.color_top, 0, 0),
		video::S3DVertex( BS / 2, -BS / 2, -BS / 2, 0, 0, 0, cur_liquid.color_top, 1, 0),
		video::S3DVertex( BS / 2, -BS / 2,  BS / 2, 0, 0, 0, cur_liquid.color_top, 1, 1),
		video::S3DVertex(-BS / 2, -BS / 2,  BS / 2, 0, 0, 0, cur_liquid.color_top, 0, 1),
	};

	for (int i = 0; i < 4; i++) {
		if (data->m_smooth_lighting)
			vertices[i].Color = blendLightColor(vertices[i].Pos);
		vertices[i].Pos += cur_node.origin;
	}

	collector->append(cur_liquid.tile_top, vertices, 4, quad_indices, 6);
}

void MapblockMeshGenerator::drawLiquidNode()
{
	prepareLiquidNodeDrawing();
	getLiquidNeighborhood();
	calculateCornerLevels();
	drawLiquidSides();
	if (!cur_liquid.top_is_same_liquid)
		drawLiquidTop();
	if (cur_liquid.draw_bottom)
		drawLiquidBottom();
}

void MapblockMeshGenerator::drawGlasslikeNode()
{
	TileSpec tile;
	useTile(&tile, 0, 0, 0);

	for (int face = 0; face < 6; face++) {
		// Check this neighbor
		v3s16 dir = g_6dirs[face];
		v3s16 neighbor_pos = blockpos_nodes + cur_node.p + dir;
		MapNode neighbor = data->m_vmanip.getNodeNoExNoEmerge(neighbor_pos);
		// Don't make face if neighbor is of same type
		if (neighbor.getContent() == cur_node.n.getContent())
			continue;
		// Face at Z-
		v3f vertices[4] = {
			v3f(-BS / 2,  BS / 2, -BS / 2),
			v3f( BS / 2,  BS / 2, -BS / 2),
			v3f( BS / 2, -BS / 2, -BS / 2),
			v3f(-BS / 2, -BS / 2, -BS / 2),
		};

		for (v3f &vertex : vertices) {
			switch (face) {
				case D6D_ZP:
					vertex.rotateXZBy(180); break;
				case D6D_YP:
					vertex.rotateYZBy( 90); break;
				case D6D_XP:
					vertex.rotateXZBy( 90); break;
				case D6D_ZN:
					vertex.rotateXZBy(  0); break;
				case D6D_YN:
					vertex.rotateYZBy(-90); break;
				case D6D_XN:
					vertex.rotateXZBy(-90); break;
			}
		}
		drawQuad(tile, vertices, dir);
	}
}

void MapblockMeshGenerator::drawGlasslikeFramedNode()
{
	TileSpec tiles[6];
	for (int face = 0; face < 6; face++)
		getTile(g_6dirs[face], &tiles[face]);

	TileSpec glass_tiles[6];
	for (auto &glass_tile : glass_tiles)
		glass_tile = tiles[4];

	// Only respect H/V merge bits when paramtype2 = "glasslikeliquidlevel" (liquid tank)
	u8 param2 = (cur_node.f->param_type_2 == CPT2_GLASSLIKE_LIQUID_LEVEL) ?
			cur_node.n.getParam2() : 0;
	bool H_merge = !(param2 & 128);
	bool V_merge = !(param2 & 64);
	param2 &= 63;

	static const float a = BS / 2.0f;
	static const float g = a - 0.03f;
	static const float b = 0.876f * (BS / 2.0f);

	static const aabb3f frame_edges[FRAMED_EDGE_COUNT] = {
		aabb3f( b,  b, -a,  a,  a,  a), // y+
		aabb3f(-a,  b, -a, -b,  a,  a), // y+
		aabb3f( b, -a, -a,  a, -b,  a), // y-
		aabb3f(-a, -a, -a, -b, -b,  a), // y-
		aabb3f( b, -a,  b,  a,  a,  a), // x+
		aabb3f( b, -a, -a,  a,  a, -b), // x+
		aabb3f(-a, -a,  b, -b,  a,  a), // x-
		aabb3f(-a, -a, -a, -b,  a, -b), // x-
		aabb3f(-a,  b,  b,  a,  a,  a), // z+
		aabb3f(-a, -a,  b,  a, -b,  a), // z+
		aabb3f(-a, -a, -a,  a, -b, -b), // z-
		aabb3f(-a,  b, -a,  a,  a, -b), // z-
	};

	// tables of neighbor (connect if same type and merge allowed),
	// checked with g_26dirs

	// 1 = connect, 0 = face visible
	bool nb[FRAMED_NEIGHBOR_COUNT] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	// 1 = check
	static const bool check_nb_vertical [FRAMED_NEIGHBOR_COUNT] =
		{0,1,0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
	static const bool check_nb_horizontal [FRAMED_NEIGHBOR_COUNT] =
		{1,0,1,1,0,1, 0,0,0,0, 1,1,1,1, 0,0,0,0};
	static const bool check_nb_all [FRAMED_NEIGHBOR_COUNT] =
		{1,1,1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1};
	const bool *check_nb = check_nb_all;

	// neighbors checks for frames visibility
	if (H_merge || V_merge) {
		if (!H_merge)
			check_nb = check_nb_vertical; // vertical-only merge
		if (!V_merge)
			check_nb = check_nb_horizontal; // horizontal-only merge
		content_t current = cur_node.n.getContent();
		for (int i = 0; i < FRAMED_NEIGHBOR_COUNT; i++) {
			if (!check_nb[i])
				continue;
			v3s16 n2p = blockpos_nodes + cur_node.p + g_26dirs[i];
			MapNode n2 = data->m_vmanip.getNodeNoEx(n2p);
			content_t n2c = n2.getContent();
			if (n2c == current)
				nb[i] = 1;
		}
	}

	// edge visibility

	static const u8 nb_triplet[FRAMED_EDGE_COUNT][3] = {
		{1, 2,  7}, {1, 5,  6}, {4, 2, 15}, {4, 5, 14},
		{2, 0, 11}, {2, 3, 13}, {5, 0, 10}, {5, 3, 12},
		{0, 1,  8}, {0, 4, 16}, {3, 4, 17}, {3, 1,  9},
	};

	for (int edge = 0; edge < FRAMED_EDGE_COUNT; edge++) {
		bool edge_invisible;
		if (nb[nb_triplet[edge][2]])
			edge_invisible = nb[nb_triplet[edge][0]] & nb[nb_triplet[edge][1]];
		else
			edge_invisible = nb[nb_triplet[edge][0]] ^ nb[nb_triplet[edge][1]];
		if (edge_invisible)
			continue;

		f32 txc[24];
		generateCuboidTextureCoords(frame_edges[edge], txc);
		drawAutoLightedCuboid(frame_edges[edge], tiles[1], txc);
	}

	for (int face = 0; face < 6; face++) {
		if (nb[face])
			continue;

		// Face at Z-
		v3f vertices[4] = {
			v3f(-a,  a, -g),
			v3f( a,  a, -g),
			v3f( a, -a, -g),
			v3f(-a, -a, -g),
		};

		for (v3f &vertex : vertices) {
			switch (face) {
				case D6D_ZP:
					vertex.rotateXZBy(180); break;
				case D6D_YP:
					vertex.rotateYZBy( 90); break;
				case D6D_XP:
					vertex.rotateXZBy( 90); break;
				case D6D_ZN:
					vertex.rotateXZBy(  0); break;
				case D6D_YN:
					vertex.rotateYZBy(-90); break;
				case D6D_XN:
					vertex.rotateXZBy(-90); break;
			}
		}
		v3s16 dir = g_6dirs[face];
		drawQuad(glass_tiles[face], vertices, dir);
	}

	// Optionally render internal liquid level defined by param2
	// Liquid is textured with 1 tile defined in nodedef 'special_tiles'
	if (param2 > 0 && cur_node.f->param_type_2 == CPT2_GLASSLIKE_LIQUID_LEVEL &&
			cur_node.f->special_tiles[0].layers[0].texture) {
		// Internal liquid level has param2 range 0 .. 63,
		// convert it to -0.5 .. 0.5
		float vlev = (param2 / 63.0f) * 2.0f - 1.0f;
		TileSpec tile;
		getSpecialTile(0, &tile);
		aabb3f box(
			-(nb[5] ? g : b),
			-(nb[4] ? g : b),
			-(nb[3] ? g : b),
			 (nb[2] ? g : b),
			 (nb[1] ? g : b) * vlev,
			 (nb[0] ? g : b)
		);
		f32 txc[24];
		generateCuboidTextureCoords(box, txc);
		drawAutoLightedCuboid(box, tile, txc);
	}
}

void MapblockMeshGenerator::drawTorchlikeNode()
{
	u8 wall = cur_node.n.getWallMounted(nodedef);
	u8 tileindex = 0;
	switch (wall) {
		case DWM_YP: tileindex = 1; break; // ceiling
		case DWM_YN: tileindex = 0; break; // floor
		case DWM_S1: tileindex = 1; break; // ceiling, but rotated
		case DWM_S2: tileindex = 0; break; // floor, but rotated
		default: tileindex = 2; // side (or invalid, shouldn't happen)
	}
	TileSpec tile;
	useTile(&tile, tileindex, MATERIAL_FLAG_CRACK_OVERLAY, MATERIAL_FLAG_BACKFACE_CULLING);

	float size = BS / 2 * cur_node.f->visual_scale;
	v3f vertices[4] = {
		v3f(-size,  size, 0),
		v3f( size,  size, 0),
		v3f( size, -size, 0),
		v3f(-size, -size, 0),
	};

	for (v3f &vertex : vertices) {
		switch (wall) {
			case DWM_YP:
				vertex.Y += -size + BS/2;
				vertex.rotateXZBy(-45);
				break;
			case DWM_YN:
				vertex.Y += size - BS/2;
				vertex.rotateXZBy(45);
				break;
			case DWM_XP:
				vertex.X += -size + BS/2;
				break;
			case DWM_XN:
				vertex.X += -size + BS/2;
				vertex.rotateXZBy(180);
				break;
			case DWM_ZP:
				vertex.X += -size + BS/2;
				vertex.rotateXZBy(90);
				break;
			case DWM_ZN:
				vertex.X += -size + BS/2;
				vertex.rotateXZBy(-90);
				break;
			case DWM_S1:
				// same as DWM_YP, but rotated 90°
				vertex.Y += -size + BS/2;
				vertex.rotateXZBy(45);
				break;
			case DWM_S2:
				// same as DWM_YN, but rotated -90°
				vertex.Y += size - BS/2;
				vertex.rotateXZBy(-45);
				break;
		}
	}
	drawQuad(tile, vertices);
}

void MapblockMeshGenerator::drawSignlikeNode()
{
	u8 wall = cur_node.n.getWallMounted(nodedef);
	TileSpec tile;
	useTile(&tile, 0, MATERIAL_FLAG_CRACK_OVERLAY, MATERIAL_FLAG_BACKFACE_CULLING);
	static const float offset = BS / 16;
	float size = BS / 2 * cur_node.f->visual_scale;
	// Wall at X+ of node
	v3f vertices[4] = {
		v3f(BS / 2 - offset,  size,  size),
		v3f(BS / 2 - offset,  size, -size),
		v3f(BS / 2 - offset, -size, -size),
		v3f(BS / 2 - offset, -size,  size),
	};

	for (v3f &vertex : vertices) {
		switch (wall) {
			case DWM_YP:
				vertex.rotateXYBy( 90); break;
			case DWM_YN:
				vertex.rotateXYBy(-90); break;
			case DWM_XP:
				vertex.rotateXZBy(  0); break;
			case DWM_XN:
				vertex.rotateXZBy(180); break;
			case DWM_ZP:
				vertex.rotateXZBy( 90); break;
			case DWM_ZN:
				vertex.rotateXZBy(-90); break;
			case DWM_S1:
				vertex.rotateXYBy( 90); vertex.rotateXZBy(90); break;
			case DWM_S2:
				vertex.rotateXYBy(-90); vertex.rotateXZBy(-90); break;
		}
	}
	drawQuad(tile, vertices);
}

void MapblockMeshGenerator::drawPlantlikeQuad(const TileSpec &tile,
		float rotation, float quad_offset, bool offset_top_only)
{
	const f32 scale = cur_plant.scale;
	v3f vertices[4] = {
		v3f(-scale, -BS / 2 + 2.0 * scale * cur_plant.plant_height, 0),
		v3f( scale, -BS / 2 + 2.0 * scale * cur_plant.plant_height, 0),
		v3f( scale, -BS / 2, 0),
		v3f(-scale, -BS / 2, 0),
	};
	if (cur_plant.random_offset_Y) {
		PseudoRandom yrng(cur_plant.face_num++
				| cur_node.p.X << 16
				| cur_node.p.Z << 8
				| cur_node.p.Y << 24);
		cur_plant.offset.Y = -BS * ((yrng.next() % 16 / 16.0) * 0.125);
	}
	int offset_count = offset_top_only ? 2 : 4;
	for (int i = 0; i < offset_count; i++)
		vertices[i].Z += quad_offset;

	for (v3f &vertex : vertices) {
		vertex.rotateXZBy(rotation + cur_plant.rotate_degree);
		vertex += cur_plant.offset;
	}

	u8 wall = cur_node.n.getWallMounted(nodedef);
	if (wall != DWM_YN) {
		for (v3f &vertex : vertices) {
			switch (wall) {
				case DWM_YP:
					vertex.rotateYZBy(180);
					vertex.rotateXZBy(180);
					break;
				case DWM_XP:
					vertex.rotateXYBy(90);
					break;
				case DWM_XN:
					vertex.rotateXYBy(-90);
					vertex.rotateYZBy(180);
					break;
				case DWM_ZP:
					vertex.rotateYZBy(-90);
					vertex.rotateXYBy(90);
					break;
				case DWM_ZN:
					vertex.rotateYZBy(90);
					vertex.rotateXYBy(90);
					break;
			}
		}
	}

	drawQuad(tile, vertices, v3s16(0, 0, 0), cur_plant.plant_height);
}

void MapblockMeshGenerator::drawPlantlike(const TileSpec &tile, bool is_rooted)
{
	cur_plant.draw_style = PLANT_STYLE_CROSS;
	cur_plant.offset = v3f(0, 0, 0);
	cur_plant.scale = BS / 2 * cur_node.f->visual_scale;
	cur_plant.rotate_degree = 0.0f;
	cur_plant.random_offset_Y = false;
	cur_plant.face_num = 0;
	cur_plant.plant_height = 1.0;

	switch (cur_node.f->param_type_2) {
	case CPT2_MESHOPTIONS:
		cur_plant.draw_style = PlantlikeStyle(cur_node.n.param2 & MO_MASK_STYLE);
		if (cur_node.n.param2 & MO_BIT_SCALE_SQRT2)
			cur_plant.scale *= 1.41421;
		if (cur_node.n.param2 & MO_BIT_RANDOM_OFFSET) {
			PseudoRandom rng(cur_node.p.X << 8 | cur_node.p.Z | cur_node.p.Y << 16);
			cur_plant.offset.X = BS * ((rng.next() % 16 / 16.0) * 0.29 - 0.145);
			cur_plant.offset.Z = BS * ((rng.next() % 16 / 16.0) * 0.29 - 0.145);
		}
		if (cur_node.n.param2 & MO_BIT_RANDOM_OFFSET_Y)
			cur_plant.random_offset_Y = true;
		break;

	case CPT2_DEGROTATE:
	case CPT2_COLORED_DEGROTATE:
		cur_plant.rotate_degree = 1.5f * cur_node.n.getDegRotate(nodedef);
		break;

	case CPT2_LEVELED:
		cur_plant.plant_height = cur_node.n.param2 / 16.0;
		break;

	default:
		break;
	}

	if (is_rooted) {
		u8 wall = cur_node.n.getWallMounted(nodedef);
		switch (wall) {
			case DWM_YP:
				cur_plant.offset.Y += BS*2;
				break;
			case DWM_XN:
			case DWM_XP:
			case DWM_ZN:
			case DWM_ZP:
				cur_plant.offset.X += -BS;
				cur_plant.offset.Y +=  BS;
				break;
		}
	}

	switch (cur_plant.draw_style) {
	case PLANT_STYLE_CROSS:
		drawPlantlikeQuad(tile, 46);
		drawPlantlikeQuad(tile, -44);
		break;

	case PLANT_STYLE_CROSS2:
		drawPlantlikeQuad(tile, 91);
		drawPlantlikeQuad(tile, 1);
		break;

	case PLANT_STYLE_STAR:
		drawPlantlikeQuad(tile, 121);
		drawPlantlikeQuad(tile, 241);
		drawPlantlikeQuad(tile, 1);
		break;

	case PLANT_STYLE_HASH:
		drawPlantlikeQuad(tile,   1, BS / 4);
		drawPlantlikeQuad(tile,  91, BS / 4);
		drawPlantlikeQuad(tile, 181, BS / 4);
		drawPlantlikeQuad(tile, 271, BS / 4);
		break;

	case PLANT_STYLE_HASH2:
		drawPlantlikeQuad(tile,   1, -BS / 2, true);
		drawPlantlikeQuad(tile,  91, -BS / 2, true);
		drawPlantlikeQuad(tile, 181, -BS / 2, true);
		drawPlantlikeQuad(tile, 271, -BS / 2, true);
		break;
	}
}

void MapblockMeshGenerator::drawPlantlikeNode()
{
	TileSpec tile;
	useTile(&tile);
	drawPlantlike(tile);
}

void MapblockMeshGenerator::drawPlantlikeRootedNode()
{
	drawSolidNode();
	TileSpec tile;
	useTile(&tile, 0, MATERIAL_FLAG_CRACK_OVERLAY, 0, true);
	cur_node.origin += v3f(0.0, BS, 0.0);
	cur_node.p.Y++;
	if (data->m_smooth_lighting) {
		getSmoothLightFrame();
	} else {
		MapNode ntop = data->m_vmanip.getNodeNoEx(blockpos_nodes + cur_node.p);
		auto light = LightPair(getInteriorLight(ntop, 0, nodedef));
		cur_node.lcolor = encode_light(light, cur_node.f->light_source);
	}
	drawPlantlike(tile, true);
	cur_node.p.Y--;
}

void MapblockMeshGenerator::drawFirelikeQuad(const TileSpec &tile, float rotation,
		float opening_angle, float offset_h, float offset_v)
{
	const f32 scale = BS / 2 * cur_node.f->visual_scale;
	v3f vertices[4] = {
		v3f(-scale, -BS / 2 + scale * 2, 0),
		v3f( scale, -BS / 2 + scale * 2, 0),
		v3f( scale, -BS / 2, 0),
		v3f(-scale, -BS / 2, 0),
	};

	for (v3f &vertex : vertices) {
		vertex.rotateYZBy(opening_angle);
		vertex.Z += offset_h;
		vertex.rotateXZBy(rotation);
		vertex.Y += offset_v;
	}
	drawQuad(tile, vertices);
}

void MapblockMeshGenerator::drawFirelikeNode()
{
	TileSpec tile;
	useTile(&tile);

	// Check for adjacent nodes
	bool neighbors = false;
	bool neighbor[6] = {0, 0, 0, 0, 0, 0};
	content_t current = cur_node.n.getContent();
	for (int i = 0; i < 6; i++) {
		v3s16 n2p = blockpos_nodes + cur_node.p + g_6dirs[i];
		MapNode n2 = data->m_vmanip.getNodeNoEx(n2p);
		content_t n2c = n2.getContent();
		if (n2c != CONTENT_IGNORE && n2c != CONTENT_AIR && n2c != current) {
			neighbor[i] = true;
			neighbors = true;
		}
	}
	bool drawBasicFire = neighbor[D6D_YN] || !neighbors;
	bool drawBottomFire = neighbor[D6D_YP];

	if (drawBasicFire || neighbor[D6D_ZP])
		drawFirelikeQuad(tile, 0, -10, 0.4 * BS);
	else if (drawBottomFire)
		drawFirelikeQuad(tile, 0, 70, 0.47 * BS, 0.484 * BS);

	if (drawBasicFire || neighbor[D6D_XN])
		drawFirelikeQuad(tile, 90, -10, 0.4 * BS);
	else if (drawBottomFire)
		drawFirelikeQuad(tile, 90, 70, 0.47 * BS, 0.484 * BS);

	if (drawBasicFire || neighbor[D6D_ZN])
		drawFirelikeQuad(tile, 180, -10, 0.4 * BS);
	else if (drawBottomFire)
		drawFirelikeQuad(tile, 180, 70, 0.47 * BS, 0.484 * BS);

	if (drawBasicFire || neighbor[D6D_XP])
		drawFirelikeQuad(tile, 270, -10, 0.4 * BS);
	else if (drawBottomFire)
		drawFirelikeQuad(tile, 270, 70, 0.47 * BS, 0.484 * BS);

	if (drawBasicFire) {
		drawFirelikeQuad(tile, 45, 0, 0.0);
		drawFirelikeQuad(tile, -45, 0, 0.0);
	}
}

void MapblockMeshGenerator::drawFencelikeNode()
{
	TileSpec tile_nocrack;
	useTile(&tile_nocrack, 0, 0, 0);

	for (auto &layer : tile_nocrack.layers)
		layer.material_flags &= ~MATERIAL_FLAG_CRACK;

	// Put wood the right way around in the posts
	TileSpec tile_rot = tile_nocrack;
	tile_rot.rotation = TileRotation::R90;

	static const f32 post_rad = BS / 8;
	static const f32 bar_rad  = BS / 16;
	static const f32 bar_len  = BS / 2 - post_rad;

	// The post - always present
	static const aabb3f post(-post_rad, -BS / 2, -post_rad,
	                          post_rad,  BS / 2,  post_rad);
	static const f32 postuv[24] = {
		0.375, 0.375, 0.625, 0.625,
		0.375, 0.375, 0.625, 0.625,
		0.000, 0.000, 0.250, 1.000,
		0.250, 0.000, 0.500, 1.000,
		0.500, 0.000, 0.750, 1.000,
		0.750, 0.000, 1.000, 1.000,
	};
	drawAutoLightedCuboid(post, tile_rot, postuv);

	// Now a section of fence, +X, if there's a post there
	v3s16 p2 = cur_node.p;
	p2.X++;
	MapNode n2 = data->m_vmanip.getNodeNoEx(blockpos_nodes + p2);
	const ContentFeatures *f2 = &nodedef->get(n2);
	if (f2->drawtype == NDT_FENCELIKE) {
		static const aabb3f bar_x1(BS / 2 - bar_len,  BS / 4 - bar_rad, -bar_rad,
		                           BS / 2 + bar_len,  BS / 4 + bar_rad,  bar_rad);
		static const aabb3f bar_x2(BS / 2 - bar_len, -BS / 4 - bar_rad, -bar_rad,
		                           BS / 2 + bar_len, -BS / 4 + bar_rad,  bar_rad);
		static const f32 xrailuv[24] = {
			0.000, 0.125, 1.000, 0.250,
			0.000, 0.250, 1.000, 0.375,
			0.375, 0.375, 0.500, 0.500,
			0.625, 0.625, 0.750, 0.750,
			0.000, 0.500, 1.000, 0.625,
			0.000, 0.875, 1.000, 1.000,
		};
		drawAutoLightedCuboid(bar_x1, tile_nocrack, xrailuv);
		drawAutoLightedCuboid(bar_x2, tile_nocrack, xrailuv);
	}

	// Now a section of fence, +Z, if there's a post there
	p2 = cur_node.p;
	p2.Z++;
	n2 = data->m_vmanip.getNodeNoEx(blockpos_nodes + p2);
	f2 = &nodedef->get(n2);
	if (f2->drawtype == NDT_FENCELIKE) {
		static const aabb3f bar_z1(-bar_rad,  BS / 4 - bar_rad, BS / 2 - bar_len,
		                            bar_rad,  BS / 4 + bar_rad, BS / 2 + bar_len);
		static const aabb3f bar_z2(-bar_rad, -BS / 4 - bar_rad, BS / 2 - bar_len,
		                            bar_rad, -BS / 4 + bar_rad, BS / 2 + bar_len);
		static const f32 zrailuv[24] = {
			0.1875, 0.0625, 0.3125, 0.3125, // cannot rotate; stretch
			0.2500, 0.0625, 0.3750, 0.3125, // for wood texture instead
			0.0000, 0.5625, 1.0000, 0.6875,
			0.0000, 0.3750, 1.0000, 0.5000,
			0.3750, 0.3750, 0.5000, 0.5000,
			0.6250, 0.6250, 0.7500, 0.7500,
		};
		drawAutoLightedCuboid(bar_z1, tile_nocrack, zrailuv);
		drawAutoLightedCuboid(bar_z2, tile_nocrack, zrailuv);
	}
}

bool MapblockMeshGenerator::isSameRail(v3s16 dir)
{
	MapNode node2 = data->m_vmanip.getNodeNoEx(blockpos_nodes + cur_node.p + dir);
	if (node2.getContent() == cur_node.n.getContent())
		return true;
	const ContentFeatures &def2 = nodedef->get(node2);
	return ((def2.drawtype == NDT_RAILLIKE) &&
		(def2.getGroup(raillike_groupname) == cur_rail.raillike_group));
}

namespace {
	static const v3s16 rail_direction[4] = {
		v3s16( 0, 0,  1),
		v3s16( 0, 0, -1),
		v3s16(-1, 0,  0),
		v3s16( 1, 0,  0),
	};
	static const int rail_slope_angle[4] = {0, 180, 90, -90};

	enum RailTile {
		straight,
		curved,
		junction,
		cross,
	};
	struct RailDesc {
		int tile_index;
		int angle;
	};
	static const RailDesc rail_kinds[16] = {
		                 // +x -x -z +z
		                 //-------------
		{straight,   0}, //  .  .  .  .
		{straight,   0}, //  .  .  . +Z
		{straight,   0}, //  .  . -Z  .
		{straight,   0}, //  .  . -Z +Z
		{straight,  90}, //  . -X  .  .
		{  curved, 180}, //  . -X  . +Z
		{  curved, 270}, //  . -X -Z  .
		{junction, 180}, //  . -X -Z +Z
		{straight,  90}, // +X  .  .  .
		{  curved,  90}, // +X  .  . +Z
		{  curved,   0}, // +X  . -Z  .
		{junction,   0}, // +X  . -Z +Z
		{straight,  90}, // +X -X  .  .
		{junction,  90}, // +X -X  . +Z
		{junction, 270}, // +X -X -Z  .
		{   cross,   0}, // +X -X -Z +Z
	};
}

void MapblockMeshGenerator::drawRaillikeNode()
{
	cur_rail.raillike_group = cur_node.f->getGroup(raillike_groupname);

	int code = 0;
	int angle;
	int tile_index;
	bool sloped = false;
	for (int dir = 0; dir < 4; dir++) {
		bool rail_above = isSameRail(rail_direction[dir] + v3s16(0, 1, 0));
		if (rail_above) {
			sloped = true;
			angle = rail_slope_angle[dir];
		}
		if (rail_above ||
				isSameRail(rail_direction[dir]) ||
				isSameRail(rail_direction[dir] + v3s16(0, -1, 0)))
			code |= 1 << dir;
	}

	if (sloped) {
		tile_index = straight;
	} else {
		tile_index = rail_kinds[code].tile_index;
		angle = rail_kinds[code].angle;
	}

	TileSpec tile;
	useTile(&tile, tile_index, MATERIAL_FLAG_CRACK_OVERLAY, MATERIAL_FLAG_BACKFACE_CULLING);

	static const float offset = BS / 64;
	static const float size   = BS / 2;
	float y2 = sloped ? size : -size;
	v3f vertices[4] = {
		v3f(-size,    y2 + offset,  size),
		v3f( size,    y2 + offset,  size),
		v3f( size, -size + offset, -size),
		v3f(-size, -size + offset, -size),
	};
	if (angle)
		for (v3f &vertex : vertices)
			vertex.rotateXZBy(angle);
	drawQuad(tile, vertices);
}

namespace {
	static const v3s16 nodebox_tile_dirs[6] = {
		v3s16(0, 1, 0),
		v3s16(0, -1, 0),
		v3s16(1, 0, 0),
		v3s16(-1, 0, 0),
		v3s16(0, 0, 1),
		v3s16(0, 0, -1)
	};

	// we have this order for some reason...
	static const v3s16 nodebox_connection_dirs[6] = {
		v3s16( 0,  1,  0), // top
		v3s16( 0, -1,  0), // bottom
		v3s16( 0,  0, -1), // front
		v3s16(-1,  0,  0), // left
		v3s16( 0,  0,  1), // back
		v3s16( 1,  0,  0), // right
	};
}

void MapblockMeshGenerator::drawAllfacesNode()
{
	static const aabb3f box(-BS / 2, -BS / 2, -BS / 2, BS / 2, BS / 2, BS / 2);
	TileSpec tiles[6];
	for (int face = 0; face < 6; face++)
		getTile(nodebox_tile_dirs[face], &tiles[face]);
	if (data->m_smooth_lighting)
		getSmoothLightFrame();
	drawAutoLightedCuboid(box, tiles, 6);
}

void MapblockMeshGenerator::drawNodeboxNode()
{
	TileSpec tiles[6];
	for (int face = 0; face < 6; face++) {
		// Handles facedir rotation for textures
		getTile(nodebox_tile_dirs[face], &tiles[face]);
	}

	bool param2_is_rotation =
			cur_node.f->param_type_2 == CPT2_COLORED_FACEDIR ||
			cur_node.f->param_type_2 == CPT2_COLORED_WALLMOUNTED ||
			cur_node.f->param_type_2 == CPT2_COLORED_4DIR ||
			cur_node.f->param_type_2 == CPT2_FACEDIR ||
			cur_node.f->param_type_2 == CPT2_WALLMOUNTED ||
			cur_node.f->param_type_2 == CPT2_4DIR;

	bool param2_is_level =
			cur_node.f->param_type_2 == CPT2_LEVELED;

	// locate possible neighboring nodes to connect to
	u8 neighbors_set = 0;
	u8 solid_neighbors = 0;
	u8 sametype_neighbors = 0;
	for (int dir = 0; dir != 6; dir++) {
		u8 flag = 1 << dir;
		v3s16 p2 = blockpos_nodes + cur_node.p + nodebox_tile_dirs[dir];
		MapNode n2 = data->m_vmanip.getNodeNoEx(p2);

		// mark neighbors that are the same node type
		// and have the same rotation or higher level stored as param2
		if (n2.param0 == cur_node.n.param0 &&
				(!param2_is_rotation || cur_node.n.param2 == n2.param2) &&
				(!param2_is_level || cur_node.n.param2 <= n2.param2))
			sametype_neighbors |= flag;

		// mark neighbors that are simple solid blocks
		if (nodedef->get(n2).drawtype == NDT_NORMAL)
			solid_neighbors |= flag;

		if (cur_node.f->node_box.type == NODEBOX_CONNECTED) {
			p2 = blockpos_nodes + cur_node.p + nodebox_connection_dirs[dir];
			n2 = data->m_vmanip.getNodeNoEx(p2);
			if (nodedef->nodeboxConnects(cur_node.n, n2, flag))
				neighbors_set |= flag;
		}
	}

	std::vector<aabb3f> boxes;
	cur_node.n.getNodeBoxes(nodedef, &boxes, neighbors_set);

	bool isTransparent = false;

	for (const TileSpec &tile : tiles) {
		if (tile.layers[0].isTransparent()) {
			isTransparent = true;
			break;
		}
	}

	if (isTransparent) {
		std::vector<float> sections;
		// Preallocate 8 default splits + Min&Max for each nodebox
		sections.reserve(8 + 2 * boxes.size());

		for (int axis = 0; axis < 3; axis++) {
			// identify sections

			if (axis == 0) {
				// Default split at node bounds, up to 3 nodes in each direction
				for (float s = -3.5f * BS; s < 4.0f * BS; s += 1.0f * BS)
					sections.push_back(s);
			}
			else {
				// Avoid readding the same 8 default splits for Y and Z
				sections.resize(8);
			}

			// Add edges of existing node boxes, rounded to 1E-3
			for (size_t i = 0; i < boxes.size(); i++) {
				sections.push_back(std::floor(boxes[i].MinEdge[axis] * 1E3) * 1E-3);
				sections.push_back(std::floor(boxes[i].MaxEdge[axis] * 1E3) * 1E-3);
			}

			// split the boxes at recorded sections
			// limit splits to avoid runaway crash if inner loop adds infinite splits
			// due to e.g. precision problems.
			// 100 is just an arbitrary, reasonably high number.
			for (size_t i = 0; i < boxes.size() && i < 100; i++) {
				aabb3f *box = &boxes[i];
				for (float section : sections) {
					if (box->MinEdge[axis] < section && box->MaxEdge[axis] > section) {
						aabb3f copy(*box);
						copy.MinEdge[axis] = section;
						box->MaxEdge[axis] = section;
						boxes.push_back(copy);
						box = &boxes[i]; // find new address of the box in case of reallocation
					}
				}
			}
		}
	}

	for (auto &box : boxes) {
		u8 mask = getNodeBoxMask(box, solid_neighbors, sametype_neighbors);

		f32 txc[24];
		generateCuboidTextureCoords(box, txc);
		drawAutoLightedCuboid(box, tiles, 6, txc, mask);
	}
}

void MapblockMeshGenerator::drawMeshNode()
{
	u8 facedir = 0;
	scene::IMesh* mesh;
	int degrotate = 0;

	if (cur_node.f->param_type_2 == CPT2_FACEDIR ||
			cur_node.f->param_type_2 == CPT2_COLORED_FACEDIR ||
			cur_node.f->param_type_2 == CPT2_4DIR ||
			cur_node.f->param_type_2 == CPT2_COLORED_4DIR) {
		facedir = cur_node.n.getFaceDir(nodedef);
	} else if (cur_node.f->param_type_2 == CPT2_WALLMOUNTED ||
			cur_node.f->param_type_2 == CPT2_COLORED_WALLMOUNTED) {
		// Convert wallmounted to 6dfacedir.
		facedir = cur_node.n.getWallMounted(nodedef);
		facedir = wallmounted_to_facedir[facedir];
	} else if (cur_node.f->param_type_2 == CPT2_DEGROTATE ||
			cur_node.f->param_type_2 == CPT2_COLORED_DEGROTATE) {
		degrotate = cur_node.n.getDegRotate(nodedef);
	}

	if (cur_node.f->mesh_ptr) {
		// clone and rotate mesh
		mesh = cloneStaticMesh(cur_node.f->mesh_ptr);
		bool modified = true;
		if (facedir)
			rotateMeshBy6dFacedir(mesh, facedir);
		else if (degrotate)
			rotateMeshXZby(mesh, 1.5f * degrotate);
		else
			modified = false;
		if (modified) {
			recalculateBoundingBox(mesh);
		}
	} else {
		warningstream << "drawMeshNode(): missing mesh" << std::endl;
		return;
	}

	for (u32 j = 0; j < mesh->getMeshBufferCount(); j++) {
		// Only up to 6 tiles are supported
		const u32 tile_idx = mesh->getTextureSlot(j);
		TileSpec tile;
		useTile(&tile, MYMIN(tile_idx, 5));

		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		video::S3DVertex *vertices = (video::S3DVertex *)buf->getVertices();
		u32 vertex_count = buf->getVertexCount();

		// Mesh is always private here. So the lighting is applied to each
		// vertex right here.
		if (data->m_smooth_lighting) {
			for (u32 k = 0; k < vertex_count; k++) {
				video::S3DVertex &vertex = vertices[k];
				vertex.Color = blendLightColor(vertex.Pos, vertex.Normal);
				vertex.Pos += cur_node.origin;
			}
		} else {
			bool is_light_source = cur_node.f->light_source != 0;
			for (u32 k = 0; k < vertex_count; k++) {
				video::S3DVertex &vertex = vertices[k];
				video::SColor color = cur_node.lcolor;
				if (!is_light_source)
					applyFacesShading(color, vertex.Normal);
				vertex.Color = color;
				vertex.Pos += cur_node.origin;
			}
		}
		collector->append(tile, vertices, vertex_count,
			buf->getIndices(), buf->getIndexCount());
	}
	mesh->drop();
}

// also called when the drawtype is known but should have been pre-converted
void MapblockMeshGenerator::errorUnknownDrawtype()
{
	infostream << "Got drawtype " << cur_node.f->drawtype << std::endl;
	FATAL_ERROR("Unknown drawtype");
}

void MapblockMeshGenerator::drawNode()
{
	cur_node.origin = intToFloat(cur_node.p, BS);
	switch (cur_node.f->drawtype) {
		case NDT_AIRLIKE:  // Not drawn at all
			return;
		case NDT_LIQUID:
		case NDT_NORMAL: // solid nodes don’t need the usual setup
			drawSolidNode();
			return;
		default:
			break;
	}
	if (data->m_smooth_lighting) {
		getSmoothLightFrame();
	} else {
		auto light = LightPair(getInteriorLight(cur_node.n, 0, nodedef));
		cur_node.lcolor = encode_light(light, cur_node.f->light_source);
	}
	switch (cur_node.f->drawtype) {
		case NDT_FLOWINGLIQUID:     drawLiquidNode(); break;
		case NDT_GLASSLIKE:         drawGlasslikeNode(); break;
		case NDT_GLASSLIKE_FRAMED:  drawGlasslikeFramedNode(); break;
		case NDT_ALLFACES:          drawAllfacesNode(); break;
		case NDT_TORCHLIKE:         drawTorchlikeNode(); break;
		case NDT_SIGNLIKE:          drawSignlikeNode(); break;
		case NDT_PLANTLIKE:         drawPlantlikeNode(); break;
		case NDT_PLANTLIKE_ROOTED:  drawPlantlikeRootedNode(); break;
		case NDT_FIRELIKE:          drawFirelikeNode(); break;
		case NDT_FENCELIKE:         drawFencelikeNode(); break;
		case NDT_RAILLIKE:          drawRaillikeNode(); break;
		case NDT_NODEBOX:           drawNodeboxNode(); break;
		case NDT_MESH:              drawMeshNode(); break;
		default:                    errorUnknownDrawtype(); break;
	}
}

void MapblockMeshGenerator::generate()
{
	ZoneScoped;

	for (cur_node.p.Z = 0; cur_node.p.Z < data->m_side_length; cur_node.p.Z++)
	for (cur_node.p.Y = 0; cur_node.p.Y < data->m_side_length; cur_node.p.Y++)
	for (cur_node.p.X = 0; cur_node.p.X < data->m_side_length; cur_node.p.X++) {
		cur_node.n = data->m_vmanip.getNodeNoEx(blockpos_nodes + cur_node.p);
		cur_node.f = &nodedef->get(cur_node.n);
		drawNode();
	}
}
