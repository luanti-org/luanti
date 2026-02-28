// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "AnimatedMeshSceneNode.h"
#include "BoneSceneNode.h"
#include "ISceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "S3DVertex.h"
#include "Transform.h"
#include "irrTypes.h"
#include "matrix4.h"
#include "os.h"
#include "SkinnedMesh.h"
#include "IDummyTransformationSceneNode.h"
#include "BoneSceneNode.h"
#include "IMaterialRenderer.h"
#include "IMesh.h"
#include "IMeshCache.h"
#include "IAnimatedMesh.h"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <cassert>

namespace scene
{

//! constructor
AnimatedMeshSceneNode::AnimatedMeshSceneNode(IAnimatedMesh *mesh,
		ISceneNode *parent, ISceneManager *mgr, s32 id,
		const core::vector3df &position,
		const core::vector3df &rotation,
		const core::vector3df &scale) :
		ISceneNode(parent, mgr, id, position, rotation, scale),
		Mesh(nullptr),
		LastTimeMs(0),
		JointsUsed(false),
		ReadOnlyMaterials(false), RenderFromIdentity(false),
		PassCount(0)
{
	setMesh(mesh);
}

void AnimatedMeshSceneNode::advanceAnimations(f32 dtime_s)
{
	anim_spec.advance(dtime_s);
}

void AnimatedMeshSceneNode::OnRegisterSceneNode()
{
	if (IsVisible && Mesh) {
		// because this node supports rendering of mixed mode meshes consisting of
		// transparent and solid material at the same time, we need to go through all
		// materials, check of what type they are and register this node for the right
		// render pass according to that.

		video::IVideoDriver *driver = SceneManager->getVideoDriver();

		PassCount = 0;
		int transparentCount = 0;
		int solidCount = 0;

		// count transparent and solid materials in this scene node
		const u32 numMaterials = ReadOnlyMaterials ? Mesh->getMeshBufferCount() : Materials.size();
		for (u32 i = 0; i < numMaterials; ++i) {
			const video::SMaterial &material = ReadOnlyMaterials ? Mesh->getMeshBuffer(i)->getMaterial() : Materials[i];

			if (driver->needsTransparentRenderPass(material))
				++transparentCount;
			else
				++solidCount;

			if (solidCount && transparentCount)
				break;
		}

		// register according to material types counted

		if (solidCount)
			SceneManager->registerNodeForRendering(this, scene::ESNRP_SOLID);

		if (transparentCount)
			SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);

		ISceneNode::OnRegisterSceneNode();
	}
}

//! OnAnimate() is called just before rendering the whole scene.
void AnimatedMeshSceneNode::OnAnimate(u32 time_ms)
{
	if (LastTimeMs == 0) { // first frame
		LastTimeMs = time_ms;
	}

	// set CurrentFrameNr
	const u32 dtime_ms = time_ms - LastTimeMs;
	const f32 dtime_s = dtime_ms / 1000.0f;
	advanceAnimations(dtime_s);
	LastTimeMs = time_ms;

	// This needs to be done on animate, which is called recursively *before*
	// anything is rendered so that the transformations of children are up to date
	animateJoints();

	if (OnAnimateCallback)
		OnAnimateCallback(dtime_s);

	ISceneNode::OnAnimate(time_ms);

	if (auto *skinnedMesh = dynamic_cast<SkinnedMesh*>(Mesh.get())) {
		for (u16 i = 0; i < PerJoint.SceneNodes.size(); ++i)
			PerJoint.GlobalMatrices[i] = PerJoint.SceneNodes[i]->getRelativeTransformation();
		assert(PerJoint.GlobalMatrices.size() == skinnedMesh->getJointCount());
		skinnedMesh->calculateGlobalMatrices(PerJoint.GlobalMatrices);
		Box = skinnedMesh->calculateBoundingBox(PerJoint.GlobalMatrices);
	} else {
		Box = Mesh->getBoundingBox();
	}
}

