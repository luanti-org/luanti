// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "collector.h"
#include <stdexcept>
#include <cassert>

bool PreMeshBuffer::append(const PreMeshBuffer &other)
{
	const size_t nv = vertices.size();
	const size_t ni = indices.size();
	if (nv + other.vertices.size() > U16_MAX)
		return false;

	vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
	indices.insert(indices.end(), other.indices.begin(), other.indices.end());
	for (size_t i = ni; i < indices.size(); i++)
		indices[i] += nv;
	return true;
}

void MeshCollector::append(const TileSpec &tile, const video::S3DVertex *vertices,
		u32 numVertices, const u16 *indices, u32 numIndices)
{
	for (int layernum = 0; layernum < MAX_TILE_LAYERS; layernum++) {
		const TileLayer &layer = tile.layers[layernum];
		if (layer.empty())
			continue;
		append(layer, vertices, numVertices, indices, numIndices, layernum);
	}
}

void MeshCollector::append(const TileLayer &layer, const video::S3DVertex *vertices,
		u32 numVertices, const u16 *indices, u32 numIndices, u8 layernum)
{
	PreMeshBuffer &p = findBuffer(layer, layernum, numVertices);

	const u16 aux = layer.texture_layer_idx;

	u32 vertex_count = p.vertices.size();
	assert(vertex_count + numVertices <= U16_MAX);
	for (u32 i = 0; i < numVertices; i++) {
		p.vertices.emplace_back(vertices[i].Pos + offset, vertices[i].Normal,
				vertices[i].Color, vertices[i].TCoords, aux);
		m_bounding_radius_sq = std::max(m_bounding_radius_sq,
				(vertices[i].Pos - m_center_pos).getLengthSQ());
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
