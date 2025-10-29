// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <cmath>
#include "lod_mapblock.h"
#include "util/basic_macros.h"
#include "util/numeric.h"
#include "util/tracy_wrapper.h"
#include "mapblock_mesh.h"
#include "settings.h"
#include "nodedef.h"
#include "client/tile.h"
#include "client/meshgen/collector.h"
#include "client/renderingengine.h"
#include "client.h"

#include "profiler.h"

static constexpr u16 quad_indices_02[] = {0, 1, 2, 2, 3, 0};
static const auto &quad_indices = quad_indices_02;

LodMeshGenerator::LodMeshGenerator(MeshMakeData *input, MeshCollector *output, const bool is_mono_mat):
	m_data(input),
	m_collector(output),
	m_nodedef(m_data->m_nodedef),
	m_blockpos_nodes(m_data->m_blockpos * MAP_BLOCKSIZE),
	m_is_mono_mat(is_mono_mat)
{
}

void LodMeshGenerator::generateBitsetMesh(const MapNode n, const u8 width,
                                          const v3s16 seg_start, const video::SColor color_in)
{
	const core::vector3df seg_offset(seg_start.X * BS, seg_start.Y * BS, seg_start.Z * BS);
	const f32 scaled_BS = BS * width;

	core::vector3df vertices[4];
	video::S3DVertex irr_vertices[4];
	for (u8 direction = 0; direction < Direction_END; direction++) {
		TileSpec tile;
		video::SColor color;
		if (m_is_mono_mat) {
			// When generating a mesh with no texture, we have to color the vertices instead.
			video::SColor c2 = m_nodedef->get(n).average_colors[direction];
			color = video::SColor(
				color.getAlpha(),
				color_in.getRed() * c2.getRed() / 255U,
				color_in.getGreen() * c2.getGreen() / 255U,
				color_in.getBlue() * c2.getBlue() / 255U);
		} else {
			getNodeTileN(n, m_blockpos_nodes, direction, m_data, tile);
			color = color_in;
		}

		const u64 direction_offset = BITSET_MAX_NOPAD2 * direction;
		for (u8 slice_i = 0; slice_i < BITSET_MAX_NOPAD; slice_i++) {
			const u64 slice_offset = direction_offset + BITSET_MAX_NOPAD * slice_i;
			for (u8 u = 0; u < BITSET_MAX_NOPAD; u++) {

				bitset column = m_slices[slice_offset + u];
				while (column) {
					s32 v0 = __builtin_ctzll(column);
					// Shift the bitset down, so it has no low 0s anymore,
					// then count the numer of low 1s to get the length of the greedy quad.
					s32 v1 = __builtin_ctzll(~(column >> v0));
					const bitset mask = ((1ULL << v1) - 1) << v0;
					column ^= mask;
					// Determine the width of the greedy quad
					s32 u1 = 1;
					while (u + u1 < BITSET_MAX_NOPAD && // while still in current chunk
						(m_slices[slice_offset + u + u1] & mask) == mask // and next column shares faces
						) {
						// then increase width and unset the bits
						m_slices[slice_offset + u + u1] ^= mask;
						u1++;
					}

					const core::vector2d<f32> uvs[4] = {
						core::vector2d<f32>{0, static_cast<f32>(v1*width)},
						core::vector2d<f32>{0, 0},
						core::vector2d<f32>{static_cast<f32>(u1*width), 0},
						core::vector2d<f32>{static_cast<f32>(u1*width), static_cast<f32>(v1*width)}
					};

					// calculate low (0) and high (1) coordinates for u and v axis
					u1 = (u + u1) * scaled_BS - BS / 2;
					const s32 u0 = u * scaled_BS - BS / 2;
					v1 = (v0 + v1) * scaled_BS - BS / 2;
					v0 = v0 * scaled_BS - BS / 2;
					// calculate depth at which to place the quad
					const s32 w = ((slice_i + 1) * width - 1
						+ (direction % 2 == 1 ? -width + 1 : 1)) * BS
						- BS / 2;

					switch (direction) {
					case 0:
					case 1:
						vertices[0] = core::vector3df(u0, w, v0);
						vertices[1] = core::vector3df(u1, w, v0);
						vertices[2] = core::vector3df(u1, w, v1);
						vertices[3] = core::vector3df(u0, w, v1);
						break;
					case 2:
						vertices[0] = core::vector3df(w, u0, v0);
						vertices[1] = core::vector3df(w, u0, v1);
						vertices[2] = core::vector3df(w, u1, v1);
						vertices[3] = core::vector3df(w, u1, v0);
						break;
					case 3:
						vertices[0] = core::vector3df(w, u0, v1);
						vertices[1] = core::vector3df(w, u1, v1);
						vertices[2] = core::vector3df(w, u1, v0);
						vertices[3] = core::vector3df(w, u0, v0);
						break;
					case 4:
						vertices[0] = core::vector3df(u1, v0, w);
						vertices[1] = core::vector3df(u0, v0, w);
						vertices[2] = core::vector3df(u0, v1, w);
						vertices[3] = core::vector3df(u1, v1, w);
						break;
					default:
						vertices[0] = core::vector3df(u0, v0, w);
						vertices[1] = core::vector3df(u0, v1, w);
						vertices[2] = core::vector3df(u1, v1, w);
						vertices[3] = core::vector3df(u1, v0, w);
						break;
					}
					for (core::vector3df& v : vertices)
						v += seg_offset;

					// set winding order
					switch (direction) {
					case 1:
					case 3:
					case 5:
						irr_vertices[0] = video::S3DVertex(vertices[0], s_normals[direction], color, uvs[0]);
						irr_vertices[1] = video::S3DVertex(vertices[1], s_normals[direction], color, uvs[1]);
						irr_vertices[2] = video::S3DVertex(vertices[2], s_normals[direction], color, uvs[2]);
						irr_vertices[3] = video::S3DVertex(vertices[3], s_normals[direction], color, uvs[3]);
						break;
					default:
						irr_vertices[0] = video::S3DVertex(vertices[0], s_normals[direction], color, uvs[0]);
						irr_vertices[1] = video::S3DVertex(vertices[3], s_normals[direction], color, uvs[1]);
						irr_vertices[2] = video::S3DVertex(vertices[2], s_normals[direction], color, uvs[2]);
						irr_vertices[3] = video::S3DVertex(vertices[1], s_normals[direction], color, uvs[3]);
					}
					m_collector->append(m_is_mono_mat ? s_static_tile : tile, irr_vertices, 4, quad_indices, 6);
				}
			}
		}
	}
}

