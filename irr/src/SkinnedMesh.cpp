// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "SkinnedMesh.h"
#include "IBoneSceneNode.h"
#include "CBoneSceneNode.h"
#include "IAnimatedMeshSceneNode.h"
#include "SSkinMeshBuffer.h"
#include "os.h"
#include <vector>

namespace irr
{
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

//! returns the animated mesh based
IMesh *SkinnedMesh::getMesh(f32 frame)
{
	// animate(frame,startFrameLoop, endFrameLoop);
	if (frame == -1)
		return this;

	animateMesh(frame);
	skinMesh();
	return this;
}

//--------------------------------------------------------------------------
//			Keyframe Animation
//--------------------------------------------------------------------------

//! Animates joints based on frame input
void SkinnedMesh::animateMesh(f32 frame)
{
	if (!HasAnimation || LastAnimatedFrame == frame)
		return;

	LastAnimatedFrame = frame;
	SkinnedLastFrame = false;

	for (auto *joint : AllJoints) {
		// The joints can be animated here with no input from their
		// parents, but for setAnimationMode extra checks are needed
		// to their parents
		joint->keys.updateTransform(frame,
				joint->Animatedposition,
				joint->Animatedrotation,
				joint->Animatedscale);
	}

	// Note:
	// LocalAnimatedMatrix needs to be built at some point, but this function may be called lots of times for
	// one render (to play two animations at the same time) LocalAnimatedMatrix only needs to be built once.
	// a call to buildAllLocalAnimatedMatrices is needed before skinning the mesh, and before the user gets the joints to move

	//----------------
	// Temp!
	buildAllLocalAnimatedMatrices();
	//-----------------

	updateBoundingBox();
}

void SkinnedMesh::buildAllLocalAnimatedMatrices()
{
	for (auto *joint : AllJoints) {
		// Could be faster:

		if (!joint->keys.empty()) {
			joint->GlobalSkinningSpace = false;

			// IRR_TEST_BROKEN_QUATERNION_USE: TODO - switched to getMatrix_transposed instead of getMatrix for downward compatibility.
			//								   Not tested so far if this was correct or wrong before quaternion fix!
			// Note that using getMatrix_transposed inverts the rotation.
			joint->Animatedrotation.getMatrix_transposed(joint->LocalAnimatedMatrix);

			// --- joint->LocalAnimatedMatrix *= joint->Animatedrotation.getMatrix() ---
			f32 *m1 = joint->LocalAnimatedMatrix.pointer();
			core::vector3df &Pos = joint->Animatedposition;
			m1[0] += Pos.X * m1[3];
			m1[1] += Pos.Y * m1[3];
			m1[2] += Pos.Z * m1[3];
			m1[4] += Pos.X * m1[7];
			m1[5] += Pos.Y * m1[7];
			m1[6] += Pos.Z * m1[7];
			m1[8] += Pos.X * m1[11];
			m1[9] += Pos.Y * m1[11];
			m1[10] += Pos.Z * m1[11];
			m1[12] += Pos.X * m1[15];
			m1[13] += Pos.Y * m1[15];
			m1[14] += Pos.Z * m1[15];
			// -----------------------------------

			if (!joint->keys.scale.empty()) {
				/*
				core::matrix4 scaleMatrix;
				scaleMatrix.setScale(joint->Animatedscale);
				joint->LocalAnimatedMatrix *= scaleMatrix;
				*/

				// -------- joint->LocalAnimatedMatrix *= scaleMatrix -----------------
				core::matrix4 &mat = joint->LocalAnimatedMatrix;
				mat[0] *= joint->Animatedscale.X;
				mat[1] *= joint->Animatedscale.X;
				mat[2] *= joint->Animatedscale.X;
				mat[3] *= joint->Animatedscale.X;
				mat[4] *= joint->Animatedscale.Y;
				mat[5] *= joint->Animatedscale.Y;
				mat[6] *= joint->Animatedscale.Y;
				mat[7] *= joint->Animatedscale.Y;
				mat[8] *= joint->Animatedscale.Z;
				mat[9] *= joint->Animatedscale.Z;
				mat[10] *= joint->Animatedscale.Z;
				mat[11] *= joint->Animatedscale.Z;
				// -----------------------------------
			}
		} else {
			joint->LocalAnimatedMatrix = joint->LocalMatrix;
		}
	}
	SkinnedLastFrame = false;
}

void SkinnedMesh::buildAllGlobalAnimatedMatrices(SJoint *joint, SJoint *parentJoint)
{
	if (!joint) {
		for (auto *rootJoint : RootJoints)
			buildAllGlobalAnimatedMatrices(rootJoint, 0);
		return;
	} else {
		// Find global matrix...
		if (!parentJoint || joint->GlobalSkinningSpace)
			joint->GlobalAnimatedMatrix = joint->LocalAnimatedMatrix;
		else
			joint->GlobalAnimatedMatrix = parentJoint->GlobalAnimatedMatrix * joint->LocalAnimatedMatrix;
	}

	for (auto *childJoint : joint->Children)
		buildAllGlobalAnimatedMatrices(childJoint, joint);
}

//--------------------------------------------------------------------------
//				Software Skinning
//--------------------------------------------------------------------------

//! Preforms a software skin on this mesh based of joint positions
void SkinnedMesh::skinMesh()
{
	if (!HasAnimation || SkinnedLastFrame)
		return;

	//----------------
	// This is marked as "Temp!".  A shiny dubloon to whomever can tell me why.
	buildAllGlobalAnimatedMatrices();
	//-----------------

	SkinnedLastFrame = true;
	if (!HardwareSkinning) {
		// rigid animation
		for (auto *joint : AllJoints) {
			for (u32 attachedMeshIdx : joint->AttachedMeshes) {
				SSkinMeshBuffer *Buffer = (*SkinningBuffers)[attachedMeshIdx];
				Buffer->Transformation = joint->GlobalAnimatedMatrix;
			}
		}

		// clear skinning helper array
		for (std::vector<char> &buf : Vertices_Moved)
			std::fill(buf.begin(), buf.end(), false);

		// skin starting with the root joints
		for (auto *rootJoint : RootJoints)
			skinJoint(rootJoint, 0);

		for (auto *buffer : *SkinningBuffers)
			buffer->setDirty(EBT_VERTEX);
	}
	updateBoundingBox();
}

void SkinnedMesh::skinJoint(SJoint *joint, SJoint *parentJoint)
{
	if (joint->Weights.size()) {
		// Find this joints pull on vertices...
		// Note: It is assumed that the global inversed matrix has been calculated at this point.
		core::matrix4 jointVertexPull = joint->GlobalAnimatedMatrix * joint->GlobalInversedMatrix.value();

		core::vector3df thisVertexMove, thisNormalMove;

		auto &buffersUsed = *SkinningBuffers;

		// Skin Vertices Positions and Normals...
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

			buffersUsed[weight.buffer_id]->boundingBoxNeedsRecalculated();
		}
	}

