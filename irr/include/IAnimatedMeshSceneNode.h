// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"
#include "IBoneSceneNode.h"
#include "IAnimatedMesh.h"

namespace scene
{
class IAnimatedMeshSceneNode;

//! Scene node capable of displaying an animated mesh.
class IAnimatedMeshSceneNode : public ISceneNode
{
public:
	//! Constructor
	IAnimatedMeshSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f)) :
			ISceneNode(parent, mgr, id, position, rotation, scale) {}

	//! Destructor
	virtual ~IAnimatedMeshSceneNode() {}

	//! Sets the current frame number.
	/** From now on the animation is played from this frame.
	\param frame: Number of the frame to let the animation be started from.
	The frame number must be a valid frame number of the IMesh used by this
	scene node. Set IAnimatedMesh::getMesh() for details. */
	virtual void setCurrentFrame(f32 frame) = 0;

	//! Sets the frame numbers between the animation is looped.
	/** The default is 0 to getMaxFrameNumber() of the mesh.
	Number of played frames is end-start.
	It interpolates toward the last frame but stops when it is reached.
	It does not interpolate back to start even when looping.
	Looping animations should ensure last and first frame-key are identical.
	\param begin: Start frame number of the loop.
	\param end: End frame number of the loop.
	\return True if successful, false if not. */
	virtual bool setFrameLoop(f32 begin, f32 end) = 0;

	//! Sets the speed with which the animation is played.
	/** \param framesPerSecond: Frames per second played. */
	virtual void setAnimationSpeed(f32 framesPerSecond) = 0;

	//! Gets the speed with which the animation is played.
	/** \return Frames per second played. */
	virtual f32 getAnimationSpeed() const = 0;

	//! Get a pointer to a joint in the mesh (if the mesh is a bone based mesh).
	/** With this method it is possible to attach scene nodes to
	joints for example possible to attach a weapon to the left hand
	of an animated model. This example shows how:
	\code
	ISceneNode* hand =
		yourAnimatedMeshSceneNode->getJointNode("LeftHand");
	hand->addChild(weaponSceneNode);
	\endcode
	Please note that the joint returned by this method may not exist
	before this call and the joints in the node were created by it.
	\param jointName: Name of the joint.
	\return Pointer to the scene node which represents the joint
	with the specified name. Returns 0 if the contained mesh is not
	an skinned mesh or the name of the joint could not be found. */
	virtual IBoneSceneNode *getJointNode(const c8 *jointName) = 0;

	//! same as getJointNode(const c8* jointName), but based on id
	virtual IBoneSceneNode *getJointNode(u32 jointID) = 0;

	//! Gets joint count.
	/** \return Amount of joints in the mesh. */
	virtual u32 getJointCount() const = 0;

	//! Returns the currently displayed frame number.
	virtual f32 getFrameNr() const = 0;
	//! Returns the current start frame number.
	virtual f32 getStartFrame() const = 0;
	//! Returns the current end frame number.
	virtual f32 getEndFrame() const = 0;

	//! Sets looping mode which is on by default.
	/** If set to false, animations will not be played looped. */
	virtual void setLoopMode(bool playAnimationLooped) = 0;

	//! returns the current loop mode
	/** When true the animations are played looped */
	virtual bool getLoopMode() const = 0;

	//! Will be called right after the joints have been animated,
	//! but before the transforms have been propagated recursively to children.
	virtual void setOnAnimateCallback(
			const std::function<void(f32 dtime)> &cb) = 0;

	//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
	/** In this way it is possible to change the materials a mesh
	causing all mesh scene nodes referencing this mesh to change
	too. */
	virtual void setReadOnlyMaterials(bool readonly) = 0;

	//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
	virtual bool isReadOnlyMaterials() const = 0;

	//! Sets a new mesh
	virtual void setMesh(IAnimatedMesh *mesh) = 0;

	//! Returns the current mesh
	virtual IAnimatedMesh *getMesh() = 0;

	//! Sets the transition time in seconds
	/** Note: You must call animateJoints(), or the mesh will not animate. */
	virtual void setTransitionTime(f32 Time) = 0;

	//! animates the joints in the mesh based on the current frame.
	/** Also takes in to account transitions. */
	virtual void animateJoints() = 0;

	//! render mesh ignoring its transformation.
	/** Culling is unaffected. */
	virtual void setRenderFromIdentity(bool On) = 0;

	//! Creates a clone of this scene node and its children.
	/** \param newParent An optional new parent.
	\param newManager An optional new scene manager.
	\return The newly created clone of this node. */
	virtual ISceneNode *clone(ISceneNode *newParent = 0, ISceneManager *newManager = 0) = 0;
};

} // end namespace scene