LightPair LodMeshGenerator::computeMaxFaceLight(const MapNode n, const v3s16 p, const v3s16 dir) const
{
	const u16 lp1 = getFaceLight(n, m_data->m_vmanip.getNodeNoExNoEmerge(p + dir), m_nodedef);
	const u16 lp2 = getFaceLight(n, m_data->m_vmanip.getNodeNoExNoEmerge(p - dir), m_nodedef);
	return static_cast<LightPair>(std::max(lp1, lp2));
}

void LodMeshGenerator::generateGreedyLod(const std::bitset<NodeDrawType_END> types, const v3s16 seg_start,
		const v3s16 seg_size, const u8 width)
{
	// all nodes in this volume, , for finding node faces
	bitset all_set_nodes[3 * BITSET_MAX * BITSET_MAX] = {0};
	std::map<content_t, MapNode> node_types;
	// all nodes in this volume, on each of the 3 axes, grouped by type and brightness, for use in actual mesh generation
	std::unordered_map<NodeKey, bitset[3 * BITSET_MAX * BITSET_MAX]> set_nodes;

	const v3s16 to = seg_start + seg_size;
	const s16 max_light_step = std::min<s16>(MAP_BLOCKSIZE, width);

	v3s16 p;
	for (p.Z = seg_start.Z - 1; p.Z < to.Z + width; p.Z += width)
	for (p.Y = seg_start.Y - 1; p.Y < to.Y + width; p.Y += width)
	for (p.X = seg_start.X - 1; p.X < to.X + width; p.X += width) {
		MapNode n = m_data->m_vmanip.getNodeNoExNoEmerge(p);
		if (n.getContent() == CONTENT_IGNORE) {
			continue;
		}
		// when our sample is air, take more samples in a straight line down, to make sure we always hit the surface
		// otherwise, snowy mountains or grassy hills would display lumps of dirt and stone
		const ContentFeatures* f = &m_nodedef->get(n);
		for (u8 subtr = 1; subtr < width && f->drawtype == NDT_AIRLIKE; subtr++) {
			n = m_data->m_vmanip.getNodeNoExNoEmerge(p - v3s16(0, subtr, 0));
			f = &m_nodedef->get(n);
		}
		if (!types.test(f->drawtype)) {
			continue;
		}

		const content_t node_type = n.getContent();
		const v3s16 p_scaled = (p - seg_start + 1) / width;
		node_types.try_emplace(node_type, n);

		if (f->drawtype == NDT_NORMAL) {
			// take a light sample for each side of a node, on each axis and take the maximum.
			// it would be more accurate to take a sample for each of the 6 directions intead of each axis,
			// but that would take twice the ram
			LightPair lp = computeMaxFaceLight(n, p, v3s16(max_light_step, 0, 0));
			NodeKey key = NodeKey{node_type, lp};
			set_nodes[key][BITSET_MAX * p_scaled.Y + p_scaled.Z] |= 1ULL << p_scaled.X; // x axis

			lp = computeMaxFaceLight(n, p, v3s16(0, max_light_step, 0));
			key = NodeKey{node_type, lp};
			set_nodes[key][BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Z] |= 1ULL << p_scaled.Y; // y axis

			lp = computeMaxFaceLight(n, p, v3s16(0, 0, max_light_step));
			key = NodeKey{node_type, lp};
			set_nodes[key][2 * BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Y] |= 1ULL << p_scaled.Z; // z axis
		}
		else {
			const LightPair lp = static_cast<LightPair>(getInteriorLight(n, 0, m_nodedef));

			NodeKey key{node_type, lp};
			auto &nodes = set_nodes[key];
			nodes[BITSET_MAX * p_scaled.Y + p_scaled.Z] |= 1ULL << p_scaled.X; // x axis
			nodes[BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Z] |= 1ULL << p_scaled.Y; // y axis
			nodes[2 * BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Y] |= 1ULL << p_scaled.Z; // z axis
		}

		all_set_nodes[BITSET_MAX * p_scaled.Y + p_scaled.Z] |= 1ULL << p_scaled.X; // x axis
		all_set_nodes[BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Z] |= 1ULL << p_scaled.Y; // y axis
		all_set_nodes[2 * BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Y] |= 1ULL << p_scaled.Z; // z axis
	}

	static constexpr bitset U62_MAX = U64_MAX >> 2;

	for (const auto& [node_key, nodes] : set_nodes) {
		for (u8 u = 1; u <= BITSET_MAX_NOPAD; u++)
		for (u8 v = 1; v <= BITSET_MAX_NOPAD; v++) {
			// Shifting the bitset of set nodes in a column to the right, inverting it, then &-ing it with another bitset
			// leaves only the bits with no neighbors to their left. So this calculates all left facing node faces
			// in that column.
			// To do it like this means we need a bit of padding on each side, which is why we are limited to only 62
			// nodes per volume.
			// These operations are considerably faster on a regular u64 (here aliased as bitset) instead of an
			// std::bitset, so we *cant* flatten these bitsets into one large std::bitset.
			m_nodes_faces[BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX2 + BITSET_MAX * u + v] >> 1))
				>> 1 & U62_MAX;

			m_nodes_faces[BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX2 + BITSET_MAX * u + v] << 1))
				>> 1 & U62_MAX;

			m_nodes_faces[2 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX * u + v] >> 1))
				>> 1 & U62_MAX;

			m_nodes_faces[3 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX * u + v] << 1))
				>> 1 & U62_MAX;

			m_nodes_faces[4 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] >> 1))
				>> 1 & U62_MAX;

			m_nodes_faces[5 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] << 1))
				>> 1 & U62_MAX;
		}

		// We only calculated the visible node faces per column, so far.
		// But to use greedy meshing, we need the faces *next* to each other, not behind each other.
		// Each node face is mapped to their corresponding slice/plane
		memset(m_slices, 0, sizeof(m_slices));
		for (u8 direction = 0; direction < Direction_END; direction++) {
			const u64 direction_offset = BITSET_MAX_NOPAD2 * direction;
			for (u8 u = 0; u < BITSET_MAX_NOPAD; u++) {
				const u64 u_offset = direction_offset + BITSET_MAX_NOPAD * u;
				for (u8 v = 0; v < BITSET_MAX_NOPAD; v++) {
					bitset column = m_nodes_faces[u_offset + v];
					while (column) {
						const u8 first_filled = __builtin_ctzll(column);
						m_slices[direction_offset + BITSET_MAX_NOPAD * first_filled + u] |= 1ULL << v;
						column &= column - 1;
					}
				}
			}
		}

		MapNode n = node_types[node_key.content];
		const video::SColor color = encode_light(node_key.light, m_nodedef->getLightingFlags(n).light_source);

		generateBitsetMesh(n, width, seg_start - m_blockpos_nodes, color);
	}
}

