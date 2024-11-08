// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CAnimatedMeshSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "S3DVertex.h"
#include "os.h"
#include "CSkinnedMesh.h"
#include "IDummyTransformationSceneNode.h"
#include "IBoneSceneNode.h"
#include "IMaterialRenderer.h"
#include "IMesh.h"
#include "IMeshCache.h"
#include "IAnimatedMesh.h"
#include "IFileSystem.h"
#include "quaternion.h"
#include <algorithm>

namespace irr
{
namespace scene
{

//! constructor
CAnimatedMeshSceneNode::CAnimatedMeshSceneNode(IAnimatedMesh *mesh,
		ISceneNode *parent, ISceneManager *mgr, s32 id,
		const core::vector3df &position,
		const core::vector3df &rotation,
		const core::vector3df &scale) :
		IAnimatedMeshSceneNode(parent, mgr, id, position, rotation, scale),
		Mesh(0),
		StartFrame(0), EndFrame(0), FramesPerSecond(0.025f),
		CurrentFrameNr(0.f), LastTimeMs(0),
		TransitionTime(0), Transiting(0.f), TransitingBlend(0.f),
		JointMode(EJUOR_NONE), JointsUsed(false),
		Looping(true), ReadOnlyMaterials(false), RenderFromIdentity(false),
		LoopCallBack(0), PassCount(0)
{
#ifdef _DEBUG
	setDebugName("CAnimatedMeshSceneNode");
#endif

	setMesh(mesh);
}

//! destructor
CAnimatedMeshSceneNode::~CAnimatedMeshSceneNode()
{
	if (LoopCallBack)
		LoopCallBack->drop();
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
		} else // backwards...
		{
			if (CurrentFrameNr < StartFrame)
				CurrentFrameNr = EndFrame - fmodf(EndFrame - CurrentFrameNr, EndFrame - StartFrame);
		}
	} else {
		// play animation non looped

		CurrentFrameNr += timeMs * FramesPerSecond;
		if (FramesPerSecond > 0.f) { // forwards...
			if (CurrentFrameNr > EndFrame) {
				CurrentFrameNr = EndFrame;
				if (LoopCallBack)
					LoopCallBack->OnAnimationEnd(this);
			}
		} else // backwards...
		{
			if (CurrentFrameNr < StartFrame) {
				CurrentFrameNr = StartFrame;
				if (LoopCallBack)
					LoopCallBack->OnAnimationEnd(this);
			}
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
		return Mesh->getMesh(getFrameNr());
	} else {
		// As multiple scene nodes may be sharing the same skinned mesh, we have to
		// re-animate it every frame to ensure that this node gets the mesh that it needs.

		CSkinnedMesh *skinnedMesh = static_cast<CSkinnedMesh *>(Mesh);

		if (JointMode == EJUOR_CONTROL) // write to mesh
			skinnedMesh->transferJointsToMesh(JointChildSceneNodes);
		else
			skinnedMesh->animateMesh(getFrameNr(), 1.0f);

		// Update the skinned mesh for the current joint transforms.
		skinnedMesh->skinMesh();

		if (JointMode == EJUOR_READ) { // read from mesh
			skinnedMesh->recoverJointsFromMesh(JointChildSceneNodes);

			//---slow---
			for (u32 n = 0; n < JointChildSceneNodes.size(); ++n)
				if (JointChildSceneNodes[n]->getParent() == this) {
					JointChildSceneNodes[n]->updateAbsolutePositionOfAllChildren(); // temp, should be an option
				}
		}

		if (JointMode == EJUOR_CONTROL) {
			// For meshes other than EJUOR_CONTROL, this is done by calling animateMesh()
			skinnedMesh->updateBoundingBox();
		}

		return skinnedMesh;
	}
}

//! OnAnimate() is called just before rendering the whole scene.
void CAnimatedMeshSceneNode::OnAnimate(u32 timeMs)
{
	if (LastTimeMs == 0) { // first frame
		LastTimeMs = timeMs;
	}

	// set CurrentFrameNr
	buildFrameNr(timeMs - LastTimeMs);
	LastTimeMs = timeMs;

	IAnimatedMeshSceneNode::OnAnimate(timeMs);
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

	if (m) {
		Box = m->getBoundingBox();
	} else {
#ifdef _DEBUG
		os::Printer::log("Animated Mesh returned no mesh to render.", Mesh->getDebugName(), ELL_WARNING);
#endif
		return;
	}

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
		debug_mat.AntiAliasing = 0;
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

		if (DebugDataVisible & scene::EDS_BBOX)
			driver->draw3DBox(Box, video::SColor(255, 255, 255, 255));

		// show bounding box
		if (DebugDataVisible & scene::EDS_BBOX_BUFFERS) {
			for (u32 g = 0; g < m->getMeshBufferCount(); ++g) {
				const IMeshBuffer *mb = m->getMeshBuffer(g);

				if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer *)mb)->Transformation);
				driver->draw3DBox(mb->getBoundingBox(), video::SColor(255, 190, 128, 128));
			}
		}

		// show skeleton
		if (DebugDataVisible & scene::EDS_SKELETON) {
			if (Mesh->getMeshType() == EAMT_SKINNED) {
				// draw skeleton

				for (u32 g = 0; g < ((ISkinnedMesh *)Mesh)->getAllJoints().size(); ++g) {
					ISkinnedMesh::SJoint *joint = ((ISkinnedMesh *)Mesh)->getAllJoints()[g];

					for (u32 n = 0; n < joint->Children.size(); ++n) {
						driver->draw3DLine(joint->GlobalAnimatedMatrix.getTranslation(),
								joint->Children[n]->GlobalAnimatedMatrix.getTranslation(),
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

	ISkinnedMesh *skinnedMesh = (ISkinnedMesh *)Mesh;

	const std::optional<u32> number = skinnedMesh->getJointNumber(jointName);

	if (!number.has_value()) {
		os::Printer::log("Joint with specified name not found in skinned mesh", jointName, ELL_DEBUG);
		return 0;
	}

	if (JointChildSceneNodes.size() <= *number) {
		os::Printer::log("Joint was found in mesh, but is not loaded into node", jointName, ELL_WARNING);
		return 0;
	}

	return JointChildSceneNodes[*number];
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

	if (JointChildSceneNodes.size() <= jointID) {
		os::Printer::log("Joint not loaded into node", ELL_WARNING);
		return 0;
	}

	return JointChildSceneNodes[jointID];
}

//! Gets joint count.
u32 CAnimatedMeshSceneNode::getJointCount() const
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return 0;

	ISkinnedMesh *skinnedMesh = (ISkinnedMesh *)Mesh;

	return skinnedMesh->getJointCount();
}

//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
bool CAnimatedMeshSceneNode::removeChild(ISceneNode *child)
{
	if (ISceneNode::removeChild(child)) {
		if (JointsUsed) { // stop weird bugs caused while changing parents as the joints are being created
			for (u32 i = 0; i < JointChildSceneNodes.size(); ++i) {
				if (JointChildSceneNodes[i] == child) {
					JointChildSceneNodes[i] = 0; // remove link to child
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

//! Sets a callback interface which will be called if an animation
//! playback has ended. Set this to 0 to disable the callback again.
void CAnimatedMeshSceneNode::setAnimationEndCallback(IAnimationEndCallBack *callback)
{
	if (callback == LoopCallBack)
		return;

	if (LoopCallBack)
		LoopCallBack->drop();

	LoopCallBack = callback;

	if (LoopCallBack)
		LoopCallBack->grab();
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

	IMesh *m = Mesh->getMesh(0);
	if (m) {
		Materials.clear();
		Materials.reallocate(m->getMeshBufferCount());

		for (u32 i = 0; i < m->getMeshBufferCount(); ++i) {
			IMeshBuffer *mb = m->getMeshBuffer(i);
			if (mb)
				Materials.push_back(mb->getMaterial());
			else
				Materials.push_back(video::SMaterial());
		}
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

//! Set the joint update mode (0-unused, 1-get joints only, 2-set joints only, 3-move and set)
void CAnimatedMeshSceneNode::setJointMode(E_JOINT_UPDATE_ON_RENDER mode)
{
	checkJoints();
	JointMode = mode;
}

//! Sets the transition time in seconds (note: This needs to enable joints, and setJointmode maybe set to 2)
//! you must call animateJoints(), or the mesh will not animate
void CAnimatedMeshSceneNode::setTransitionTime(f32 time)
{
	const u32 ttime = (u32)core::floor32(time * 1000.0f);
	if (TransitionTime == ttime)
		return;
	TransitionTime = ttime;
	if (ttime != 0)
		setJointMode(EJUOR_CONTROL);
	else
		setJointMode(EJUOR_NONE);
}

//! render mesh ignoring its transformation. Used with ragdolls. (culling is unaffected)
void CAnimatedMeshSceneNode::setRenderFromIdentity(bool enable)
{
	RenderFromIdentity = enable;
}

//! updates the joint positions of this mesh
void CAnimatedMeshSceneNode::animateJoints(bool CalculateAbsolutePositions)
{
	if (Mesh && Mesh->getMeshType() == EAMT_SKINNED) {
		checkJoints();
		const f32 frame = getFrameNr(); // old?

		CSkinnedMesh *skinnedMesh = static_cast<CSkinnedMesh *>(Mesh);

		skinnedMesh->transferOnlyJointsHintsToMesh(JointChildSceneNodes);
		skinnedMesh->animateMesh(frame, 1.0f);
		skinnedMesh->recoverJointsFromMesh(JointChildSceneNodes);

		//-----------------------------------------
		//		Transition
		//-----------------------------------------

		if (Transiting != 0.f) {
			// Init additional matrices
			if (PretransitingSave.size() < JointChildSceneNodes.size()) {
				for (u32 n = PretransitingSave.size(); n < JointChildSceneNodes.size(); ++n)
					PretransitingSave.push_back(core::matrix4());
			}

			for (u32 n = 0; n < JointChildSceneNodes.size(); ++n) {
				//------Position------

				JointChildSceneNodes[n]->setPosition(
						core::lerp(
								PretransitingSave[n].getTranslation(),
								JointChildSceneNodes[n]->getPosition(),
								TransitingBlend));

				//------Rotation------

				// Code is slow, needs to be fixed up

				const core::quaternion RotationStart(PretransitingSave[n].getRotationDegrees() * core::DEGTORAD);
				const core::quaternion RotationEnd(JointChildSceneNodes[n]->getRotation() * core::DEGTORAD);

				core::quaternion QRotation;
				QRotation.slerp(RotationStart, RotationEnd, TransitingBlend);

				core::vector3df tmpVector;
				QRotation.toEuler(tmpVector);
				tmpVector *= core::RADTODEG; // convert from radians back to degrees
				JointChildSceneNodes[n]->setRotation(tmpVector);

				//------Scale------

				// JointChildSceneNodes[n]->setScale(
				//		core::lerp(
				//			PretransitingSave[n].getScale(),
				//			JointChildSceneNodes[n]->getScale(),
				//			TransitingBlend));
			}
		}

		if (CalculateAbsolutePositions) {
			//---slow---
			for (u32 n = 0; n < JointChildSceneNodes.size(); ++n) {
				if (JointChildSceneNodes[n]->getParent() == this) {
					JointChildSceneNodes[n]->updateAbsolutePositionOfAllChildren(); // temp, should be an option
				}
			}
		}
	}
}

/*!
 */
void CAnimatedMeshSceneNode::checkJoints()
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return;

	if (!JointsUsed) {
		for (u32 i = 0; i < JointChildSceneNodes.size(); ++i)
			removeChild(JointChildSceneNodes[i]);
		JointChildSceneNodes.clear();

		// Create joints for SkinnedMesh
		((CSkinnedMesh *)Mesh)->addJoints(JointChildSceneNodes, this, SceneManager);
		((CSkinnedMesh *)Mesh)->recoverJointsFromMesh(JointChildSceneNodes);

		JointsUsed = true;
		JointMode = EJUOR_READ;
	}
}

/*!
 */
void CAnimatedMeshSceneNode::beginTransition()
{
	if (!JointsUsed)
		return;

	if (TransitionTime != 0) {
		// Check the array is big enough
		if (PretransitingSave.size() < JointChildSceneNodes.size()) {
			for (u32 n = PretransitingSave.size(); n < JointChildSceneNodes.size(); ++n)
				PretransitingSave.push_back(core::matrix4());
		}

		// Copy the position of joints
		for (u32 n = 0; n < JointChildSceneNodes.size(); ++n)
			PretransitingSave[n] = JointChildSceneNodes[n]->getRelativeTransformation();

		Transiting = core::reciprocal((f32)TransitionTime);
	}
	TransitingBlend = 0.f;
}

/*!
 */
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
	newNode->JointMode = JointMode;
	newNode->JointsUsed = JointsUsed;
	newNode->TransitionTime = TransitionTime;
	newNode->Transiting = Transiting;
	newNode->TransitingBlend = TransitingBlend;
	newNode->Looping = Looping;
	newNode->ReadOnlyMaterials = ReadOnlyMaterials;
	newNode->LoopCallBack = LoopCallBack;
	if (newNode->LoopCallBack)
		newNode->LoopCallBack->grab();
	newNode->PassCount = PassCount;
	newNode->JointChildSceneNodes = JointChildSceneNodes;
	newNode->PretransitingSave = PretransitingSave;
	newNode->RenderFromIdentity = RenderFromIdentity;

	return newNode;
}

} // end namespace scene
} // end namespace irr
