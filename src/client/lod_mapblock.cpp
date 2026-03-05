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
#include "porting.h"
#include "mesh.h"
#include "node_visuals.h"

#include "profiler.h"
#include "SMesh.h"
#include "util/directiontables.h"

static constexpr u16 quad_indices_02[] = {0, 1, 2, 2, 3, 0};
static const auto &quad_indices = quad_indices_02;

LodMeshGenerator::LodMeshGenerator(MeshMakeData *input, MeshCollector *output, const bool is_textureless, const u32 solid_shader_id):
	m_data(input),
	m_collector(output),
	m_nodedef(m_data->m_nodedef),
	m_blockpos_nodes(m_data->m_blockpos * MAP_BLOCKSIZE),
	m_is_textureless(is_textureless)
{
	m_solid_tile.layers[0].shader_id = solid_shader_id;
}

void LodMeshGenerator::drawMeshNode()
{
	u8 facedir = 0;
	scene::IMesh* mesh;
	int degrotate = 0;
	video::SColor base_color = encode_light(LightPair(getInteriorLight(m_cur_node.n, 0, m_nodedef)), m_cur_node.f->light_source);

	if (m_cur_node.f->param_type_2 == CPT2_FACEDIR ||
			m_cur_node.f->param_type_2 == CPT2_COLORED_FACEDIR ||
			m_cur_node.f->param_type_2 == CPT2_4DIR ||
			m_cur_node.f->param_type_2 == CPT2_COLORED_4DIR) {
		facedir = m_cur_node.n.getFaceDir(m_nodedef);
	} else if (m_cur_node.f->param_type_2 == CPT2_WALLMOUNTED ||
			m_cur_node.f->param_type_2 == CPT2_COLORED_WALLMOUNTED) {
		// Convert wallmounted to 6dfacedir.
		facedir = m_cur_node.n.getWallMounted(m_nodedef);
		facedir = wallmounted_to_facedir[facedir];
	} else if (m_cur_node.f->param_type_2 == CPT2_DEGROTATE ||
			m_cur_node.f->param_type_2 == CPT2_COLORED_DEGROTATE) {
		degrotate = m_cur_node.n.getDegRotate(m_nodedef);
	}

	if (m_cur_node.f->visuals->mesh_ptr) {
		// clone and rotate mesh
		mesh = cloneStaticMesh(m_cur_node.f->visuals->mesh_ptr);
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
		getNodeTileN(m_cur_node.n, m_cur_node.p, MYMIN(tile_idx, 5), m_data, tile);

		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		video::S3DVertex *vertices = static_cast<video::S3DVertex*>(buf->getVertices());
		const u32 vertex_count = buf->getVertexCount();

		// Mesh is always private here. So the lighting is applied to each
		// vertex right here.
		const bool is_light_source = m_cur_node.f->light_source != 0;
		for (u32 k = 0; k < vertex_count; k++) {
			video::S3DVertex &vertex = vertices[k];
			video::SColor color = base_color;
			if (!is_light_source)
				applyFacesShading(color, vertex.Normal);
			vertex.Color = color;
			vertex.Pos.X += m_cur_node.p.X * BS;
			vertex.Pos.Y += m_cur_node.p.Y * BS;
			vertex.Pos.Z += m_cur_node.p.Z * BS;
		}
		m_collector->append(tile, vertices, vertex_count,
			buf->getIndices(), buf->getIndexCount());
	}
	std::ignore = mesh->drop();
}

