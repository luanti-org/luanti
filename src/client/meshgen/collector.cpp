// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "collector.h"
#include <stdexcept>
#include "log.h"
#include "client/mesh.h"

void MeshCollector::append(const TileSpec &tile, const video::S3DVertex *vertices,
		u32 numVertices, const u16 *indices, u32 numIndices)
{
	for (int layernum = 0; layernum < MAX_TILE_LAYERS; layernum++) {
		const TileLayer *layer = &tile.layers[layernum];
		if (layer->texture_id == 0)
			continue;
		append(*layer, vertices, numVertices, indices, numIndices, layernum,
				tile.world_aligned);
	}
}

void MeshCollector::append(const TileLayer &layer, const video::S3DVertex *vertices,
		u32 numVertices, const u16 *indices, u32 numIndices, u8 layernum,
		bool use_scale)
{
	PreMeshBuffer &p = findBuffer(layer, layernum, numVertices);

	f32 scale = 1.0f;
	if (use_scale)
		scale = 1.0f / layer.scale;

	u32 vertex_count = p.vertices.size();
	for (u32 i = 0; i < numVertices; i++) {
		p.vertices.emplace_back(vertices[i].Pos + offset, vertices[i].Normal,
				vertices[i].Color, scale * vertices[i].TCoords);
		m_bounding_radius_sq = std::max(m_bounding_radius_sq,
				(vertices[i].Pos - m_center_pos).getLengthSQ());
	}

	for (u32 i = 0; i < numIndices; i++)
		p.indices.push_back(indices[i] + vertex_count);
}

void MeshCollector::append(const TileSpec &tile, const video::S3DVertex *vertices,
		u32 numVertices, const u16 *indices, u32 numIndices, v3f pos,
		video::SColor c, u8 light_source)
{
	for (int layernum = 0; layernum < MAX_TILE_LAYERS; layernum++) {
		const TileLayer *layer = &tile.layers[layernum];
		if (layer->texture_id == 0)
			continue;
		append(*layer, vertices, numVertices, indices, numIndices, pos, c,
				light_source, layernum, tile.world_aligned);
	}
}

void MeshCollector::append(const TileLayer &layer, const video::S3DVertex *vertices,
		u32 numVertices, const u16 *indices, u32 numIndices, v3f pos,
		video::SColor c, u8 light_source, u8 layernum, bool use_scale)
{
	PreMeshBuffer &p = findBuffer(layer, layernum, numVertices);

	f32 scale = 1.0f;
	if (use_scale)
		scale = 1.0f / layer.scale;

	u32 vertex_count = p.vertices.size();
	for (u32 i = 0; i < numVertices; i++) {
		video::SColor color = c;
		if (!light_source)
			applyFacesShading(color, vertices[i].Normal);
		auto vpos = vertices[i].Pos + pos + offset;
		p.vertices.emplace_back(vpos, vertices[i].Normal, color,
				scale * vertices[i].TCoords);
		m_bounding_radius_sq = std::max(m_bounding_radius_sq,
				(vpos - m_center_pos).getLengthSQ());
	}

	for (u32 i = 0; i < numIndices; i++)
		p.indices.push_back(indices[i] + vertex_count);
}

PreMeshBuffer &MeshCollector::findBuffer(
		const TileLayer &layer, u8 layernum, u32 numVertices)
{
	if (numVertices > U16_MAX)
		throw std::invalid_argument(
				"Mesh can't contain more than 65536 vertices");
	std::vector<PreMeshBuffer> &buffers = prebuffers[layernum];
	for (PreMeshBuffer &p : buffers)
		if (p.layer == layer && p.vertices.size() + numVertices <= U16_MAX)
			return p;
	buffers.emplace_back(layer);
	return buffers.back();
}