	// Skin all children
	for (auto *childJoint : joint->Children)
		skinJoint(childJoint, joint);
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
	return 0;
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

//! (This feature is not implemented in irrlicht yet)
bool SkinnedMesh::setHardwareSkinning(bool on)
{
	if (HardwareSkinning != on) {
		if (on) {

			// set mesh to static pose...
			for (auto *joint : AllJoints) {
				for (const auto &weight : joint->Weights) {
					const u16 buffer_id = weight.buffer_id;
					const u32 vertex_id = weight.vertex_id;
					LocalBuffers[buffer_id]->getVertex(vertex_id)->Pos = weight.StaticPos;
					LocalBuffers[buffer_id]->getVertex(vertex_id)->Normal = weight.StaticNormal;
					LocalBuffers[buffer_id]->boundingBoxNeedsRecalculated();
				}
			}
		}

		HardwareSkinning = on;
	}
	return HardwareSkinning;
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
	SkinnedLastFrame = false;
	LastAnimatedFrame = -1;
}

void SkinnedMesh::calculateGlobalMatrices(SJoint *joint, SJoint *parentJoint)
{
	if (!joint && parentJoint) // bit of protection from endless loops
		return;

	// Go through the root bones
	if (!joint) {
		for (auto *rootJoint : RootJoints)
			calculateGlobalMatrices(rootJoint, nullptr);
		return;
	}

	if (!parentJoint)
		joint->GlobalMatrix = joint->LocalMatrix;
	else
		joint->GlobalMatrix = parentJoint->GlobalMatrix * joint->LocalMatrix;

	joint->LocalAnimatedMatrix = joint->LocalMatrix;
	joint->GlobalAnimatedMatrix = joint->GlobalMatrix;

	if (!joint->GlobalInversedMatrix.has_value()) { // might be pre calculated
		joint->GlobalInversedMatrix = joint->GlobalMatrix;
		joint->GlobalInversedMatrix->makeInverse(); // slow
	}

	for (auto *childJoint : joint->Children)
		calculateGlobalMatrices(childJoint, joint);
	SkinnedLastFrame = false;
}

void SkinnedMesh::checkForAnimation()
{
	// Check for animation...
	HasAnimation = false;
	for (auto *joint : AllJoints) {
		if (!joint->keys.empty()) {
			HasAnimation = true;
			break;
		}
	}

	// meshes with weights, are still counted as animated for ragdolls, etc
	if (!HasAnimation) {
		for (auto *joint : AllJoints) {
			if (joint->Weights.size()) {
				HasAnimation = true;
				break;
			}
		}
	}

	if (HasAnimation) {
		EndFrame = 0.0f;
		for (const auto *joint : AllJoints) {
			EndFrame = std::max(EndFrame, joint->keys.getEndFrame());
		}
	}

	if (HasAnimation && !PreparedForSkinning) {
		PreparedForSkinning = true;

		// check for bugs:
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

		// An array used in skinning

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

				// weight._Pos=&Buffers[buffer_id]->getVertex(vertex_id)->Pos;
			}
		}