//! renders the node.
void AnimatedMeshSceneNode::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();

	if (!Mesh || !driver)
		return;

	const bool isTransparentPass =
			SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT;

	++PassCount;

	if (auto *sm = dynamic_cast<SkinnedMesh *>(Mesh.get())) {
		sm->rigidAnimation(PerJoint.GlobalMatrices);
		if (sm->useSoftwareSkinning()) {
			// Perform software skinning; matrices have already been calculated in OnAnimate
			sm->skinMesh(PerJoint.GlobalMatrices);
			++driver->getFrameStats().SWSkinnedMeshes;
		} else if (sm->hasWeights()) {
			driver->setJointTransforms(sm->calculateSkinMatrices(PerJoint.GlobalMatrices));
			++driver->getFrameStats().HWSkinnedMeshes;
		}
	}
	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);

	for (u32 i = 0; i < Mesh->getMeshBufferCount(); ++i) {
		const bool transparent = driver->needsTransparentRenderPass(Materials[i]);

		// only render transparent buffer if this is the transparent render pass
		// and solid only in solid pass
		if (transparent == isTransparentPass) {
			scene::IMeshBuffer *mb = Mesh->getMeshBuffer(i);
			const video::SMaterial &material = ReadOnlyMaterials ? mb->getMaterial() : Materials[i];
			if (RenderFromIdentity)
				driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
			else if (Mesh->getMeshType() == EAMT_SKINNED)
				driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer *)mb)->Transformation);

			driver->setMaterial(material);
			driver->drawMeshBuffer(mb);
		}
	}

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);

	// for debug purposes only:
	if (DebugDataVisible && PassCount == 1) {
		video::SMaterial debug_mat;
		debug_mat.AntiAliasing = video::EAAM_OFF;
		driver->setMaterial(debug_mat);
		// show normals
		if (DebugDataVisible & scene::EDS_NORMALS) {
			const f32 debugNormalLength = 1.f;
			const video::SColor debugNormalColor = video::SColor(255, 34, 221, 221);
			const u32 count = Mesh->getMeshBufferCount();

			// draw normals
			for (u32 g = 0; g < count; ++g) {
				scene::IMeshBuffer *mb = Mesh->getMeshBuffer(g);
				if (RenderFromIdentity)
					driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
				else if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer *)mb)->Transformation);

				driver->drawMeshBufferNormals(mb, debugNormalLength, debugNormalColor);
			}
		}

		debug_mat.ZBuffer = video::ECFN_DISABLED;
		driver->setMaterial(debug_mat);

		// show bounding box
		if (DebugDataVisible & scene::EDS_BBOX_BUFFERS) {
			for (u32 g = 0; g < Mesh->getMeshBufferCount(); ++g) {
				const IMeshBuffer *mb = Mesh->getMeshBuffer(g);

				if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer *)mb)->Transformation);
				driver->draw3DBox(mb->getBoundingBox(), video::SColor(255, 190, 128, 128));
			}
		}

		if (DebugDataVisible & scene::EDS_BBOX)
			driver->draw3DBox(Box, video::SColor(255, 255, 255, 255));

		// show skeleton
		if (DebugDataVisible & scene::EDS_SKELETON) {
			if (Mesh->getMeshType() == EAMT_SKINNED) {
				// draw skeleton
				const auto &joints = (static_cast<SkinnedMesh *>(Mesh.get()))->getAllJoints();
				for (u16 i = 0; i < PerJoint.GlobalMatrices.size(); ++i) {
					const auto translation = PerJoint.GlobalMatrices[i].getTranslation();
					if (auto pjid = joints[i]->ParentJointID) {
						const auto parent_translation = PerJoint.GlobalMatrices[*pjid].getTranslation();
						driver->draw3DLine(parent_translation, translation,
								video::SColor(255, 51, 66, 255));
					}
				}
			}
		}

		// show mesh
		if (DebugDataVisible & scene::EDS_MESH_WIRE_OVERLAY) {
			debug_mat.Wireframe = true;
			debug_mat.ZBuffer = video::ECFN_DISABLED;
			driver->setMaterial(debug_mat);

			for (u32 g = 0; g < Mesh->getMeshBufferCount(); ++g) {
				const IMeshBuffer *mb = Mesh->getMeshBuffer(g);
				if (RenderFromIdentity)
					driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
				else if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer *)mb)->Transformation);
				driver->drawMeshBuffer(mb);
			}
		}
	}
}

//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32> &AnimatedMeshSceneNode::getBoundingBox() const
{
	return Box;
}

//! returns the material based on the zero based index i.
video::SMaterial &AnimatedMeshSceneNode::getMaterial(u32 i)
{
	if (i >= Materials.size())
		return ISceneNode::getMaterial(i);

	return Materials[i];
}

