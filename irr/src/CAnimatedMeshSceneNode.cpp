// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CAnimatedMeshSceneNode.h"
#include "CBoneSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "S3DVertex.h"
#include "Transform.h"
#include "irrTypes.h"
#include "matrix4.h"
#include "os.h"
#include "SkinnedMesh.h"
#include "IDummyTransformationSceneNode.h"
#include "IBoneSceneNode.h"
#include "IMaterialRenderer.h"
#include "IMesh.h"
#include "IMeshCache.h"
#include "IAnimatedMesh.h"
#include "IFileSystem.h"
#include "quaternion.h"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <cassert>

namespace scene
{

//! constructor
CAnimatedMeshSceneNode::CAnimatedMeshSceneNode(IAnimatedMesh *mesh,
		ISceneNode *parent, ISceneManager *mgr, s32 id,
		const core::vector3df &position,
		const core::vector3df &rotation,
		const core::vector3df &scale) :
		IAnimatedMeshSceneNode(parent, mgr, id, position, rotation, scale),
		Mesh(nullptr),
		StartFrame(0), EndFrame(0), FramesPerSecond(0.025f),
		CurrentFrameNr(0.f), LastTimeMs(0),
		TransitionTime(0), Transiting(0.f), TransitingBlend(0.f),
		JointsUsed(false),
		Looping(true), ReadOnlyMaterials(false), RenderFromIdentity(false),
		PassCount(0)
{
	setMesh(mesh);
}

//! destructor
CAnimatedMeshSceneNode::~CAnimatedMeshSceneNode()
{
	if (Mesh)
		Mesh->drop();
}

//! Sets the current frame. From now on the animation is played from this frame.
void CAnimatedMeshSceneNode::setCurrentFrame(f32 frame)
{
	// if you pass an out of range value, we just clamp it
	CurrentFrameNr = core::clamp(frame, (f32)StartFrame, (f32)EndFrame);

	beginTransition(); // transit to this frame if enabled
}

//! Returns the currently displayed frame number.
f32 CAnimatedMeshSceneNode::getFrameNr() const
{
	return CurrentFrameNr;
}

//! Get CurrentFrameNr and update transiting settings
void CAnimatedMeshSceneNode::buildFrameNr(u32 timeMs)
{
	if (Transiting != 0.f) {
		TransitingBlend += (f32)(timeMs)*Transiting;
		if (TransitingBlend > 1.f) {
			Transiting = 0.f;
			TransitingBlend = 0.f;
		}
	}

	if (StartFrame == EndFrame) {
		CurrentFrameNr = StartFrame; // Support for non animated meshes
	} else if (Looping) {
		// play animation looped
		CurrentFrameNr += timeMs * FramesPerSecond;

		// We have no interpolation between EndFrame and StartFrame,
		// the last frame must be identical to first one with our current solution.
		if (FramesPerSecond > 0.f) { // forwards...
			if (CurrentFrameNr > EndFrame)
				CurrentFrameNr = StartFrame + fmodf(CurrentFrameNr - StartFrame, EndFrame - StartFrame);
		} else { // backwards...
			if (CurrentFrameNr < StartFrame)
				CurrentFrameNr = EndFrame - fmodf(EndFrame - CurrentFrameNr, EndFrame - StartFrame);
		}
	} else {
		// play animation non looped

		CurrentFrameNr += timeMs * FramesPerSecond;
		if (FramesPerSecond > 0.f) { // forwards...
			CurrentFrameNr = std::min(CurrentFrameNr, EndFrame);
		} else { // backwards...
			CurrentFrameNr = std::max(CurrentFrameNr, StartFrame);
		}
	}
}

void CAnimatedMeshSceneNode::OnRegisterSceneNode()
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

IMesh *CAnimatedMeshSceneNode::getMeshForCurrentFrame()
{
	if (Mesh->getMeshType() != EAMT_SKINNED) {
		return Mesh;
	}

	// As multiple scene nodes may be sharing the same skinned mesh, we have to
	// re-animate it every frame to ensure that this node gets the mesh that it needs.

	auto *skinnedMesh = static_cast<SkinnedMesh *>(Mesh);

	// Matrices have already been calculated in OnAnimate
	skinnedMesh->skinMesh(PerJoint.GlobalMatrices);

	return skinnedMesh;
}

//! OnAnimate() is called just before rendering the whole scene.
void CAnimatedMeshSceneNode::OnAnimate(u32 timeMs)
{
	if (LastTimeMs == 0) { // first frame
		LastTimeMs = timeMs;
	}

	// set CurrentFrameNr
	const u32 dtimeMs = timeMs - LastTimeMs;
	buildFrameNr(dtimeMs);
	LastTimeMs = timeMs;

	// This needs to be done on animate, which is called recursively *before*
	// anything is rendered so that the transformations of children are up to date
	animateJoints();

	// Copy old transforms *before* bone overrides have been applied.
	// TODO if there are no bone overrides or no animation blending, this is unnecessary.
	copyOldTransforms();

	if (OnAnimateCallback)
		OnAnimateCallback(dtimeMs / 1000.0f);

	IAnimatedMeshSceneNode::OnAnimate(timeMs);

	if (auto *skinnedMesh = dynamic_cast<SkinnedMesh*>(Mesh)) {
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
void CAnimatedMeshSceneNode::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();

	if (!Mesh || !driver)
		return;

	const bool isTransparentPass =
			SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT;

	++PassCount;

	scene::IMesh *m = getMeshForCurrentFrame();
	assert(m);

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);

	for (u32 i = 0; i < m->getMeshBufferCount(); ++i) {
		const bool transparent = driver->needsTransparentRenderPass(Materials[i]);

		// only render transparent buffer if this is the transparent render pass
		// and solid only in solid pass
		if (transparent == isTransparentPass) {
			scene::IMeshBuffer *mb = m->getMeshBuffer(i);
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
			const u32 count = m->getMeshBufferCount();

			// draw normals
			for (u32 g = 0; g < count; ++g) {
				scene::IMeshBuffer *mb = m->getMeshBuffer(g);
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
			for (u32 g = 0; g < m->getMeshBufferCount(); ++g) {
				const IMeshBuffer *mb = m->getMeshBuffer(g);

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
				const auto &joints = (static_cast<SkinnedMesh *>(Mesh))->getAllJoints();
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

			for (u32 g = 0; g < m->getMeshBufferCount(); ++g) {
				const IMeshBuffer *mb = m->getMeshBuffer(g);
				if (RenderFromIdentity)
					driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
				else if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer *)mb)->Transformation);
				driver->drawMeshBuffer(mb);
			}
		}
	}
}