void LodMeshGenerator::drawSolidNode()
{
	aabb3f box(v3f(-0.5 * BS), v3f(-0.5 * BS));
	box.MinEdge += intToFloat(m_cur_node.p + 1 - m_node_width + v3s16(0, m_node_width - (m_cur_node.p.Y % m_node_width) - 1, 0), BS);
	box.MaxEdge += intToFloat(m_cur_node.p + 1, BS);

	const bool is_liquid = m_cur_node.f->drawtype == NDT_LIQUID || m_cur_node.f->drawtype == NDT_FLOWINGLIQUID;
	const bool uses_textureless_tile = m_is_textureless && !(is_liquid && g_settings->getBool("enable_waving_water"));

	core::vector2d<f32> uvs[4];
	core::vector3df vertices[4];
	video::S3DVertex irr_vertices[4];

	for (int face = 0; face < Direction_END; face++) {
		v3s16 p2 = m_cur_node.p + tile_dirs[face] * m_node_width + m_blockpos_nodes;
		MapNode neighbor = m_data->m_vmanip.getNodeNoExNoEmerge(p2);
		const content_t n2 = neighbor.getContent();
		if (n2 == m_cur_node.n.getContent() || n2 == CONTENT_IGNORE)
			continue;
		if (n2 != CONTENT_AIR) {
			const ContentFeatures &f2 = m_nodedef->get(n2);
			if (f2.visuals->solidness == 2 || (is_liquid && m_cur_node.f->sameLiquidRender(f2)))
				continue;
		}

		const u16 light = getFaceLight(m_cur_node.n, neighbor, m_nodedef);
		video::SColor color = encode_light(light, m_cur_node.f->light_source);
		TileSpec tile;
		getNodeTileN(m_cur_node.n, m_blockpos_nodes, face, m_data, tile);

		if (uses_textureless_tile) {
			// When generating a mesh with no texture, we have to color the vertices instead of relying on the texture.
			video::SColor c2 = m_cur_node.f->visuals->average_colors[face];
			video::SColor c3 = tile.layers[0].color;
			color = video::SColor(
				color.getAlpha(),
				color.getRed() * c2.getRed() * c3.getRed() / 65025U,
				color.getGreen() * c2.getGreen() * c3.getGreen() / 65025U,
				color.getBlue() * c2.getBlue() * c3.getBlue() / 65025U);
		}

		switch (face) {
		case LEFT:
		case RIGHT:
		case DOWN:
		case BACK:
			uvs[0] = core::vector2d<f32>{0, static_cast<f32>(m_node_width)};
			uvs[1] = core::vector2d<f32>{0, 0};
			uvs[2] = core::vector2d<f32>{static_cast<f32>(m_node_width), 0};
			uvs[3] = core::vector2d<f32>{static_cast<f32>(m_node_width), static_cast<f32>(m_node_width)};
			break;
		default:
			uvs[0] = core::vector2d<f32>{0, static_cast<f32>(m_node_width)};
			uvs[1] = core::vector2d<f32>{0, 0};
			uvs[2] = core::vector2d<f32>{static_cast<f32>(m_node_width), 0};
			uvs[3] = core::vector2d<f32>{static_cast<f32>(m_node_width), static_cast<f32>(m_node_width)};
		}

		switch (face) {
		case UP:
			vertices[0] = core::vector3df(box.MaxEdge.X, box.MaxEdge.Y, box.MinEdge.Z);
			vertices[1] = core::vector3df(box.MinEdge.X, box.MaxEdge.Y, box.MinEdge.Z);
			vertices[2] = core::vector3df(box.MinEdge.X, box.MaxEdge.Y, box.MaxEdge.Z);
			vertices[3] = core::vector3df(box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z);
			break;
		case DOWN:
			vertices[0] = core::vector3df(box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z);
			vertices[1] = core::vector3df(box.MaxEdge.X, box.MinEdge.Y, box.MinEdge.Z);
			vertices[2] = core::vector3df(box.MaxEdge.X, box.MinEdge.Y, box.MaxEdge.Z);
			vertices[3] = core::vector3df(box.MinEdge.X, box.MinEdge.Y, box.MaxEdge.Z);
			break;
		case LEFT:
			vertices[0] = core::vector3df(box.MaxEdge.X, box.MinEdge.Y, box.MinEdge.Z);
			vertices[1] = core::vector3df(box.MaxEdge.X, box.MaxEdge.Y, box.MinEdge.Z);
			vertices[2] = core::vector3df(box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z);
			vertices[3] = core::vector3df(box.MaxEdge.X, box.MinEdge.Y, box.MaxEdge.Z);
			break;
		case RIGHT:
			vertices[0] = core::vector3df(box.MinEdge.X, box.MinEdge.Y, box.MaxEdge.Z);
			vertices[1] = core::vector3df(box.MinEdge.X, box.MaxEdge.Y, box.MaxEdge.Z);
			vertices[2] = core::vector3df(box.MinEdge.X, box.MaxEdge.Y, box.MinEdge.Z);
			vertices[3] = core::vector3df(box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z);
			break;
		case BACK:
			vertices[0] = core::vector3df(box.MaxEdge.X, box.MinEdge.Y, box.MaxEdge.Z);
			vertices[1] = core::vector3df(box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z);
			vertices[2] = core::vector3df(box.MinEdge.X, box.MaxEdge.Y, box.MaxEdge.Z);
			vertices[3] = core::vector3df(box.MinEdge.X, box.MinEdge.Y, box.MaxEdge.Z);
			break;
		default:
			vertices[0] = core::vector3df(box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z);
			vertices[1] = core::vector3df(box.MinEdge.X, box.MaxEdge.Y, box.MinEdge.Z);
			vertices[2] = core::vector3df(box.MaxEdge.X, box.MaxEdge.Y, box.MinEdge.Z);
			vertices[3] = core::vector3df(box.MaxEdge.X, box.MinEdge.Y, box.MinEdge.Z);
			break;
		}
		irr_vertices[0] = video::S3DVertex(vertices[0], s_normals[face], color, uvs[0]);
		irr_vertices[1] = video::S3DVertex(vertices[1], s_normals[face], color, uvs[1]);
		irr_vertices[2] = video::S3DVertex(vertices[2], s_normals[face], color, uvs[2]);
		irr_vertices[3] = video::S3DVertex(vertices[3], s_normals[face], color, uvs[3]);
		m_collector->append(uses_textureless_tile ? m_solid_tile : tile, irr_vertices, 4, quad_indices, 6);
	}
}