//! returns amount of materials used by this scene node.
u32 AnimatedMeshSceneNode::getMaterialCount() const
{
	return Materials.size();
}

//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
BoneSceneNode *AnimatedMeshSceneNode::getJointNode(const c8 *jointName)
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED) {
		os::Printer::log("No mesh, or mesh not of skinned mesh type", ELL_WARNING);
		return 0;
	}

	checkJoints();

	auto *skinnedMesh = (SkinnedMesh *) Mesh.get();

	const std::optional<u32> number = skinnedMesh->getJointNumber(jointName);

	if (!number.has_value()) {
		os::Printer::log("Joint with specified name not found in skinned mesh", jointName, ELL_DEBUG);
		return 0;
	}

	if (PerJoint.SceneNodes.size() <= *number) {
		os::Printer::log("Joint was found in mesh, but is not loaded into node", jointName, ELL_WARNING);
		return 0;
	}

	return PerJoint.SceneNodes[*number].get();
}

//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
BoneSceneNode *AnimatedMeshSceneNode::getJointNode(u32 jointID)
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED) {
		os::Printer::log("No mesh, or mesh not of skinned mesh type", ELL_WARNING);
		return 0;
	}

	checkJoints();

	if (PerJoint.SceneNodes.size() <= jointID) {
		os::Printer::log("Joint not loaded into node", ELL_WARNING);
		return 0;
	}

	return PerJoint.SceneNodes[jointID].get();
}

//! Gets joint count.
u32 AnimatedMeshSceneNode::getJointCount() const
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return 0;

	auto *skinnedMesh = (SkinnedMesh *) Mesh.get();

	return skinnedMesh->getJointCount();
}

//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
bool AnimatedMeshSceneNode::removeChild(ISceneNode *child)
{
	if (ISceneNode::removeChild(child)) {
		if (JointsUsed) { // stop weird bugs caused while changing parents as the joints are being created
			for (u32 i = 0; i < PerJoint.SceneNodes.size(); ++i) {
				if (PerJoint.SceneNodes[i].get() == child) {
					PerJoint.SceneNodes[i].reset(); // remove link to child
					break;
				}
			}
		}
		return true;
	}

	return false;
}

//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
void AnimatedMeshSceneNode::setReadOnlyMaterials(bool readonly)
{
	ReadOnlyMaterials = readonly;
}

//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool AnimatedMeshSceneNode::isReadOnlyMaterials() const
{
	return ReadOnlyMaterials;
}

void AnimatedMeshSceneNode::setMesh(IAnimatedMesh *mesh)
{
	if (!mesh)
		return; // won't set null mesh

	if (Mesh != mesh) {
		Mesh.grab(mesh);
	}

	// get materials and bounding box
	Box = Mesh->getBoundingBox();

	Materials.clear();
	Materials.reallocate(Mesh->getMeshBufferCount());

	for (u32 i = 0; i < Mesh->getMeshBufferCount(); ++i) {
		IMeshBuffer *mb = Mesh->getMeshBuffer(i);
		if (mb)
			Materials.push_back(mb->getMaterial());
		else
			Materials.push_back(video::SMaterial());
	}

	// clean up joint nodes
	if (JointsUsed) {
		JointsUsed = false;
		checkJoints();
	}

	anim_spec.tracks.clear();
}

//! updates the absolute position based on the relative and the parents position
void AnimatedMeshSceneNode::updateAbsolutePosition()
{
	ISceneNode::updateAbsolutePosition();
}

//! render mesh ignoring its transformation. Used with ragdolls. (culling is unaffected)
void AnimatedMeshSceneNode::setRenderFromIdentity(bool enable)
{
	RenderFromIdentity = enable;
}

void AnimatedMeshSceneNode::addJoints()
{
	const auto &joints = static_cast<SkinnedMesh*>(Mesh.get())->getAllJoints();
	PerJoint.setN(joints.size());
	PerJoint.SceneNodes.clear();
	PerJoint.SceneNodes.reserve(joints.size());
	for (size_t i = 0; i < joints.size(); ++i) {
		const auto *joint = joints[i];
		ISceneNode *parent = this;
		if (joint->ParentJointID)
			parent = PerJoint.SceneNodes.at(*joint->ParentJointID).get(); // exists because of topo. order
		assert(parent);
		const auto *matrix = std::get_if<core::matrix4>(&joint->transform);
		PerJoint.SceneNodes.push_back(irr_ptr<BoneSceneNode>(new BoneSceneNode(
				parent, SceneManager, 0, i, joint->Name,
				matrix ? core::Transform{} : std::get<core::Transform>(joint->transform),
				matrix ? *matrix : std::optional<core::matrix4>{})));
	}
}