		// normalize weights
		normalizeWeights();
	}
	SkinnedLastFrame = false;
}

//! called by loader after populating with mesh and bone data
SkinnedMesh *SkinnedMeshBuilder::finalize()
{
	os::Printer::log("Skinned Mesh - finalize", ELL_DEBUG);

	// Make sure we recalc the next frame
	LastAnimatedFrame = -1;
	SkinnedLastFrame = false;

	// calculate bounding box
	for (auto *buffer : LocalBuffers) {
		buffer->recalculateBoundingBox();
	}

	if (AllJoints.size() || RootJoints.size()) {
		// populate AllJoints or RootJoints, depending on which is empty
		if (RootJoints.empty()) {

			for (auto *joint : AllJoints) {

				bool foundParent = false;
				for (const auto *parentJoint : AllJoints) {
					for (const auto *childJoint : parentJoint->Children) {
						if (childJoint == joint)
							foundParent = true;
					}
				}

				if (!foundParent)
					RootJoints.push_back(joint);
			}
		} else {
			AllJoints = RootJoints;
		}
	}

	// Set array sizes...

	for (u32 i = 0; i < LocalBuffers.size(); ++i) {
		Vertices_Moved.emplace_back(LocalBuffers[i]->getVertexCount());
	}

	checkForAnimation();

	if (HasAnimation) {
		for (auto *joint : AllJoints) {
			joint->keys.cleanup();
		}
	}

	// Needed for animation and skinning...

	calculateGlobalMatrices(0, 0);

	// rigid animation for non animated meshes
	for (auto *joint : AllJoints) {
		for (u32 attachedMeshIdx : joint->AttachedMeshes) {
			SSkinMeshBuffer *Buffer = (*SkinningBuffers)[attachedMeshIdx];
			Buffer->Transformation = joint->GlobalAnimatedMatrix;
		}
	}

	// calculate bounding box
	if (LocalBuffers.empty())
		BoundingBox.reset(0, 0, 0);
	else {
		irr::core::aabbox3df bb(LocalBuffers[0]->BoundingBox);
		LocalBuffers[0]->Transformation.transformBoxEx(bb);
		BoundingBox.reset(bb);

		for (u32 j = 1; j < LocalBuffers.size(); ++j) {
			bb = LocalBuffers[j]->BoundingBox;
			LocalBuffers[j]->Transformation.transformBoxEx(bb);

			BoundingBox.addInternalBox(bb);
		}
	}

	return this;
}

