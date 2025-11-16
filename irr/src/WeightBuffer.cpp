#include "WeightBuffer.h"

#include <algorithm>
#include <numeric>

namespace scene {

void WeightBuffer::VertexWeights::addWeight(u16 joint_id, f32 weight)
{
	assert(weight >= 0.0f);
	auto min_weight = std::min_element(strengths.begin(), strengths.end());
	if (*min_weight > weight)
		return;

	*min_weight = weight;
	joint_ids[std::distance(strengths.begin(), min_weight)] = joint_id;
}

void WeightBuffer::addWeight(u32 vertex_id, u16 joint_id, f32 weight)
{
	weights[vertex_id].addWeight(joint_id, weight);
}

void WeightBuffer::VertexWeights::skinVertex(core::vector3df &pos, core::vector3df &normal,
		const std::vector<core::matrix4> &joint_transforms) const
{
	f32 total_weight = 0.0f;
	core::vector3df skinned_pos;
	core::vector3df skinned_normal;
	for (u16 i = 0; i < MAX_WEIGHTS_PER_VERTEX; ++i) {
		u16 joint_id = joint_ids[i];
		f32 weight = strengths[i];

		const auto &transform = joint_transforms[joint_id];
		core::vector3df transformed_pos = pos;
		transform.transformVect(transformed_pos);
		skinned_pos += weight * transformed_pos;
		skinned_normal += weight * transform.rotateAndScaleVect(normal);
		total_weight += weight;
	}
	if (core::equals(total_weight, 0.0f))
		return;

	pos = skinned_pos;
	// Need to renormalize normal after potentially scaling
	normal = skinned_normal.normalize();
}

void WeightBuffer::skinVertex(u32 vertex_id, core::vector3df &pos, core::vector3df &normal,
		const std::vector<core::matrix4> &joint_transforms) const
{
	return weights[vertex_id].skinVertex(pos, normal, joint_transforms);
}

void WeightBuffer::skin(IVertexBuffer *dst,
		const std::vector<core::matrix4> &joint_transforms) const
{
	assert(animated_vertices.has_value());
	for (u32 i = 0; i < animated_vertices->size(); ++i) {

		u32 vertex_id = (*animated_vertices)[i];
		auto pos = static_positions[i];
		auto normal = static_normals[i];
		skinVertex(vertex_id, pos, normal, joint_transforms);
		dst->getPosition(vertex_id) = pos;
		dst->getNormal(vertex_id) = normal;
	}
	if (!animated_vertices->empty())
		dst->setDirty();
}

void WeightBuffer::finalize()
{
	// Normalizes weights so that they sum to 1.0 per vertex,
	// stores which vertices are animated.
	assert(!animated_vertices.has_value());
	animated_vertices.emplace();
	for (u32 i = 0; i < size(); ++i) {
		auto &strengths = weights[i].strengths;
		f32 total_weight = std::accumulate(strengths.begin(), strengths.end(), 0.0f);
		if (core::equals(total_weight, 0.0f)) {
			std::fill(strengths.begin(), strengths.end(), 0.0f);
			continue;
		}
		animated_vertices->emplace_back(i);
		if (core::equals(total_weight, 1.0f))
			continue;
		for (auto &strength : strengths)
			strength /= total_weight;
	}
	animated_vertices->shrink_to_fit();
}

void WeightBuffer::updateStaticPose(const IVertexBuffer *vbuf)
{
	if (!static_normals)
		static_normals = std::make_unique<core::vector3df[]>(animated_vertices->size());
	if (!static_positions)
		static_positions = std::make_unique<core::vector3df[]>(animated_vertices->size());
	for (size_t idx = 0; idx < animated_vertices->size(); ++idx) {
		u32 vertex_id = (*animated_vertices)[idx];
		static_positions[idx] = vbuf->getPosition(vertex_id);
		static_normals[idx] = vbuf->getNormal(vertex_id);
	}
}

void WeightBuffer::resetToStatic(IVertexBuffer *vbuf) const
{
	assert(animated_vertices.has_value());
	for (size_t idx = 0; idx < animated_vertices->size(); ++idx) {
		u32 vertex_id = (*animated_vertices)[idx];
		vbuf->getPosition(vertex_id) = static_positions[idx];
		vbuf->getNormal(vertex_id) = static_normals[idx];
	}
	if (!animated_vertices->empty())
		vbuf->setDirty();
}

} // end namespace scene