void AnimatedMeshSceneNode::updateJointSceneNodes(
		const std::vector<SkinnedMesh::SJoint::VariantTransform> &transforms)
{
	for (size_t i = 0; i < transforms.size(); ++i) {
		const auto &transform = transforms[i];
		auto *node = static_cast<BoneSceneNode*>(PerJoint.SceneNodes[i]);
		if (const auto *trs = std::get_if<core::Transform>(&transform)) {
			node->setTransform(*trs);
			// .x lets animations override matrix transforms entirely.
			node->Matrix = std::nullopt;
		} else {
			node->Matrix = std::get<core::matrix4>(transform);
		}
	}
}

//! updates the joint positions of this mesh
void AnimatedMeshSceneNode::animateJoints()
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return;

	checkJoints();

	const u16 n_tracks = anim_spec.tracks.size();
	std::vector<u16> anim_idxs(n_tracks);
	struct Progress {
		SkinnedMesh::AnimationProgress progress;
		s32 priority;
	};
	std::vector<Progress> progresses;
	for (const auto [track, anim] : anim_spec.tracks) {
		SkinnedMesh::AnimationProgress progress = {
			track,
			anim.cur_frame,
			anim.blend > 0.0f ? (anim.blend_progress / anim.blend) : 1.0f,
		};
		progresses.push_back({progress, anim.priority});
	}
	std::sort(progresses.begin(), progresses.end(),
			[](const Progress &a, const Progress &b) {
				return a.priority > b.priority;
			});
	std::vector<SkinnedMesh::AnimationProgress> final_progresses;
	for (const auto &p : progresses)
		final_progresses.push_back(p.progress);

	SkinnedMesh *skinned_mesh = static_cast<SkinnedMesh *>(Mesh.get());
	if (!skinned_mesh->isStatic()) {
		auto transforms = skinned_mesh->animateMesh(
				final_progresses, PerJoint.PreTransSaves);
		updateJointSceneNodes(transforms);
	}

	// Copy old transforms (for potential blending) *before* bone overrides have been applied.
	// TODO if there are no bone overrides or no animation blending, this is unnecessary.
	copyOldTransforms();
}

void AnimatedMeshSceneNode::checkJoints()
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return;

	if (!JointsUsed) {
		for (u32 i = 0; i < PerJoint.SceneNodes.size(); ++i)
			removeChild(PerJoint.SceneNodes[i].get());
		addJoints();

		JointsUsed = true;
	}
}

void AnimatedMeshSceneNode::copyOldTransforms()
{
	for (u32 i = 0; i < PerJoint.SceneNodes.size(); ++i) {
		if (!PerJoint.SceneNodes[i]->Matrix) {
			PerJoint.PreTransSaves[i] = PerJoint.SceneNodes[i]->getTransform();
		} else {
			PerJoint.PreTransSaves[i] = std::nullopt;
		}
	}
}

ISceneNode *AnimatedMeshSceneNode::clone(ISceneNode *newParent, ISceneManager *newManager)
{
	if (!newParent)
		newParent = Parent;
	if (!newManager)
		newManager = SceneManager;

	AnimatedMeshSceneNode *newNode =
			new AnimatedMeshSceneNode(Mesh.get(), NULL, newManager, ID, RelativeTranslation,
					RelativeRotation, RelativeScale);

	if (newParent) {
		newNode->setParent(newParent); // not in constructor because virtual overload for updateAbsolutePosition won't be called
		newNode->drop();
	}

	newNode->cloneMembers(this, newManager);

	newNode->Materials = Materials;
	newNode->Box = Box;
	newNode->Mesh = Mesh;
	newNode->JointsUsed = JointsUsed;
	newNode->ReadOnlyMaterials = ReadOnlyMaterials;
	newNode->PassCount = PassCount;
	newNode->PerJoint.SceneNodes = PerJoint.SceneNodes;
	newNode->PerJoint.PreTransSaves = PerJoint.PreTransSaves;
	newNode->RenderFromIdentity = RenderFromIdentity;

	return newNode;
}

} // end namespace scene