void LodMeshGenerator::generateLodChunks(const std::bitset<NodeDrawType_END> types, const u8 width)
{
	ScopeProfiler sp(g_profiler, "Client: Mesh Making LOD Greedy", SPT_AVG);

	// split chunk into 62^3 segments to be able to use 64 bit ints as bitsets
	// the other two bits are used as padding to find node faces
	const int attempted_seg_size = BITSET_MAX_NOPAD * width;

	for (u16 x = 0; x < m_data->m_side_length; x += attempted_seg_size)
	for (u16 y = 0; y < m_data->m_side_length; y += attempted_seg_size)
	for (u16 z = 0; z < m_data->m_side_length; z += attempted_seg_size) {
		const v3s16 seg_start(
			x + m_blockpos_nodes.X,
			y + m_blockpos_nodes.Y,
			z + m_blockpos_nodes.Z);
		const v3s16 seg_size(
			std::min(m_data->m_side_length - x - 1, attempted_seg_size),
			std::min(m_data->m_side_length - y - 1, attempted_seg_size),
			std::min(m_data->m_side_length - z - 1, attempted_seg_size));
		generateGreedyLod(types, seg_start, seg_size, width);
	}
}

void LodMeshGenerator::generate(const u8 lod)
{
	ZoneScoped;

	// cap LODs to 8, since there is no use larger than 256 node LODs
    u8 width = 1 << MYMIN(lod - 1, 7);

	// cap LODs width to chunk size to account for different mesh chunk settings
	if (width > m_data->m_side_length)
		width = m_data->m_side_length;

	// transparents are always rendered separately
	std::bitset<NodeDrawType_END> transparent_set;
	transparent_set.set(NDT_LIQUID);
	if (g_settings->get("leaves_style") == "simple")
		transparent_set.set(NDT_GLASSLIKE);

	generateLodChunks(transparent_set, width);

	std::bitset<NodeDrawType_END> solid_set;
	solid_set.set(NDT_NORMAL);
	solid_set.set(NDT_NODEBOX);
	solid_set.set(NDT_ALLFACES);

	generateLodChunks(solid_set, width);
}