void SkinnedMesh::updateBoundingBox()
{
	if (!SkinningBuffers)
		return;

	BoundingBox.reset(0, 0, 0);

	for (auto *buffer : *SkinningBuffers) {
		buffer->recalculateBoundingBox();
		core::aabbox3df bb = buffer->BoundingBox;
		buffer->Transformation.transformBoxEx(bb);

		BoundingBox.addInternalBox(bb);
	}
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

	AllJoints.push_back(joint);
	if (!parent) {
		// Add root joints to array in finalize()
	} else {
		// Set parent (Be careful of the mesh loader also setting the parent)
		parent->Children.push_back(joint);
	}

	return joint;
}

void SkinnedMeshBuilder::addPositionKey(SJoint *joint, f32 frame, core::vector3df pos)
{
	_IRR_DEBUG_BREAK_IF(!joint);
	joint->keys.position.pushBack(frame, pos);
}

void SkinnedMeshBuilder::addScaleKey(SJoint *joint, f32 frame, core::vector3df scale)
{
	_IRR_DEBUG_BREAK_IF(!joint);
	joint->keys.scale.pushBack(frame, scale);
}

void SkinnedMeshBuilder::addRotationKey(SJoint *joint, f32 frame, core::quaternion rot)
{
	_IRR_DEBUG_BREAK_IF(!joint);
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

void SkinnedMesh::recoverJointsFromMesh(std::vector<IBoneSceneNode *> &jointChildSceneNodes)
{
	for (u32 i = 0; i < AllJoints.size(); ++i) {
		IBoneSceneNode *node = jointChildSceneNodes[i];
		SJoint *joint = AllJoints[i];
		node->setPosition(joint->LocalAnimatedMatrix.getTranslation());
		node->setRotation(joint->LocalAnimatedMatrix.getRotationDegrees());
		node->setScale(joint->LocalAnimatedMatrix.getScale());

		node->updateAbsolutePosition();
	}
}

void SkinnedMesh::transferJointsToMesh(const std::vector<IBoneSceneNode *> &jointChildSceneNodes)
{
	for (u32 i = 0; i < AllJoints.size(); ++i) {
		const IBoneSceneNode *const node = jointChildSceneNodes[i];
		SJoint *joint = AllJoints[i];

		joint->LocalAnimatedMatrix.setRotationDegrees(node->getRotation());
		joint->LocalAnimatedMatrix.setTranslation(node->getPosition());
		joint->LocalAnimatedMatrix *= core::matrix4().setScale(node->getScale());

		joint->GlobalSkinningSpace = (node->getSkinningSpace() == EBSS_GLOBAL);
	}
	// Make sure we recalc the next frame
	LastAnimatedFrame = -1;
	SkinnedLastFrame = false;
}

void SkinnedMesh::addJoints(std::vector<IBoneSceneNode *> &jointChildSceneNodes,
		IAnimatedMeshSceneNode *node, ISceneManager *smgr)
{
	// Create new joints
	for (u32 i = 0; i < AllJoints.size(); ++i) {
		jointChildSceneNodes.push_back(new CBoneSceneNode(0, smgr, 0, i, AllJoints[i]->Name));
	}

	// Match up parents
	for (u32 i = 0; i < jointChildSceneNodes.size(); ++i) {
		const SJoint *const joint = AllJoints[i]; // should be fine

		s32 parentID = -1;

		for (u32 j = 0; (parentID == -1) && (j < AllJoints.size()); ++j) {
			if (i != j) {
				const SJoint *const parentTest = AllJoints[j];
				for (u32 n = 0; n < parentTest->Children.size(); ++n) {
					if (parentTest->Children[n] == joint) {
						parentID = j;
						break;
					}
				}
			}
		}

		IBoneSceneNode *bone = jointChildSceneNodes[i];
		if (parentID != -1)
			bone->setParent(jointChildSceneNodes[parentID]);
		else
			bone->setParent(node);

		bone->drop();
	}
	SkinnedLastFrame = false;
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
} // end namespace irr
