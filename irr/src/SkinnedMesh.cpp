// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "SkinnedMesh.h"
#include "IBoneSceneNode.h"
#include "CBoneSceneNode.h"
#include "IAnimatedMeshSceneNode.h"
#include "SSkinMeshBuffer.h"
#include "Transform.h"
#include "aabbox3d.h"
#include "irrMath.h"
#include "matrix4.h"
#include "os.h"
#include "vector3d.h"
#include <cassert>
#include <cstddef>
#include <variant>
#include <vector>
#include <cassert>

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

//! Gets the default animation speed of the animated mesh.
/** \return Amount of frames per second. If the amount is 0, it is a static, non animated mesh. */
f32 SkinnedMesh::getAnimationSpeed() const
{
	return FramesPerSecond;
}

//! Gets the frame count of the animated mesh.
/** \param fps Frames per second to play the animation with. If the amount is 0, it is not animated.
The actual speed is set in the scene node the mesh is instantiated in.*/
void SkinnedMesh::setAnimationSpeed(f32 fps)
{
	FramesPerSecond = fps;
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

	// clear skinning helper array
	for (std::vector<char> &buf : Vertices_Moved)
		std::fill(buf.begin(), buf.end(), false);

	// skin starting with the root joints
	for (size_t i = 0; i < AllJoints.size(); ++i) {
		auto *joint = AllJoints[i];
		if (joint->Weights.empty())
			continue;

		// Find this joints pull on vertices
		// Note: It is assumed that the global inversed matrix has been calculated at this point.
		core::matrix4 jointVertexPull = global_matrices[i] * joint->GlobalInversedMatrix.value();

		core::vector3df thisVertexMove, thisNormalMove;

		auto &buffersUsed = *SkinningBuffers;

		// Skin Vertices, Positions and Normals
		for (const auto &weight : joint->Weights) {
			// Pull this vertex...
			jointVertexPull.transformVect(thisVertexMove, weight.StaticPos);

			if (AnimateNormals) {
				thisNormalMove = jointVertexPull.rotateAndScaleVect(weight.StaticNormal);
				thisNormalMove.normalize(); // must renormalize after potentially scaling
			}

			if (!(*(weight.Moved))) {
				*(weight.Moved) = true;

				buffersUsed[weight.buffer_id]->getVertex(weight.vertex_id)->Pos = thisVertexMove * weight.strength;

				if (AnimateNormals)
					buffersUsed[weight.buffer_id]->getVertex(weight.vertex_id)->Normal = thisNormalMove * weight.strength;

				//*(weight._Pos) = thisVertexMove * weight.strength;
			} else {
				buffersUsed[weight.buffer_id]->getVertex(weight.vertex_id)->Pos += thisVertexMove * weight.strength;

				if (AnimateNormals)
					buffersUsed[weight.buffer_id]->getVertex(weight.vertex_id)->Normal += thisNormalMove * weight.strength;

				//*(weight._Pos) += thisVertexMove * weight.strength;
			}
		}
	}

	for (auto *buffer : *SkinningBuffers)
		buffer->setDirty(EBT_VERTEX);
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

void SkinnedMesh::setTextureSlot(u32 meshbufNr, u32 textureSlot) {
	TextureSlots.at(meshbufNr) = textureSlot;
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

void SkinnedMesh::refreshJointCache()
{
	// copy cache from the mesh...
	for (auto *joint : AllJoints) {
		for (auto &weight : joint->Weights) {
			const u16 buffer_id = weight.buffer_id;
			const u32 vertex_id = weight.vertex_id;
			weight.StaticPos = LocalBuffers[buffer_id]->getVertex(vertex_id)->Pos;
			weight.StaticNormal = LocalBuffers[buffer_id]->getVertex(vertex_id)->Normal;
		}
	}
}

void SkinnedMesh::resetAnimation()
{
	// copy from the cache to the mesh...
	for (auto *joint : AllJoints) {
		for (const auto &weight : joint->Weights) {
			const u16 buffer_id = weight.buffer_id;
			const u32 vertex_id = weight.vertex_id;
			LocalBuffers[buffer_id]->getVertex(vertex_id)->Pos = weight.StaticPos;
			LocalBuffers[buffer_id]->getVertex(vertex_id)->Normal = weight.StaticNormal;
		}
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
	for (auto *joint : AllJoints) {
		if (!joint->Weights.empty()) {
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
		for (auto &weight : joint->Weights) {
			const u16 buffer_id = weight.buffer_id;
			const u32 vertex_id = weight.vertex_id;

			// check for invalid ids
			if (buffer_id >= LocalBuffers.size()) {
				os::Printer::log("Skinned Mesh: Weight buffer id too large", ELL_WARNING);
				weight.buffer_id = weight.vertex_id = 0;
			} else if (vertex_id >= LocalBuffers[buffer_id]->getVertexCount()) {
				os::Printer::log("Skinned Mesh: Weight vertex id too large", ELL_WARNING);
				weight.buffer_id = weight.vertex_id = 0;
			}
		}
	}

	for (u32 i = 0; i < Vertices_Moved.size(); ++i)
		for (u32 j = 0; j < Vertices_Moved[i].size(); ++j)
			Vertices_Moved[i][j] = false;

	// For skinning: cache weight values for speed
	for (auto *joint : AllJoints) {
		for (auto &weight : joint->Weights) {
			const u16 buffer_id = weight.buffer_id;
			const u32 vertex_id = weight.vertex_id;

			weight.Moved = &Vertices_Moved[buffer_id][vertex_id];
			weight.StaticPos = LocalBuffers[buffer_id]->getVertex(vertex_id)->Pos;
			weight.StaticNormal = LocalBuffers[buffer_id]->getVertex(vertex_id)->Normal;
		}
	}

	normalizeWeights();

	for (auto *joint : AllJoints) {
		joint->keys.cleanup();
	}
}

void SkinnedMesh::calculateStaticBoundingBox()
{
	std::vector<std::vector<bool>> animated(getMeshBufferCount());
	for (u32 mb = 0; mb < getMeshBufferCount(); mb++)
		animated[mb] = std::vector<bool>(getMeshBuffer(mb)->getVertexCount());

	for (auto *joint : AllJoints) {
		for (auto &weight : joint->Weights) {
			const u16 buffer_id = weight.buffer_id;
			const u32 vertex_id = weight.vertex_id;
			animated[buffer_id][vertex_id] = true;
		}
	}

	bool first = true;
	for (u16 mb = 0; mb < getMeshBufferCount(); mb++) {
		for (u32 v = 0; v < getMeshBuffer(mb)->getVertexCount(); v++) {
			if (!animated[mb][v]) {
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
}

void SkinnedMesh::calculateJointBoundingBoxes()
{
	for (auto *joint : AllJoints) {
		bool first = true;
		for (auto &weight : joint->Weights) {
			if (weight.strength < 1e-6)
				continue;
			auto pos = weight.StaticPos;
			joint->GlobalInversedMatrix.value().transformVect(pos);
			if (!first) {
				joint->LocalBoundingBox.addInternalPoint(pos);
			} else {
				joint->LocalBoundingBox.reset(pos);
				first = false;
			}
		}
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

void SkinnedMesh::topoSortJoints()
{
	size_t n = AllJoints.size();

	std::vector<u16> new_to_old_id;

	std::vector<std::vector<u16>> children(n);
	for (u16 i = 0; i < n; ++i) {
		if (auto parentId = AllJoints[i]->ParentJointID)
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

	std::vector<SJoint *> joints(n);
	for (u16 i = 0; i < n; ++i) {
		joints[i] = AllJoints[new_to_old_id[i]];
		joints[i]->JointID = i;
		if (auto parentId = joints[i]->ParentJointID)
			joints[i]->ParentJointID = old_to_new_id[*parentId];
	}
	AllJoints = std::move(joints);

	for (u16 i = 0; i < n; ++i) {
		if (auto pjid = AllJoints[i]->ParentJointID)
			assert(*pjid < i);
	}
}

//! called by loader after populating with mesh and bone data
SkinnedMesh *SkinnedMeshBuilder::finalize()
{
	os::Printer::log("Skinned Mesh - finalize", ELL_DEBUG);

	topoSortJoints();

	// Set array sizes
	for (u32 i = 0; i < LocalBuffers.size(); ++i) {
		Vertices_Moved.emplace_back(LocalBuffers[i]->getVertexCount());
	}

	prepareForSkinning();

	std::vector<core::matrix4> matrices;
	matrices.reserve(AllJoints.size());
	for (auto *joint : AllJoints) {
		if (const auto *matrix = std::get_if<core::matrix4>(&joint->transform))
			matrices.push_back(*matrix);
		else
		 	matrices.push_back(std::get<core::Transform>(joint->transform).buildMatrix());
	}
	calculateGlobalMatrices(matrices);

	for (size_t i = 0; i < AllJoints.size(); ++i) {
		auto *joint = AllJoints[i];
		if (!joint->GlobalInversedMatrix) {
			joint->GlobalInversedMatrix = matrices[i];
			joint->GlobalInversedMatrix->makeInverse();
		}
		// rigid animation for non animated meshes
		for (u32 attachedMeshIdx : joint->AttachedMeshes) {
			SSkinMeshBuffer *Buffer = (*SkinningBuffers)[attachedMeshIdx];
			Buffer->Transformation = matrices[i];
		}
	}

	recalculateBaseBoundingBoxes();
	StaticPoseBox = calculateBoundingBox(matrices);

	return this;
}

scene::SSkinMeshBuffer *SkinnedMeshBuilder::addMeshBuffer()
{
	scene::SSkinMeshBuffer *buffer = new scene::SSkinMeshBuffer();
	TextureSlots.push_back(LocalBuffers.size());
	LocalBuffers.push_back(buffer);
	return buffer;
}

void SkinnedMeshBuilder::addMeshBuffer(SSkinMeshBuffer *meshbuf)
{
	TextureSlots.push_back(LocalBuffers.size());
	LocalBuffers.push_back(meshbuf);
}

SkinnedMesh::SJoint *SkinnedMeshBuilder::addJoint(SJoint *parent)
{
	SJoint *joint = new SJoint;
	joint->setParent(parent);

	joint->JointID = AllJoints.size();
	AllJoints.push_back(joint);

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

SkinnedMesh::SWeight *SkinnedMeshBuilder::addWeight(SJoint *joint)
{
	if (!joint)
		return nullptr;

	joint->Weights.emplace_back();
	return &joint->Weights.back();
}

void SkinnedMesh::normalizeWeights()
{
	// note: unsure if weights ids are going to be used.

	// Normalise the weights on bones....

	std::vector<std::vector<f32>> verticesTotalWeight;

	verticesTotalWeight.reserve(LocalBuffers.size());
	for (u32 i = 0; i < LocalBuffers.size(); ++i) {
		verticesTotalWeight.emplace_back(LocalBuffers[i]->getVertexCount());
	}

	for (u32 i = 0; i < verticesTotalWeight.size(); ++i)
		for (u32 j = 0; j < verticesTotalWeight[i].size(); ++j)
			verticesTotalWeight[i][j] = 0;

	for (auto *joint : AllJoints) {
		auto &weights = joint->Weights;

		weights.erase(std::remove_if(weights.begin(), weights.end(), [](const auto &weight) {
			return weight.strength <= 0;
		}), weights.end());

		for (const auto &weight : weights) {
			verticesTotalWeight[weight.buffer_id][weight.vertex_id] += weight.strength;
		}
	}

	for (auto *joint : AllJoints) {
		for (auto &weight : joint->Weights) {
			const f32 total = verticesTotalWeight[weight.buffer_id][weight.vertex_id];
			if (total != 0 && total != 1)
				weight.strength /= total;
		}
	}
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
