// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "ESceneNodeTypes.h"
#include "ECullingTypes.h"
#include "EDebugSceneTypes.h"
#include "SMaterial.h"
#include "irrArray.h"
#include "aabbox3d.h"
#include "matrix4.h"

#include <list>
#include <optional>
#include <string>
#include <cassert>

namespace scene
{
class ISceneNode;
class ISceneManager;

//! Typedef for list of scene nodes
typedef std::list<ISceneNode *> ISceneNodeList;

//! Scene node interface.
/** A scene node is a node in the hierarchical scene graph. Every scene
node may have children, which are also scene nodes. Children move
relative to their parent's position. If the parent of a node is not
visible, its children won't be visible either. In this way, it is for
example easily possible to attach a light to a moving car, or to place
a walking character on a moving platform on a moving ship.
*/
class ISceneNode : virtual public IReferenceCounted
{
public:
	//! Constructor
	ISceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id = -1,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f)) :
			RelativeTranslation(position),
			RelativeRotation(rotation), RelativeScale(scale),
			Parent(0), SceneManager(mgr), ID(id),
			AutomaticCullingState(EAC_BOX), DebugDataVisible(EDS_OFF),
			IsVisible(true), IsDebugObject(false)
	{
		if (parent)
			parent->addChild(this);

		updateAbsolutePosition();
	}

	//! Destructor
	virtual ~ISceneNode()
	{
		// delete all children
		removeAll();
	}

	//! This method is called just before the rendering process of the whole scene.
	/** Nodes may register themselves in the render pipeline during this call,
	precalculate the geometry which should be rendered, and prevent their
	children from being able to register themselves if they are clipped by simply
	not calling their OnRegisterSceneNode method.
	If you are implementing your own scene node, you should overwrite this method
	with an implementation code looking like this:
	\code
	if (IsVisible)
		SceneManager->registerNodeForRendering(this);

	ISceneNode::OnRegisterSceneNode();
	\endcode
	*/
	virtual void OnRegisterSceneNode()
	{
		if (IsVisible) {
			ISceneNodeList::iterator it = Children.begin();
			for (; it != Children.end(); ++it)
				(*it)->OnRegisterSceneNode();
		}
	}

	//! OnAnimate() is called just before rendering the whole scene.
	/** Nodes may calculate or store animations here, and may do other useful things,
	depending on what they are. Also, OnAnimate() should be called for all
	child scene nodes here. This method will be called once per frame, independent
	of whether the scene node is visible or not.
	\param timeMs Current time in milliseconds. */
	virtual void OnAnimate(u32 timeMs)
	{
		if (!IsVisible && Children.empty())
			return;

		updateAbsolutePosition();
		for (auto *child : Children)
			child->OnAnimate(timeMs);
	}

	//! Renders the node.
	virtual void render() = 0;

	//! Returns the name of the node.
	/** \return Name as character string. */
	virtual const std::optional<std::string> &getName() const
	{
		return Name;
	}

	//! Sets the name of the node.
	/** \param name New name of the scene node. */
	virtual void setName(const std::optional<std::string> &name)
	{
		Name = name;
	}

	//! Get the axis aligned, not transformed bounding box of this node.
	/** This means that if this node is an animated 3d character,
	moving in a room, the bounding box will always be around the
	origin. To get the box in real world coordinates, just
	transform it with the matrix you receive with
	getAbsoluteTransformation() or simply use
	getTransformedBoundingBox(), which does the same.
	\return The non-transformed bounding box. */
	virtual const core::aabbox3d<f32> &getBoundingBox() const = 0;

	//! Get the axis aligned, transformed and animated absolute bounding box of this node.
	/** Note: The result is still an axis-aligned bounding box, so it's size
	changes with rotation.
	\return The transformed bounding box. */
	virtual const core::aabbox3d<f32> getTransformedBoundingBox() const
	{
		core::aabbox3d<f32> box = getBoundingBox();
		AbsoluteTransformation.transformBoxEx(box);
		return box;
	}

	//! Get a the 8 corners of the original bounding box transformed and
	//! animated by the absolute transformation.
	/** Note: The result is _not_ identical to getTransformedBoundingBox().getEdges(),
	but getting an aabbox3d of these edges would then be identical.
	\param edges Receives an array with the transformed edges */
	virtual void getTransformedBoundingBoxEdges(core::array<core::vector3d<f32>> &edges) const
	{
		edges.set_used(8);
		getBoundingBox().getEdges(edges.pointer());
		for (u32 i = 0; i < 8; ++i)
			AbsoluteTransformation.transformVect(edges[i]);
	}

	//! Get the absolute transformation of the node. Is recalculated every OnAnimate()-call.
	/** NOTE: For speed reasons the absolute transformation is not
	automatically recalculated on each change of the relative
	transformation or by a transformation change of an parent. Instead the
	update usually happens once per frame in OnAnimate. You can enforce
	an update with updateAbsolutePosition().
	\return The absolute transformation matrix. */
	virtual const core::matrix4 &getAbsoluteTransformation() const
	{
		return AbsoluteTransformation;
	}

	//! Returns the relative transformation of the scene node.
	/** The relative transformation is stored internally as 3
	vectors: translation, rotation and scale. To get the relative
	transformation matrix, it is calculated from these values.
	\return The relative transformation matrix. */
	virtual core::matrix4 getRelativeTransformation() const
	{
		core::matrix4 mat;
		mat.setRotationDegrees(RelativeRotation);
		mat.setTranslation(RelativeTranslation);

		if (RelativeScale != core::vector3df(1.f, 1.f, 1.f)) {
			core::matrix4 smat;
			smat.setScale(RelativeScale);
			mat *= smat;
		}

		return mat;
	}

	//! Returns whether the node should be visible (if all of its parents are visible).
	/** This is only an option set by the user, but has nothing to
	do with geometry culling
	\return The requested visibility of the node, true means
	visible (if all parents are also visible). */
	virtual bool isVisible() const
	{
		return IsVisible;
	}

	//! Check whether the node is truly visible, taking into accounts its parents' visibility
	/** \return true if the node and all its parents are visible,
	false if this or any parent node is invisible. */
	virtual bool isTrulyVisible() const
	{
		if (!IsVisible)
			return false;

		if (!Parent)
			return true;

		return Parent->isTrulyVisible();
	}

	//! Sets if the node should be visible or not.
	/** All children of this node won't be visible either, when set
	to false. Invisible nodes are not valid candidates for selection by
	collision manager bounding box methods.
	\param isVisible If the node shall be visible. */
	virtual void setVisible(bool isVisible)
	{
		IsVisible = isVisible;
	}

	//! Get the id of the scene node.
	/** This id can be used to identify the node.
	\return The id. */
	virtual s32 getID() const
	{
		return ID;
	}

	//! Sets the id of the scene node.
	/** This id can be used to identify the node.
	\param id The new id. */
	virtual void setID(s32 id)
	{
		ID = id;
	}

	//! Adds a child to this scene node.
	/** If the scene node already has a parent it is first removed
	from the other parent.
	\param child A pointer to the new child. */
	virtual void addChild(ISceneNode *child)
	{
		if (child && (child != this)) {
			// Change scene manager?
			if (SceneManager != child->SceneManager)
				child->setSceneManager(SceneManager);

			child->grab();
			child->remove(); // remove from old parent
			// Note: This iterator is not invalidated until we erase it.
			child->ThisIterator = Children.insert(Children.end(), child);
			child->Parent = this;
		}
	}

	//! Removes a child from this scene node.
	/**
	\param child A pointer to the child which shall be removed.
	\return True if the child was removed, and false if not,
	e.g. because it belongs to a different parent or no parent. */
	virtual bool removeChild(ISceneNode *child)
	{
		if (child->Parent != this)
			return false;

		// The iterator must be set since the parent is not null.
		assert(child->ThisIterator.has_value());
		auto it = *child->ThisIterator;
		child->ThisIterator = std::nullopt;
		child->Parent = nullptr;
		child->drop();
		Children.erase(it);
		return true;
	}

	//! Removes all children of this scene node
	/** The scene nodes found in the children list are also dropped
	and might be deleted if no other grab exists on them.
	*/
	virtual void removeAll()
	{
		for (auto &child : Children) {
			child->Parent = nullptr;
			child->ThisIterator = std::nullopt;
			child->drop();
		}
		Children.clear();
	}

	//! Removes this scene node from the scene
	/** If no other grab exists for this node, it will be deleted.
	 */
	virtual void remove()
	{
		if (Parent)
			Parent->removeChild(this);
	}

	//! Returns the material based on the zero based index i.
	/** To get the amount of materials used by this scene node, use
	getMaterialCount(). This function is needed for inserting the
	node into the scene hierarchy at an optimal position for
	minimizing renderstate changes, but can also be used to
	directly modify the material of a scene node.
	\param num Zero based index. The maximal value is getMaterialCount() - 1.
	\return The material at that index. */
	virtual video::SMaterial &getMaterial(u32 num)
	{
		// We return a default material since a reference can't be null,
		// but note that writing to this is a mistake either by a child class
		// or the caller, because getMaterialCount() is zero.
		// Doing so will helpfully cause a segfault.
		return const_cast<video::SMaterial&>(video::IdentityMaterial);
	}

	//! Get amount of materials used by this scene node.
	/** \return Current amount of materials of this scene node. */
	virtual u32 getMaterialCount() const
	{
		return 0;
	}

	//! Execute a function on all materials of this scene node.
	/** Useful for setting material properties, e.g. if you want the whole
	mesh to be affected by light. */
	template <typename F>
	void forEachMaterial(F &&fn)
	{
		for (u32 i = 0; i < getMaterialCount(); i++) {
			fn(getMaterial(i));
		}
	}

	//! Gets the scale of the scene node relative to its parent.
	/** This is the scale of this node relative to its parent.
	If you want the absolute scale, use
	getAbsoluteTransformation().getScale()
	\return The scale of the scene node. */
	virtual const core::vector3df &getScale() const
	{
		return RelativeScale;
	}

	//! Sets the relative scale of the scene node.
	/** \param scale New scale of the node, relative to its parent. */
	virtual void setScale(const core::vector3df &scale)
	{
		RelativeScale = scale;
	}

	//! Gets the rotation of the node relative to its parent.
	/** Note that this is the relative rotation of the node.
	If you want the absolute rotation, use
	getAbsoluteTransformation().getRotation()
	\return Current relative rotation of the scene node. */
	virtual const core::vector3df &getRotation() const
	{
		return RelativeRotation;
	}

	//! Sets the rotation of the node relative to its parent.
	/** This only modifies the relative rotation of the node.
	\param rotation New rotation of the node in degrees. */
	virtual void setRotation(const core::vector3df &rotation)
	{
		RelativeRotation = rotation;
	}

	//! Gets the position of the node relative to its parent.
	/** Note that the position is relative to the parent. If you want
	the position in world coordinates, use getAbsolutePosition() instead.
	\return The current position of the node relative to the parent. */
	virtual const core::vector3df &getPosition() const
	{
		return RelativeTranslation;
	}

	//! Sets the position of the node relative to its parent.
	/** Note that the position is relative to the parent.
	\param newpos New relative position of the scene node. */
	virtual void setPosition(const core::vector3df &newpos)
	{
		RelativeTranslation = newpos;
	}

	//! Gets the absolute position of the node in world coordinates.
	/** If you want the position of the node relative to its parent,
	use getPosition() instead.
	NOTE: For speed reasons the absolute position is not
	automatically recalculated on each change of the relative
	position or by a position change of an parent. Instead the
	update usually happens once per frame in OnAnimate. You can enforce
	an update with updateAbsolutePosition().
	\return The current absolute position of the scene node (updated on last call of updateAbsolutePosition). */
	virtual core::vector3df getAbsolutePosition() const
	{
		return AbsoluteTransformation.getTranslation();
	}

	//! Set a culling style or disable culling completely.
	/** Box cullling (EAC_BOX) is set by default. Note that not
	all SceneNodes support culling and that some nodes always cull
	their geometry because it is their only reason for existence,
	for example the OctreeSceneNode.
	\param state The culling state to be used. Check E_CULLING_TYPE for possible values.*/
	void setAutomaticCulling(u16 state)
	{
		AutomaticCullingState = state;
	}

	//! Gets the automatic culling state.
	/** \return The automatic culling state. */
	u16 getAutomaticCulling() const
	{
		return AutomaticCullingState;
	}

	//! Sets if debug data like bounding boxes should be drawn.
	/** A bitwise OR of the types from @ref scene::E_DEBUG_SCENE_TYPE.
	Please note that not all scene nodes support all debug data types.
	\param state The debug data visibility state to be used. */
	virtual void setDebugDataVisible(u16 state)
	{
		DebugDataVisible = state;
	}

	//! Returns if debug data like bounding boxes are drawn.
	/** \return A bitwise OR of the debug data values from
	@ref scene::E_DEBUG_SCENE_TYPE that are currently visible. */
	u16 isDebugDataVisible() const
	{
		return DebugDataVisible;
	}

	//! Sets if this scene node is a debug object.
	/** Debug objects have some special properties, for example they can be easily
	excluded from collision detection or from serialization, etc. */
	void setIsDebugObject(bool debugObject)
	{
		IsDebugObject = debugObject;
	}

	//! Returns if this scene node is a debug object.
	/** Debug objects have some special properties, for example they can be easily
	excluded from collision detection or from serialization, etc.
	\return If this node is a debug object, true is returned. */
	bool isDebugObject() const
	{
		return IsDebugObject;
	}

	//! Returns a const reference to the list of all children.
	/** \return The list of all children of this node. */
	const std::list<ISceneNode *> &getChildren() const
	{
		return Children;
	}

	//! Changes the parent of the scene node.
	/** \param newParent The new parent to be used. */
	virtual void setParent(ISceneNode *newParent)
	{
		grab();
		remove();

		if (newParent)
			newParent->addChild(this);

		drop();
	}

	//! Updates the absolute position based on the relative and the parents position
	/** Note: This does not recursively update the parents absolute positions, so if you have a deeper
		hierarchy you might want to update the parents first.*/
	virtual void updateAbsolutePosition()
	{
		if (Parent) {
			AbsoluteTransformation =
					Parent->getAbsoluteTransformation() * getRelativeTransformation();
		} else
			AbsoluteTransformation = getRelativeTransformation();
	}

	//! Returns the parent of this scene node
	/** \return A pointer to the parent. */
	scene::ISceneNode *getParent() const
	{
		return Parent;
	}

	//! Returns type of the scene node
	/** \return The type of this node. */
	virtual ESCENE_NODE_TYPE getType() const
	{
		return ESNT_UNKNOWN;
	}

	//! Creates a clone of this scene node and its children.
	/** \param newParent An optional new parent.
	\param newManager An optional new scene manager.
	\return The newly created clone of this node. */
	virtual ISceneNode *clone(ISceneNode *newParent = 0, ISceneManager *newManager = 0)
	{
		return 0; // to be implemented by derived classes
	}

	//! Retrieve the scene manager for this node.
	/** \return The node's scene manager. */
	virtual ISceneManager *getSceneManager(void) const { return SceneManager; }