//! Returns the current start frame number.
f32 CAnimatedMeshSceneNode::getStartFrame() const
{
	return StartFrame;
}

//! Returns the current start frame number.
f32 CAnimatedMeshSceneNode::getEndFrame() const
{
	return EndFrame;
}

//! sets the frames between the animation is looped.
//! the default is 0 - MaximalFrameCount of the mesh.
bool CAnimatedMeshSceneNode::setFrameLoop(f32 begin, f32 end)
{
	const f32 maxFrame = Mesh->getMaxFrameNumber();
	if (end < begin) {
		StartFrame = std::clamp<f32>(end, 0, maxFrame);
		EndFrame = std::clamp<f32>(begin, StartFrame, maxFrame);
	} else {
		StartFrame = std::clamp<f32>(begin, 0, maxFrame);
		EndFrame = std::clamp<f32>(end, StartFrame, maxFrame);
	}
	if (FramesPerSecond < 0)
		setCurrentFrame(EndFrame);
	else
		setCurrentFrame(StartFrame);

	return true;
}

//! sets the speed with witch the animation is played
void CAnimatedMeshSceneNode::setAnimationSpeed(f32 framesPerSecond)
{
	FramesPerSecond = framesPerSecond * 0.001f;
}

f32 CAnimatedMeshSceneNode::getAnimationSpeed() const
{
	return FramesPerSecond * 1000.f;
}

//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32> &CAnimatedMeshSceneNode::getBoundingBox() const
{
	return Box;
}

//! returns the material based on the zero based index i.
video::SMaterial &CAnimatedMeshSceneNode::getMaterial(u32 i)
{
	if (i >= Materials.size())
		return ISceneNode::getMaterial(i);

	return Materials[i];
}

//! returns amount of materials used by this scene node.
u32 CAnimatedMeshSceneNode::getMaterialCount() const
{
	return Materials.size();
}

