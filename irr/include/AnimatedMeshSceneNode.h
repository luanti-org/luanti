// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "BoneSceneNode.h"
#include "ISceneNode.h"
#include "IAnimatedMesh.h"

#include "SkinnedMesh.h"
#include "Transform.h"
#include "AnimSpec.h"
#include "irr_ptr.h"
#include "matrix4.h"

namespace scene
{

class AnimatedMeshSceneNode : public ISceneNode
{
public:
	//! constructor
	AnimatedMeshSceneNode(IAnimatedMesh *mesh, ISceneNode *parent, ISceneManager *mgr, s32 id,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f));

	//! frame
	void OnRegisterSceneNode() override;

	//! OnAnimate() is called just before rendering the whole scene.
	void OnAnimate(u32 timeMs) override;

	//! renders the node.
	void render() override;

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override;

	//! returns the material based on the zero based index i. To get the amount
	//! of materials used by this scene node, use getMaterialCount().
	//! This function is needed for inserting the node into the scene hierarchy on a
	//! optimal position for minimizing renderstate changes, but can also be used
	//! to directly modify the material of a scene node.
	video::SMaterial &getMaterial(u32 i) override;

	//! returns amount of materials used by this scene node.
	u32 getMaterialCount() const override;

	//! Removes a child from this scene node.
	//! Implemented here, to be able to remove the shadow properly, if there is one,
	//! or to remove attached child.
	bool removeChild(ISceneNode *child) override;

	//! Returns type of the scene node
	ESCENE_NODE_TYPE getType() const override { return ESNT_ANIMATED_MESH; }

	//! Creates a clone of this scene node and its children.
	/** \param newParent An optional new parent.
	\param newManager An optional new scene manager.
	\return The newly created clone of this node. */
	ISceneNode *clone(ISceneNode *newParent = 0, ISceneManager *newManager = 0) override;

	//! Will be called right after the joints have been animated,
	//! but before the transforms have been propagated recursively to children.
	void setOnAnimateCallback(
			const std::function<void(f32 dtime)> &cb)
	{
		OnAnimateCallback = cb;
	}

	//! Returns a pointer to a child node (nullptr if not found),
	//! which has the same transformation as
	//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
	//! This can be used to attach children.
	BoneSceneNode *getJointNode(const c8 *jointName);

	//! same as getJointNode(const c8* jointName), but based on id
	BoneSceneNode *getJointNode(u32 jointID);

	//! Gets joint count.
	u32 getJointCount() const;

	//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
	/* In this way it is possible to change the materials a mesh causing all mesh scene nodes
	referencing this mesh to change too. */
	void setReadOnlyMaterials(bool readonly);

	//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
	bool isReadOnlyMaterials() const;

	//! Sets a new mesh. Animations will need to be reset after this call.
	void setMesh(IAnimatedMesh *mesh);

	//! Returns the current mesh
	IAnimatedMesh *getMesh()
	{ return Mesh.get(); }

	//! updates the absolute position based on the relative and the parents position
	void updateAbsolutePosition() override;

	void updateJointSceneNodes(const std::vector<SkinnedMesh::SJoint::VariantTransform> &transforms);

	//! Updates the joint positions of this mesh, taking into accoutn transitions
	void animateJoints();

	void addJoints();

	//! render mesh ignoring its transformation. Used with ragdolls. (culling is unaffected)
	void setRenderFromIdentity(bool On);

	AnimSpec &getAnimation()
	{ return anim_spec; }

private:

	void advanceAnimations(f32 dtime_s);
	void checkJoints();
	void copyOldTransforms();

	core::array<video::SMaterial> Materials;
	core::aabbox3d<f32> Box{{0.0f, 0.0f, 0.0f}};
	irr_ptr<IAnimatedMesh> Mesh;

	u32 LastTimeMs;

	AnimSpec anim_spec;

	bool JointsUsed;

	bool ReadOnlyMaterials;
	bool RenderFromIdentity;

	s32 PassCount;
	std::function<void(f32)> OnAnimateCallback;

	struct PerJointData {
		std::vector<irr_ptr<BoneSceneNode>> SceneNodes;
		std::vector<core::matrix4> GlobalMatrices;
		std::vector<std::optional<core::Transform>> PreTransSaves;
		void setN(u16 n) {
			SceneNodes.clear();
			SceneNodes.resize(n);
			GlobalMatrices.clear();
			GlobalMatrices.resize(n);
			PreTransSaves.clear();
			PreTransSaves.resize(n);
		}
	};

	PerJointData PerJoint;
};

} // end namespace scene
