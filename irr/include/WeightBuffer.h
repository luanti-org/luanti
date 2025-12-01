// Copyright (C) 2025 Lars Müller
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "vector3d.h"
#include "matrix4.h"
#include "IVertexBuffer.h"

#include <cassert>
#include <memory>
#include <optional>
#include <vector>

namespace scene
{

struct WeightBuffer
{
	constexpr static u16 MAX_WEIGHTS_PER_VERTEX = 4;
	// ID-weight pairs for a joint
	struct VertexWeights {
		std::array<u16, MAX_WEIGHTS_PER_VERTEX> joint_ids = {};
		std::array<f32, MAX_WEIGHTS_PER_VERTEX> weights = {};
		void addWeight(u16 joint_id, f32 weight);
		/// Transform given position and normal with these weights
		void skinVertex(core::vector3df &pos, core::vector3df &normal,
				const std::vector<core::matrix4> &joint_transforms) const;
	};
	std::vector<VertexWeights> weights;

	std::optional<std::vector<u32>> animated_vertices;

	// A bit of a hack for now: Store static positions here so we can use them for skinning.
	// Ideally we might want a design where we do not mutate the original vertex buffer at all.
	std::unique_ptr<core::vector3df[]> static_positions;
	std::unique_ptr<core::vector3df[]> static_normals;

	WeightBuffer(size_t n_verts) : weights(n_verts) {}

	const std::array<u16, MAX_WEIGHTS_PER_VERTEX> &getJointIds(u32 vertex_id) const
	{ return weights[vertex_id].joint_ids; }

	const std::array<f32, MAX_WEIGHTS_PER_VERTEX> &getWeights(u32 vertex_id) const
	{ return weights[vertex_id].weights; }

	size_t size() const
	{ return weights.size(); }

	void addWeight(u32 vertex_id, u16 joint_id, f32 weight);

	/// Transform position and normal using the weights of the given vertex
	void skinVertex(u32 vertex_id, core::vector3df &pos, core::vector3df &normal,
			const std::vector<core::matrix4> &joint_transforms) const;

	/// @note src and dst can be the same buffer
	void skin(IVertexBuffer *dst,
			const std::vector<core::matrix4> &joint_transforms) const;

	/// Prepares this buffer for use in skinning.
	void finalize();

	void updateStaticPose(const IVertexBuffer *vbuf);

	void resetToStatic(IVertexBuffer *vbuf) const;
};

} // end namespace scene
