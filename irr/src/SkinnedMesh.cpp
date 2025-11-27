// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "SkinnedMesh.h"
#include "SSkinMeshBuffer.h"
#include "Transform.h"
#include "aabbox3d.h"
#include "matrix4.h"
#include "os.h"
#include "vector3d.h"
#include <cassert>
#include <cstddef>
#include <variant>
#include <vector>
#include <cassert>
#include <stdexcept>

namespace scene
{

//! destructor
SkinnedMesh::~SkinnedMesh()
{
	for (auto *joint : AllJoints)
		delete joint;

	for (auto *buffer : LocalBuffers) {
		if (buffer)
			buffer->drop();
	}
}

f32 SkinnedMesh::getMaxFrameNumber() const
{
	return EndFrame;
}

// Keyframe Animation


using VariantTransform = SkinnedMesh::SJoint::VariantTransform;
std::vector<VariantTransform> SkinnedMesh::animateMesh(f32 frame)
{
	assert(HasAnimation);
	std::vector<VariantTransform> result;
	result.reserve(AllJoints.size());
	for (auto *joint : AllJoints)
		result.push_back(joint->animate(frame));
	return result;
}

core::aabbox3df SkinnedMesh::calculateBoundingBox(
		const std::vector<core::matrix4> &global_transforms)
{
	assert(global_transforms.size() == AllJoints.size());
	core::aabbox3df result = StaticPartsBox;
	// skeletal animation
	for (u16 i = 0; i < AllJoints.size(); ++i) {
		auto box = AllJoints[i]->LocalBoundingBox;
		global_transforms[i].transformBoxEx(box);
		result.addInternalBox(box);
	}
	// rigid animation
	for (u16 i = 0; i < AllJoints.size(); ++i) {
		for (u32 j : AllJoints[i]->AttachedMeshes) {
			auto box = (*SkinningBuffers)[j]->BoundingBox;
			global_transforms[i].transformBoxEx(box);
			result.addInternalBox(box);
		}
	}
	return result;
}

// Software Skinning

void SkinnedMesh::skinMesh(const std::vector<core::matrix4> &global_matrices)
{
	if (!HasAnimation)
		return;

	// rigid animation
	for (size_t i = 0; i < AllJoints.size(); ++i) {
		auto *joint = AllJoints[i];
		for (u32 attachedMeshIdx : joint->AttachedMeshes) {
			SSkinMeshBuffer *Buffer = (*SkinningBuffers)[attachedMeshIdx];
			Buffer->Transformation = global_matrices[i];
		}
	}

	// Premultiply with global inversed matrices, if present
	// (which they should be for joints with weights)
	std::vector<core::matrix4> joint_transforms = global_matrices;
	for (u16 i = 0; i < AllJoints.size(); ++i) {
		auto *joint = AllJoints[i];
		if (joint->GlobalInversedMatrix)
			joint_transforms[i] = joint_transforms[i] * (*joint->GlobalInversedMatrix);
	}

	for (auto *buffer : *SkinningBuffers) {
		if (buffer->Weights)
			buffer->Weights->skin(buffer->getVertexBuffer(), joint_transforms);
	}
}

//! Gets joint count.
u32 SkinnedMesh::getJointCount() const
{
	return AllJoints.size();
}

//! Gets the name of a joint.
const std::optional<std::string> &SkinnedMesh::getJointName(u32 number) const
{
	if (number >= getJointCount()) {
		static const std::optional<std::string> nullopt;
		return nullopt;
	}
	return AllJoints[number]->Name;
}

//! Gets a joint number from its name
std::optional<u32> SkinnedMesh::getJointNumber(const std::string &name) const
{
	for (u32 i = 0; i < AllJoints.size(); ++i) {
		if (AllJoints[i]->Name == name)
			return i;
	}

	return std::nullopt;
}

//! returns amount of mesh buffers.
u32 SkinnedMesh::getMeshBufferCount() const
{
	return LocalBuffers.size();
}

//! returns pointer to a mesh buffer
IMeshBuffer *SkinnedMesh::getMeshBuffer(u32 nr) const
{
	if (nr < LocalBuffers.size())
		return LocalBuffers[nr];
	else
		return 0;
}

//! Returns pointer to a mesh buffer which fits a material
IMeshBuffer *SkinnedMesh::getMeshBuffer(const video::SMaterial &material) const
{
	for (u32 i = 0; i < LocalBuffers.size(); ++i) {
		if (LocalBuffers[i]->getMaterial() == material)
			return LocalBuffers[i];
	}
	return nullptr;
}

u32 SkinnedMesh::getTextureSlot(u32 meshbufNr) const
{
	return TextureSlots.at(meshbufNr);
}

//! set the hardware mapping hint, for driver
void SkinnedMesh::setHardwareMappingHint(E_HARDWARE_MAPPING newMappingHint,
		E_BUFFER_TYPE buffer)
{
	for (u32 i = 0; i < LocalBuffers.size(); ++i)
		LocalBuffers[i]->setHardwareMappingHint(newMappingHint, buffer);
}

//! flags the meshbuffer as changed, reloads hardware buffers
void SkinnedMesh::setDirty(E_BUFFER_TYPE buffer)
{
	for (u32 i = 0; i < LocalBuffers.size(); ++i)
		LocalBuffers[i]->setDirty(buffer);
}

void SkinnedMesh::updateStaticPose()
{
	for (auto *buf : LocalBuffers) {
		if (buf->Weights)
			buf->Weights->updateStaticPose(buf->getVertexBuffer());
	}
}

void SkinnedMesh::resetAnimation()
{
	// copy from the cache to the mesh...
	for (auto *buf : LocalBuffers) {
		if (buf->Weights)
			buf->Weights->resetToStatic(buf->getVertexBuffer());
	}
}

//! Turns the given array of local matrices into an array of global matrices
//! by multiplying with respective parent matrices.
void SkinnedMesh::calculateGlobalMatrices(std::vector<core::matrix4> &matrices) const
{
	// Note that the joints are topologically sorted.
	for (u16 i = 0; i < AllJoints.size(); ++i) {
		if (auto parent_id = AllJoints[i]->ParentJointID) {
			matrices[i] = matrices[*parent_id] * matrices[i];
		}
	}
}

bool SkinnedMesh::checkForAnimation() const
{
	for (auto *joint : AllJoints) {
		if (!joint->keys.empty()) {
			return true;
		}
	}

	// meshes with weights are animatable
	for (auto *buf : LocalBuffers) {
		if (buf->Weights) {
			return true;
		}
	}

	return false;
}

void SkinnedMesh::prepareForSkinning()
{
	HasAnimation = checkForAnimation();
	if (!HasAnimation || PreparedForSkinning)
		return;

	PreparedForSkinning = true;

	EndFrame = 0.0f;
	for (const auto *joint : AllJoints) {
		EndFrame = std::max(EndFrame, joint->keys.getEndFrame());
	}

	for (auto *joint : AllJoints) {
		joint->keys.cleanup();
	}
}

void SkinnedMesh::calculateStaticBoundingBox()
{
	bool first = true;
	std::vector<bool> animated;
	for (u16 mb = 0; mb < getMeshBufferCount(); mb++) {
		auto *buf = LocalBuffers[mb];
		animated.clear();
		animated.resize(buf->getVertexCount(), false);
		if (buf->Weights) {
			for (u32 vert_id : buf->Weights->animated_vertices.value()) {
				animated[vert_id] = true;
			}
		}
		for (u32 v = 0; v < buf->getVertexCount(); v++) {
			if (animated[v])
				continue;

			auto pos = getMeshBuffer(mb)->getVertexBuffer()->getPosition(v);
			if (!first) {
				StaticPartsBox.addInternalPoint(pos);
			} else {
				StaticPartsBox.reset(pos);
				first = false;
			}
		}
	}
}

void SkinnedMesh::calculateJointBoundingBoxes()
{
	std::vector<std::optional<core::aabbox3df>> joint_boxes(AllJoints.size());
	for (auto *buf : LocalBuffers) {
		const auto &weights = buf->Weights;
		if (!weights)
			continue;
		for (u32 vert_id : weights->animated_vertices.value()) {
			const auto pos = buf->getVertex(vert_id)->Pos;
			for (u16 j = 0; j < WeightBuffer::MAX_WEIGHTS_PER_VERTEX; j++) {
				const u16 joint_id = weights->getJointIds(vert_id)[j];
				const SJoint *joint = AllJoints[joint_id];
				const f32 weight = weights->getWeights(vert_id)[j];
				if (core::equals(weight, 0.0f))
					continue;
				auto trans_pos = pos;
				joint->GlobalInversedMatrix.value().transformVect(trans_pos);
				if (joint_boxes[joint_id]) {
					joint_boxes[joint_id]->addInternalPoint(trans_pos);
				} else {
					joint_boxes[joint_id] = core::aabbox3df{trans_pos};
				}
			}
		}
	}
	for (u16 joint_id = 0; joint_id < AllJoints.size(); joint_id++) {
		auto *joint = AllJoints[joint_id];
		joint->LocalBoundingBox = joint_boxes[joint_id].value_or(core::aabbox3df{{0,0,0}});
	}
}

void SkinnedMesh::calculateBufferBoundingBoxes()
{
	for (u32 j = 0; j < LocalBuffers.size(); ++j) {
		// If we use skeletal animation, this will just be a bounding box of the static pose;
		// if we use rigid animation, this will correctly transform the points first.
		LocalBuffers[j]->recalculateBoundingBox();
	}
}

void SkinnedMesh::recalculateBaseBoundingBoxes() {
	calculateStaticBoundingBox();
	calculateJointBoundingBoxes();
	calculateBufferBoundingBoxes();
}

void SkinnedMeshBuilder::topoSortJoints()
{
	auto &joints = getJoints();
	const size_t n = joints.size();

	std::vector<u16> new_to_old_id;

	std::vector<std::vector<u16>> children(n);
	for (u16 i = 0; i < n; ++i) {
		if (auto parentId = joints[i]->ParentJointID)
			children[*parentId].push_back(i);
		else
		 	new_to_old_id.push_back(i);
	}

	// Levelorder
	for (u16 i = 0; i < n; ++i) {
		new_to_old_id.insert(new_to_old_id.end(),
				children[new_to_old_id[i]].begin(),
				children[new_to_old_id[i]].end());
	}

	std::vector<u16> old_to_new_id(n);
	for (u16 i = 0; i < n; ++i)
		old_to_new_id[new_to_old_id[i]] = i;

	std::vector<SJoint *> sorted_joints(n);
	for (u16 i = 0; i < n; ++i) {
		auto *joint = joints[new_to_old_id[i]];
		if (auto parentId = joint->ParentJointID)
			joint->ParentJointID = old_to_new_id[*parentId];
		sorted_joints[i] = joint;
		joint->JointID = i;
	}
	// Verify that the topological ordering is correct
	for (u16 i = 0; i < n; ++i) {
		if (auto pjid = sorted_joints[i]->ParentJointID)
			assert(*pjid < i);
	}
	getJoints() = std::move(sorted_joints);

	for (auto &weight : weights) {
		weight.joint_id = old_to_new_id[weight.joint_id];
	}
}

//! called by loader after populating with mesh and bone data
SkinnedMesh *SkinnedMeshBuilder::finalize() &&
{
	os::Printer::log("Skinned Mesh - finalize", ELL_DEBUG);

	// Topologically sort the joints such that parents come before their children.
	// From this point on, transformations can be calculated in linear order.
	// (see e.g. SkinnedMesh::calculateGlobalMatrices)
	topoSortJoints();

	mesh->prepareForSkinning();

	std::vector<core::matrix4> matrices;
	matrices.reserve(getJoints().size());
	for (auto *joint : getJoints()) {
		if (const auto *matrix = std::get_if<core::matrix4>(&joint->transform))
			matrices.push_back(*matrix);
		else
		 	matrices.push_back(std::get<core::Transform>(joint->transform).buildMatrix());
	}
	mesh->calculateGlobalMatrices(matrices);

	for (size_t i = 0; i < getJoints().size(); ++i) {
		auto *joint = getJoints()[i];
		if (!joint->GlobalInversedMatrix) {
			joint->GlobalInversedMatrix = matrices[i];
			joint->GlobalInversedMatrix->makeInverse();
		}
		// rigid animation for non animated meshes
		for (u32 attachedMeshIdx : joint->AttachedMeshes) {
			SSkinMeshBuffer *Buffer = (*mesh->SkinningBuffers)[attachedMeshIdx];
			Buffer->Transformation = matrices[i];
		}
	}

	for (const auto &weight : weights) {
		auto *buf = mesh->LocalBuffers.at(weight.buffer_id);
		if (!buf->Weights)
			buf->Weights = WeightBuffer(buf->getVertexCount());
		buf->Weights->addWeight(weight.vertex_id, weight.joint_id, weight.strength);
	}

	for (auto *buffer : mesh->LocalBuffers) {
		if (buffer->Weights)
			buffer->Weights->finalize();
	}
	mesh->updateStaticPose();

	mesh->recalculateBaseBoundingBoxes();
	mesh->StaticPoseBox = mesh->calculateBoundingBox(matrices);

	return mesh.release();
}

scene::SSkinMeshBuffer *SkinnedMeshBuilder::addMeshBuffer()
{
	scene::SSkinMeshBuffer *buffer = new scene::SSkinMeshBuffer();
	mesh->TextureSlots.push_back(mesh->LocalBuffers.size());
	mesh->LocalBuffers.push_back(buffer);
	return buffer;
}

u32 SkinnedMeshBuilder::addMeshBuffer(SSkinMeshBuffer *meshbuf)
{
	mesh->TextureSlots.push_back(mesh->LocalBuffers.size());
	mesh->LocalBuffers.push_back(meshbuf);
	return mesh->getMeshBufferCount() - 1;
}

SkinnedMesh::SJoint *SkinnedMeshBuilder::addJoint(SJoint *parent)
{
	SJoint *joint = new SJoint;
	joint->setParent(parent);

	joint->JointID = getJoints().size();
	getJoints().push_back(joint);

	return joint;
}

void SkinnedMeshBuilder::addPositionKey(SJoint *joint, f32 frame, core::vector3df pos)
{
	assert(joint);
	joint->keys.position.pushBack(frame, pos);
}

void SkinnedMeshBuilder::addScaleKey(SJoint *joint, f32 frame, core::vector3df scale)
{
	assert(joint);
	joint->keys.scale.pushBack(frame, scale);
}

void SkinnedMeshBuilder::addRotationKey(SJoint *joint, f32 frame, core::quaternion rot)
{
	assert(joint);
	joint->keys.rotation.pushBack(frame, rot);
}

void SkinnedMeshBuilder::addWeight(SJoint *joint, u16 buf_id, u32 vert_id, f32 strength)
{
	assert(joint);
	if (strength <= 0.0f)
		return;
	weights.emplace_back(Weight{joint->JointID, buf_id, vert_id, strength});
}

void SkinnedMesh::convertMeshToTangents()
{
	// now calculate tangents
	for (u32 b = 0; b < LocalBuffers.size(); ++b) {
		if (LocalBuffers[b]) {
			LocalBuffers[b]->convertToTangents();

			const s32 idxCnt = LocalBuffers[b]->getIndexCount();

			u16 *idx = LocalBuffers[b]->getIndices();
			video::S3DVertexTangents *v =
					(video::S3DVertexTangents *)LocalBuffers[b]->getVertices();

			for (s32 i = 0; i < idxCnt; i += 3) {
				calculateTangents(
						v[idx[i + 0]].Normal,
						v[idx[i + 0]].Tangent,
						v[idx[i + 0]].Binormal,
						v[idx[i + 0]].Pos,
						v[idx[i + 1]].Pos,
						v[idx[i + 2]].Pos,
						v[idx[i + 0]].TCoords,
						v[idx[i + 1]].TCoords,
						v[idx[i + 2]].TCoords);

				calculateTangents(
						v[idx[i + 1]].Normal,
						v[idx[i + 1]].Tangent,
						v[idx[i + 1]].Binormal,
						v[idx[i + 1]].Pos,
						v[idx[i + 2]].Pos,
						v[idx[i + 0]].Pos,
						v[idx[i + 1]].TCoords,
						v[idx[i + 2]].TCoords,
						v[idx[i + 0]].TCoords);

				calculateTangents(
						v[idx[i + 2]].Normal,
						v[idx[i + 2]].Tangent,
						v[idx[i + 2]].Binormal,
						v[idx[i + 2]].Pos,
						v[idx[i + 0]].Pos,
						v[idx[i + 1]].Pos,
						v[idx[i + 2]].TCoords,
						v[idx[i + 0]].TCoords,
						v[idx[i + 1]].TCoords);
			}
		}
	}
}

void SkinnedMesh::calculateTangents(
		core::vector3df &normal,
		core::vector3df &tangent,
		core::vector3df &binormal,
		const core::vector3df &vt1, const core::vector3df &vt2, const core::vector3df &vt3, // vertices
		const core::vector2df &tc1, const core::vector2df &tc2, const core::vector2df &tc3) // texture coords
{
	core::vector3df v1 = vt1 - vt2;
	core::vector3df v2 = vt3 - vt1;
	normal = v2.crossProduct(v1);
	normal.normalize();

	// binormal

	f32 deltaX1 = tc1.X - tc2.X;
	f32 deltaX2 = tc3.X - tc1.X;
	binormal = (v1 * deltaX2) - (v2 * deltaX1);
	binormal.normalize();

	// tangent

	f32 deltaY1 = tc1.Y - tc2.Y;
	f32 deltaY2 = tc3.Y - tc1.Y;
	tangent = (v1 * deltaY2) - (v2 * deltaY1);
	tangent.normalize();

	// adjust

	core::vector3df txb = tangent.crossProduct(binormal);
	if (txb.dotProduct(normal) < 0.0f) {
		tangent *= -1.0f;
		binormal *= -1.0f;
	}
}

} // end namespace scene
