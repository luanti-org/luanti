// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#pragma once
#include <array>
#include <vector>
#include "irrlichttypes.h"
#include "irr_v3d.h"
#include <S3DVertex.h>
#include "client/tile.h"

struct PreMeshBuffer
{
	TileLayer layer;
	std::vector<u16> indices;
	std::vector<video::S3DVertex> vertices;

	PreMeshBuffer() = default;
	explicit PreMeshBuffer(const TileLayer &layer) : layer(layer) {}
};

struct MeshCollector
{
	std::array<std::vector<PreMeshBuffer>, MAX_TILE_LAYERS> prebuffers;
	// bounding sphere radius and center
	f32 m_bounding_radius_sq = 0.0f;
	v3f m_center_pos;
	v3f offset;

	// center_pos: pos to use for bounding-sphere, in BS-space
	// offset: offset added to vertices
	MeshCollector(const v3f center_pos, v3f offset = v3f()) : m_center_pos(center_pos), offset(offset) {}

	void append(const TileSpec &material,
			const video::S3DVertex *vertices, u32 numVertices,
			const u16 *indices, u32 numIndices);
	void append(const TileSpec &material,
			const video::S3DVertex *vertices, u32 numVertices,
			const u16 *indices, u32 numIndices,
			v3f pos, video::SColor c, u8 light_source);

private:
	void append(const TileLayer &material,
			const video::S3DVertex *vertices, u32 numVertices,
			const u16 *indices, u32 numIndices,
			u8 layernum, bool use_scale = false);
	void append(const TileLayer &material,
			const video::S3DVertex *vertices, u32 numVertices,
			const u16 *indices, u32 numIndices,
			v3f pos, video::SColor c, u8 light_source,
			u8 layernum, bool use_scale = false);

	PreMeshBuffer &findBuffer(const TileLayer &layer, u8 layernum, u32 numVertices);
};