//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
IBoneSceneNode *CAnimatedMeshSceneNode::getJointNode(const c8 *jointName)
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED) {
		os::Printer::log("No mesh, or mesh not of skinned mesh type", ELL_WARNING);
		return 0;
	}

	checkJoints();

	auto *skinnedMesh = (SkinnedMesh *)Mesh;

	const std::optional<u32> number = skinnedMesh->getJointNumber(jointName);

	if (!number.has_value()) {
		os::Printer::log("Joint with specified name not found in skinned mesh", jointName, ELL_DEBUG);
		return 0;
	}

	if (PerJoint.SceneNodes.size() <= *number) {
		os::Printer::log("Joint was found in mesh, but is not loaded into node", jointName, ELL_WARNING);
		return 0;
	}

	return PerJoint.SceneNodes[*number];
}

//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
IBoneSceneNode *CAnimatedMeshSceneNode::getJointNode(u32 jointID)
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

	return PerJoint.SceneNodes[jointID];
}

//! Gets joint count.
u32 CAnimatedMeshSceneNode::getJointCount() const
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return 0;

	auto *skinnedMesh = (SkinnedMesh *)Mesh;

	return skinnedMesh->getJointCount();
}

//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
bool CAnimatedMeshSceneNode::removeChild(ISceneNode *child)
{
	if (ISceneNode::removeChild(child)) {
		if (JointsUsed) { // stop weird bugs caused while changing parents as the joints are being created
			for (u32 i = 0; i < PerJoint.SceneNodes.size(); ++i) {
				if (PerJoint.SceneNodes[i] == child) {
					PerJoint.SceneNodes[i] = 0; // remove link to child
					break;
				}
			}
		}
		return true;
	}

	return false;
}

//! Sets looping mode which is on by default. If set to false,
//! animations will not be looped.
void CAnimatedMeshSceneNode::setLoopMode(bool playAnimationLooped)
{
	Looping = playAnimationLooped;
}

//! returns the current loop mode
bool CAnimatedMeshSceneNode::getLoopMode() const
{
	return Looping;
}

//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
void CAnimatedMeshSceneNode::setReadOnlyMaterials(bool readonly)
{
	ReadOnlyMaterials = readonly;
}

//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool CAnimatedMeshSceneNode::isReadOnlyMaterials() const
{
	return ReadOnlyMaterials;
}