void LodMeshGenerator::drawNode()
{
	switch (m_cur_node.f->drawtype) {
	case NDT_MESH:
		if (!m_is_textureless)
			drawMeshNode();
		else
			drawSolidNode();
		break;
	case NDT_GLASSLIKE:
	case NDT_ALLFACES:
	case NDT_FLOWINGLIQUID:
	case NDT_LIQUID:
	case NDT_NODEBOX:
	case NDT_NORMAL:
		drawSolidNode();
	default:;
	}
}

void LodMeshGenerator::generate(const u8 lod)
{
	ZoneScoped;
	ScopeProfiler sp(g_profiler, "Client: Mesh Making LOD", SPT_AVG);

	// cap LODs to 7, since there is no use for such large LODs
	m_node_width = 1 << std::min(lod - 1, 6);

	// letting an lod node be larger than this results in too many holes in the mesh due to content_ignore
	if (m_node_width > 2 * std::log2(m_data->m_side_length))
		m_node_width = 2 * std::log2(m_data->m_side_length);

	// reduce LOD size until it actually divides the mesh width,
	// in case mesh width is not a power of two or if the LOD size previously got capped
	while (m_data->m_side_length % m_node_width != 0) m_node_width--;

	v3s16 p;
	for (p.Z = m_node_width - 1; p.Z < m_data->m_side_length; p.Z += m_node_width)
	for (p.Y = m_node_width - 1; p.Y < m_data->m_side_length; p.Y += m_node_width)
	for (p.X = m_node_width - 1; p.X < m_data->m_side_length; p.X += m_node_width) {
		if (!m_data->m_vmanip.m_area.contains(p + m_blockpos_nodes))
			continue;

		m_cur_node.p = p;
		m_cur_node.n = m_data->m_vmanip.getNodeRefUnsafeCheckFlags(m_blockpos_nodes + m_cur_node.p);
		m_cur_node.f = &m_nodedef->get(m_cur_node.n);

		for (u8 subtr = 1; subtr < m_node_width && m_cur_node.f->drawtype == NDT_AIRLIKE; subtr++) {
			// this assumes that we always have entire blocks emerged for meshgen
			m_cur_node.p = p - v3s16(0, subtr, 0);
			m_cur_node.n = m_data->m_vmanip.getNodeNoExNoEmerge(m_cur_node.p + m_blockpos_nodes);
			m_cur_node.f = &m_nodedef->get(m_cur_node.n);
		}
		drawNode();
	}
}
