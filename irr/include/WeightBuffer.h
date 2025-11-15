// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Lars Müller

#pragma once

#include "vector3d.h"
#include "matrix4.h"
#include "IVertexBuffer.h"

#include <cassert>
#include <memory>
#include <optional>

namespace scene
{

struct WeightBuffer
{
	constexpr static u16 MAX_WEIGHTS_PER_VERTEX = 4;
	size_t n_verts;
	std::unique_ptr<u16[]> joint_idxs;
	std::unique_ptr<f32[]> weights;

	std::optional<std::vector<u32>> animated_vertices;

	// A bit of a hack for now: Store static positions here so we can use them for skinning.
	// Ideally we might want a design where we do not mutate the original vertex buffer at all.
	std::unique_ptr<core::vector3df[]> static_positions;
	std::unique_ptr<core::vector3df[]> static_normals;

	WeightBuffer(size_t n_verts);

	u16 *getJointIndices(u32 vertex_id)
	{ return &joint_idxs[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }
	const u16 *getJointIndices(u32 vertex_id) const
	{ return &joint_idxs[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }

	f32 *getWeights(u32 vertex_id)
	{ return &weights[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }
	const f32 *getWeights(u32 vertex_id) const
	{ return &weights[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }

	void addWeight(u32 vertex_id, u16 joint_id, f32 weight);

	void skinVertex(u32 vertex_id, core::vector3df &pos, core::vector3df &normal,
			const std::vector<core::matrix4> &joint_transforms) const;

	/// @note src and dst can be the same buffer
	void skin(IVertexBuffer *dst,
			const std::vector<core::matrix4> &joint_transforms) const;

	/// Normalizes weights so that they sum to 1.0 per vertex,
	/// stores which vertices are animated.
	void finalize();

	void updateStaticPose(const IVertexBuffer *vbuf);

	void resetToStatic(IVertexBuffer *vbuf) const;
};

} // end namespace scene