//! Sets a new mesh
void CAnimatedMeshSceneNode::setMesh(IAnimatedMesh *mesh)
{
	if (!mesh)
		return; // won't set null mesh

	if (Mesh != mesh) {
		if (Mesh)
			Mesh->drop();

		Mesh = mesh;

		// grab the mesh (it's non-null!)
		Mesh->grab();
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

	// get start and begin time
	setAnimationSpeed(Mesh->getAnimationSpeed()); // NOTE: This had been commented out (but not removed!) in r3526. Which caused meshloader-values for speed to be ignored unless users specified explicitly. Missing a test-case where this could go wrong so I put the code back in.
	setFrameLoop(0, Mesh->getMaxFrameNumber());
}

//! updates the absolute position based on the relative and the parents position
void CAnimatedMeshSceneNode::updateAbsolutePosition()
{
	IAnimatedMeshSceneNode::updateAbsolutePosition();
}

//! Sets the transition time in seconds (note: This needs to enable joints)
//! you must call animateJoints(), or the mesh will not animate
void CAnimatedMeshSceneNode::setTransitionTime(f32 time)
{
	const u32 ttime = (u32)core::floor32(time * 1000.0f);
	if (TransitionTime == ttime)
		return;
	TransitionTime = ttime;
}

//! render mesh ignoring its transformation. Used with ragdolls. (culling is unaffected)
void CAnimatedMeshSceneNode::setRenderFromIdentity(bool enable)
{
	RenderFromIdentity = enable;
}

void CAnimatedMeshSceneNode::addJoints()
{
	const auto &joints = static_cast<SkinnedMesh*>(Mesh)->getAllJoints();
	PerJoint.setN(joints.size());
	PerJoint.SceneNodes.clear();
	PerJoint.SceneNodes.reserve(joints.size());
	for (size_t i = 0; i < joints.size(); ++i) {
		const auto *joint = joints[i];
		ISceneNode *parent = this;
		if (joint->ParentJointID)
			parent = PerJoint.SceneNodes.at(*joint->ParentJointID); // exists because of topo. order
		assert(parent);
		const auto *matrix = std::get_if<core::matrix4>(&joint->transform);
		PerJoint.SceneNodes.push_back(new CBoneSceneNode(
				parent, SceneManager, 0, i, joint->Name,
				matrix ? core::Transform{} : std::get<core::Transform>(joint->transform),
				matrix ? *matrix : std::optional<core::matrix4>{}));
	}
}

void CAnimatedMeshSceneNode::updateJointSceneNodes(
		const std::vector<SkinnedMesh::SJoint::VariantTransform> &transforms)
{
	for (size_t i = 0; i < transforms.size(); ++i) {
		const auto &transform = transforms[i];
		auto *node = static_cast<CBoneSceneNode*>(PerJoint.SceneNodes[i]);
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
void CAnimatedMeshSceneNode::animateJoints()
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return;

	checkJoints();

	SkinnedMesh *skinnedMesh = static_cast<SkinnedMesh *>(Mesh);
	if (!skinnedMesh->isStatic())
		updateJointSceneNodes(skinnedMesh->animateMesh(getFrameNr()));

	//-----------------------------------------
	//		Transition
	//-----------------------------------------

	if (Transiting != 0.f) {
		for (u32 i = 0; i < PerJoint.SceneNodes.size(); ++i) {
			if (PerJoint.PreTransSaves[i]) {
				PerJoint.SceneNodes[i]->setTransform(PerJoint.PreTransSaves[i]->interpolate(
						PerJoint.SceneNodes[i]->getTransform(), TransitingBlend));
			}
		}
	}
}

void CAnimatedMeshSceneNode::checkJoints()
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return;

	if (!JointsUsed) {
		for (u32 i = 0; i < PerJoint.SceneNodes.size(); ++i)
			removeChild(PerJoint.SceneNodes[i]);
		addJoints();

		JointsUsed = true;
	}
}

void CAnimatedMeshSceneNode::copyOldTransforms()
{
	for (u32 i = 0; i < PerJoint.SceneNodes.size(); ++i) {
		if (!PerJoint.SceneNodes[i]->Matrix) {
			PerJoint.PreTransSaves[i] = PerJoint.SceneNodes[i]->getTransform();
		} else {
			PerJoint.PreTransSaves[i] = std::nullopt;
		}
	}
}

void CAnimatedMeshSceneNode::beginTransition()
{
	if (!JointsUsed)
		return;

	if (TransitionTime != 0) {
		Transiting = core::reciprocal((f32)TransitionTime);
	}
	TransitingBlend = 0.f;
}

ISceneNode *CAnimatedMeshSceneNode::clone(ISceneNode *newParent, ISceneManager *newManager)
{
	if (!newParent)
		newParent = Parent;
	if (!newManager)
		newManager = SceneManager;

	CAnimatedMeshSceneNode *newNode =
			new CAnimatedMeshSceneNode(Mesh, NULL, newManager, ID, RelativeTranslation,
					RelativeRotation, RelativeScale);

	if (newParent) {
		newNode->setParent(newParent); // not in constructor because virtual overload for updateAbsolutePosition won't be called
		newNode->drop();
	}

	newNode->cloneMembers(this, newManager);

	newNode->Materials = Materials;
	newNode->Box = Box;
	newNode->Mesh = Mesh;
	newNode->StartFrame = StartFrame;
	newNode->EndFrame = EndFrame;
	newNode->FramesPerSecond = FramesPerSecond;
	newNode->CurrentFrameNr = CurrentFrameNr;
	newNode->JointsUsed = JointsUsed;
	newNode->TransitionTime = TransitionTime;
	newNode->Transiting = Transiting;
	newNode->TransitingBlend = TransitingBlend;
	newNode->Looping = Looping;
	newNode->ReadOnlyMaterials = ReadOnlyMaterials;
	newNode->PassCount = PassCount;
	newNode->PerJoint.SceneNodes = PerJoint.SceneNodes;
	newNode->PerJoint.PreTransSaves = PerJoint.PreTransSaves;
	newNode->RenderFromIdentity = RenderFromIdentity;

	return newNode;
}

} // end namespace scene