protected:
	//! A clone function for the ISceneNode members.
	/** This method can be used by clone() implementations of
	derived classes
	\param toCopyFrom The node from which the values are copied
	\param newManager The new scene manager. */
	void cloneMembers(ISceneNode *toCopyFrom, ISceneManager *newManager)
	{
		Name = toCopyFrom->Name;
		AbsoluteTransformation = toCopyFrom->AbsoluteTransformation;
		RelativeTranslation = toCopyFrom->RelativeTranslation;
		RelativeRotation = toCopyFrom->RelativeRotation;
		RelativeScale = toCopyFrom->RelativeScale;
		ID = toCopyFrom->ID;
		AutomaticCullingState = toCopyFrom->AutomaticCullingState;
		DebugDataVisible = toCopyFrom->DebugDataVisible;
		IsVisible = toCopyFrom->IsVisible;
		IsDebugObject = toCopyFrom->IsDebugObject;

		if (newManager)
			SceneManager = newManager;
		else
			SceneManager = toCopyFrom->SceneManager;

		// clone children

		ISceneNodeList::iterator it = toCopyFrom->Children.begin();
		for (; it != toCopyFrom->Children.end(); ++it)
			(*it)->clone(this, newManager);
	}

	//! Sets the new scene manager for this node and all children.
	//! Called by addChild when moving nodes between scene managers
	void setSceneManager(ISceneManager *newManager)
	{
		SceneManager = newManager;

		ISceneNodeList::iterator it = Children.begin();
		for (; it != Children.end(); ++it)
			(*it)->setSceneManager(newManager);
	}

	//! Name of the scene node.
	std::optional<std::string> Name;

	//! Absolute transformation of the node.
	core::matrix4 AbsoluteTransformation;

	//! Relative translation of the scene node.
	core::vector3df RelativeTranslation;

	//! Relative rotation of the scene node.
	core::vector3df RelativeRotation;

	//! Relative scale of the scene node.
	core::vector3df RelativeScale;

	//! List of all children of this node
	std::list<ISceneNode *> Children;

	//! Iterator pointing to this node in the parent's child list.
	std::optional<ISceneNodeList::iterator> ThisIterator;

	//! Pointer to the parent
	ISceneNode *Parent;

	//! Pointer to the scene manager
	ISceneManager *SceneManager;

	//! ID of the node.
	s32 ID;

	//! Automatic culling state
	u16 AutomaticCullingState;

	//! Flag if debug data should be drawn, such as Bounding Boxes.
	u16 DebugDataVisible;

	//! Is the node visible?
	bool IsVisible;

	//! Is debug object?
	bool IsDebugObject;
};

} // end namespace scene
