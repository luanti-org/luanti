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

LodMeshGenerator::LodMeshGenerator(MeshMakeData *input, MeshCollector *output, bool is_mono_mat):
    m_data(input),
    m_collector(output),
    m_nodedef(m_data->m_nodedef),
    m_blockpos_nodes(m_data->m_blockpos * MAP_BLOCKSIZE),
	m_is_mono_mat(is_mono_mat)
{
}

void LodMeshGenerator::generateBitsetMesh(const MapNode n, const u8 width,
		const v3s16 seg_start, const video::SColor color)
{
	const core::vector3df seg_offset(seg_start.X * BS, seg_start.Y * BS, seg_start.Z * BS);
	const f32 scaled_BS = BS * width;

	core::vector3df vertices[4];
	video::S3DVertex irr_vertices[4];
	for (u8 direction = 0; direction < 6; direction++) {
		TileSpec tile;
		if (!m_is_mono_mat)
			getNodeTile(n, m_blockpos_nodes, s_directions[direction], m_data, tile);
		const u64 direction_offset = BITSET_MAX_NOPAD2 * direction;
		for (u8 slice_i = 0; slice_i < BITSET_MAX_NOPAD; slice_i++) {
			const u64 slice_offset = direction_offset + BITSET_MAX_NOPAD * slice_i;
			for (u8 u = 0; u < BITSET_MAX_NOPAD; u++) {
				bitset column = m_slices[slice_offset + u];
				while (column) {
					s32 v0 = std::__countr_zero(column);
					s32 v1 = std::__countr_one(column >> v0);
					const bitset mask = ((1ULL << v1) - 1) << v0;
					column ^= mask;
					s32 u1 = 1;
					while (u + u1 < BITSET_MAX_NOPAD && // while still in current chunk
						(m_slices[slice_offset + u + u1] & mask) == mask) {
						// and next column shares faces
						m_slices[slice_offset + u + u1] ^= mask;
						u1++;
					}
					const core::vector2d<f32> uvs[4] = {
						core::vector2d<f32>{0, static_cast<f32>(v1*width)},
						core::vector2d<f32>{0, 0},
						core::vector2d<f32>{static_cast<f32>(u1*width), 0},
						core::vector2d<f32>{static_cast<f32>(u1*width), static_cast<f32>(v1*width)}
					};
					u1 = (u + u1) * scaled_BS - BS / 2;
					const s32 u0 = u * scaled_BS - BS / 2;
					v1 = (v0 + v1) * scaled_BS - BS / 2;
					v0 = v0 * scaled_BS - BS / 2;
					const s32 w = ((slice_i + 1) * width - 1
						+ (direction % 2 == 0 ? -width + 1 : 1)) * BS
						- BS / 2;
					switch (direction) {
					case 0:
						vertices[0] = core::vector3df(w, u0, v1);
						vertices[1] = core::vector3df(w, u1, v1);
						vertices[2] = core::vector3df(w, u1, v0);
						vertices[3] = core::vector3df(w, u0, v0);
						break;
					case 1:
						vertices[0] = core::vector3df(w, u0, v0);
						vertices[1] = core::vector3df(w, u0, v1);
						vertices[2] = core::vector3df(w, u1, v1);
						vertices[3] = core::vector3df(w, u1, v0);
						break;
					case 2:
					case 3:
						vertices[0] = core::vector3df(u0, w, v0);
						vertices[1] = core::vector3df(u1, w, v0);
						vertices[2] = core::vector3df(u1, w, v1);
						vertices[3] = core::vector3df(u0, w, v1);
						break;
					case 4:
						vertices[0] = core::vector3df(u0, v0, w);
						vertices[1] = core::vector3df(u0, v1, w);
						vertices[2] = core::vector3df(u1, v1, w);
						vertices[3] = core::vector3df(u1, v0, w);
						break;
					default:
						vertices[0] = core::vector3df(u1, v0, w);
						vertices[1] = core::vector3df(u0, v0, w);
						vertices[2] = core::vector3df(u0, v1, w);
						vertices[3] = core::vector3df(u1, v1, w);
						break;
					}
					for (core::vector3df& v : vertices)
						v += seg_offset;

					switch (direction) {
					case 0:
					case 2:
					case 4:
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
	bitset all_set_nodes[3 * BITSET_MAX * BITSET_MAX] = {0};
	std::unordered_map<NodeKey, MapNode> node_types;
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
		const ContentFeatures* f = &m_nodedef->get(n);
		for (u8 subtr = 1; subtr < width && f->drawtype == NDT_AIRLIKE; subtr++) {
			n = m_data->m_vmanip.getNodeNoExNoEmerge(p - subtr);
			f = &m_nodedef->get(n);
		}
		if (!types.test(f->drawtype)) {
			continue;
		}

		const content_t node_type = n.getContent();
		const v3s16 p_scaled = (p - seg_start + 1) / width;

		if (f->drawtype == NDT_NORMAL) {
			LightPair lp = computeMaxFaceLight(n, p, v3s16(max_light_step, 0, 0));
			NodeKey key = NodeKey{node_type, lp};
			node_types.try_emplace(key, n);
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
			node_types.try_emplace(key, n);
			auto &nodes = set_nodes[key];
			nodes[BITSET_MAX * p_scaled.Y + p_scaled.Z] |= 1ULL << p_scaled.X; // x axis
			nodes[BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Z] |= 1ULL << p_scaled.Y; // y axis
			nodes[2 * BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Y] |= 1ULL << p_scaled.Z; // z axis
		}

		all_set_nodes[BITSET_MAX * p_scaled.Y + p_scaled.Z] |= 1ULL << p_scaled.X; // x axis
		all_set_nodes[BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Z] |= 1ULL << p_scaled.Y; // y axis
		all_set_nodes[2 * BITSET_MAX2 + BITSET_MAX * p_scaled.X + p_scaled.Y] |= 1ULL << p_scaled.Z; // z axis
	}

	constexpr bitset U62_MAX = U64_MAX >> 2;

	for (const auto& [node_key, nodes] : set_nodes) {
		for (u8 u = 1; u <= BITSET_MAX_NOPAD; u++)
		for (u8 v = 1; v <= BITSET_MAX_NOPAD; v++) {
			m_nodes_faces[BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX * u + v] << 1))
				>> 1 & U62_MAX;

			m_nodes_faces[BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX * u + v] >> 1))
				>> 1 & U62_MAX;

			m_nodes_faces[2 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX2 + BITSET_MAX * u + v] << 1))
				>> 1 & U62_MAX;

			m_nodes_faces[3 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[BITSET_MAX2 + BITSET_MAX * u + v] >> 1))
				>> 1 & U62_MAX;

			m_nodes_faces[4 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] << 1))
				>> 1 & U62_MAX;

			m_nodes_faces[5 * BITSET_MAX_NOPAD2 + BITSET_MAX_NOPAD * (u - 1) + v - 1] =
				(nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] &
				~(all_set_nodes[2 * BITSET_MAX2 + BITSET_MAX * u + v] >> 1))
				>> 1 & U62_MAX;
		}

		memset(m_slices, 0, sizeof(m_slices));
		for (u8 direction = 0; direction < 6; direction++) {
			const u64 direction_offset = BITSET_MAX_NOPAD2 * direction;
			for (u8 u = 0; u < BITSET_MAX_NOPAD; u++) {
				const u64 u_offset = direction_offset + BITSET_MAX_NOPAD * u;
				for (u8 v = 0; v < BITSET_MAX_NOPAD; v++) {
					bitset column = m_nodes_faces[u_offset + v];
					while (column) {
						const u8 first_filled = std::__countr_zero(column);
						m_slices[direction_offset + BITSET_MAX_NOPAD * first_filled + u] |= 1ULL << v;
						column &= column - 1;
					}
				}
			}
		}

		MapNode n = node_types[node_key];
		video::SColor color = encode_light(node_key.light, m_nodedef->getLightingFlags(n).light_source);
		if (m_is_mono_mat) {
			video::SColor c2 = m_nodedef->get(n).minimap_color;
			color.set(
				color.getAlpha(),
				color.getRed() * c2.getRed() / 255U,
				color.getGreen() * c2.getGreen() / 255U,
				color.getBlue() * c2.getBlue() / 255U);
		}

		generateBitsetMesh(n, width, seg_start - m_blockpos_nodes, color);
	}
}

void LodMeshGenerator::generateLodChunks(const std::bitset<NodeDrawType_END> types, const u16 width)
{
	ScopeProfiler sp(g_profiler, "Client: Mesh Making LOD Greedy", SPT_AVG);

	const int attempted_seg_size = 62 * width;

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

    u32 width = 1 << MYMIN(lod - 1, 31);

	if (width > m_data->m_side_length)
		width = m_data->m_side_length;

	// liquids are always rendered separately
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
