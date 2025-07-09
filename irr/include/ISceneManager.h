// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "irrArray.h"
#include "vector3d.h"
#include "dimension2d.h"
#include "SColor.h"
#include "ESceneNodeTypes.h"

struct SEvent;

namespace io
{
class IReadFile;
} // end namespace io

namespace video
{
class IVideoDriver;
} // end namespace video

namespace scene
{
//! Enumeration for render passes.
/** A parameter passed to the registerNodeForRendering() method of the ISceneManager,
specifying when the node wants to be drawn in relation to the other nodes.
Note: Despite the numbering this is not used as bit-field.
*/
enum E_SCENE_NODE_RENDER_PASS
{
	//! No pass currently active
	ESNRP_NONE = 0,

	//! Camera pass. The active view is set up here. The very first pass.
	ESNRP_CAMERA = 1,

	//! This is used for sky boxes.
	ESNRP_SKY_BOX = 4,

	//! All normal objects can use this for registering themselves.
	/** This value will never be returned by
	ISceneManager::getSceneNodeRenderPass(). The scene manager
	will determine by itself if an object is transparent or solid
	and register the object as ESNRT_TRANSPARENT or ESNRP_SOLID
	automatically if you call registerNodeForRendering with this
	value (which is default). Note that it will register the node
	only as ONE type. If your scene node has both solid and
	transparent material types register it twice (one time as
	ESNRP_SOLID, the other time as ESNRT_TRANSPARENT) and in the
	render() method call getSceneNodeRenderPass() to find out the
	current render pass and render only the corresponding parts of
	the node. */
	ESNRP_AUTOMATIC = 24,

	//! Solid scene nodes or special scene nodes without materials.
	ESNRP_SOLID = 8,

	//! Transparent scene nodes, drawn after solid nodes. They are sorted from back to front and drawn in that order.
	ESNRP_TRANSPARENT = 16,

	//! Transparent effect scene nodes, drawn after Transparent nodes. They are sorted from back to front and drawn in that order.
	ESNRP_TRANSPARENT_EFFECT = 32,

	//! Drawn after transparent effect nodes. For custom gui's. Unsorted (in order nodes registered themselves).
	ESNRP_GUI = 128

};

class IAnimatedMesh;
class IAnimatedMeshSceneNode;
class IBillboardSceneNode;
class ICameraSceneNode;
class IDummyTransformationSceneNode;
class IMesh;
class SkinnedMesh;
class IMeshCache;
class ISceneCollisionManager;
class IMeshLoader;
class IMeshManipulator;
class IMeshSceneNode;
class ISceneNode;

//! The Scene Manager manages scene nodes, mesh resources, cameras and all the other stuff.
/** All Scene nodes can be created only here.
A scene node is a node in the hierarchical scene graph. Every scene node
may have children, which are other scene nodes. Children move relative
the their parents position. If the parent of a node is not visible, its
children won't be visible, too. In this way, it is for example easily
possible to attach a light to a moving car or to place a walking
character on a moving platform on a moving ship.
The SceneManager is also able to load 3d mesh files of different
formats. Take a look at getMesh() to find out what formats are
supported. If these formats are not enough, use
addExternalMeshLoader() to add new formats to the engine.
*/
class ISceneManager : public virtual IReferenceCounted
{
public:
	//! Get pointer to an animatable mesh. Loads the file if not loaded already.
	/**
	 * If you want to remove a loaded mesh from the cache again, use removeMesh().
	 * If you would like to implement and add your own file format loader to Irrlicht,
	 * see addExternalMeshLoader().
	 * \param file File handle of the mesh to load.
	 * \return Null if failed, otherwise pointer to the mesh.
	 * This pointer should not be dropped. See IReferenceCounted::drop() for more information.
	 **/
	virtual IAnimatedMesh *getMesh(io::IReadFile *file) = 0;

	//! Get interface to the mesh cache which is shared between all existing scene managers.
	/** With this interface, it is possible to manually add new loaded
	meshes (if ISceneManager::getMesh() is not sufficient), to remove them and to iterate
	through already loaded meshes. */
	virtual IMeshCache *getMeshCache() = 0;

	//! Get the video driver.
	/** \return Pointer to the video Driver.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual video::IVideoDriver *getVideoDriver() = 0;

	//! Adds a scene node for rendering an animated mesh model.
	/** \param mesh: Pointer to the loaded animated mesh to be displayed.
	\param parent: Parent of the scene node. Can be NULL if no parent.
	\param id: Id of the node. This id can be used to identify the scene node.
	\param position: Position of the space relative to its parent where the
	scene node will be placed.
	\param rotation: Initial rotation of the scene node.
	\param scale: Initial scale of the scene node.
	\param alsoAddIfMeshPointerZero: Add the scene node even if a 0 pointer is passed.
	\return Pointer to the created scene node.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual IAnimatedMeshSceneNode *addAnimatedMeshSceneNode(IAnimatedMesh *mesh,
			ISceneNode *parent = 0, s32 id = -1,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f),
			bool alsoAddIfMeshPointerZero = false) = 0;

	//! Adds a scene node for rendering a static mesh.
	/** \param mesh: Pointer to the loaded static mesh to be displayed.
	\param parent: Parent of the scene node. Can be NULL if no parent.
	\param id: Id of the node. This id can be used to identify the scene node.
	\param position: Position of the space relative to its parent where the
	scene node will be placed.
	\param rotation: Initial rotation of the scene node.
	\param scale: Initial scale of the scene node.
	\param alsoAddIfMeshPointerZero: Add the scene node even if a 0 pointer is passed.
	\return Pointer to the created scene node.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual IMeshSceneNode *addMeshSceneNode(IMesh *mesh, ISceneNode *parent = 0, s32 id = -1,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f),
			bool alsoAddIfMeshPointerZero = false) = 0;

	//! Adds a camera scene node to the scene graph and sets it as active camera.
	/** This camera does not react on user input.
	If you want to move or animate it, use ISceneNode::setPosition(),
	ICameraSceneNode::setTarget() etc methods.
	By default, a camera's look at position (set with setTarget()) and its scene node
	rotation (set with setRotation()) are independent. If you want to be able to
	control the direction that the camera looks by using setRotation() then call
	ICameraSceneNode::bindTargetAndRotation(true) on it.
	\param position: Position of the space relative to its parent where the camera will be placed.
	\param lookat: Position where the camera will look at. Also known as target.
	\param parent: Parent scene node of the camera. Can be null. If the parent moves,
	the camera will move too.
	\param id: id of the camera. This id can be used to identify the camera.
	\param makeActive Flag whether this camera should become the active one.
	Make sure you always have one active camera.
	\return Pointer to interface to camera if successful, otherwise 0.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ICameraSceneNode *addCameraSceneNode(ISceneNode *parent = 0,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &lookat = core::vector3df(0, 0, 100),
			s32 id = -1, bool makeActive = true) = 0;

	//! Adds a billboard scene node to the scene graph.
	/** A billboard is like a 3d sprite: A 2d element,
	which always looks to the camera. It is usually used for things
	like explosions, fire, lensflares and things like that.
	\param parent Parent scene node of the billboard. Can be null.
	If the parent moves, the billboard will move too.
	\param size Size of the billboard. This size is 2 dimensional
	because a billboard only has width and height.
	\param position Position of the space relative to its parent
	where the billboard will be placed.
	\param id An id of the node. This id can be used to identify
	the node.
	\param colorTop The color of the vertices at the top of the
	billboard (default: white).
	\param colorBottom The color of the vertices at the bottom of
	the billboard (default: white).
	\return Pointer to the billboard if successful, otherwise NULL.
	This pointer should not be dropped. See
	IReferenceCounted::drop() for more information. */
	virtual IBillboardSceneNode *addBillboardSceneNode(ISceneNode *parent = 0,
			const core::dimension2d<f32> &size = core::dimension2d<f32>(10.0f, 10.0f),
			const core::vector3df &position = core::vector3df(0, 0, 0), s32 id = -1,
			video::SColor colorTop = 0xFFFFFFFF, video::SColor colorBottom = 0xFFFFFFFF) = 0;

	//! Adds an empty scene node to the scene graph.
	/** Can be used for doing advanced transformations
	or structuring the scene graph.
	\return Pointer to the created scene node.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ISceneNode *addEmptySceneNode(ISceneNode *parent = 0, s32 id = -1) = 0;

	//! Adds a dummy transformation scene node to the scene graph.
	/** This scene node does not render itself, and does not respond to set/getPosition,
	set/getRotation and set/getScale. Its just a simple scene node that takes a
	matrix as relative transformation, making it possible to insert any transformation
	anywhere into the scene graph.
	\return Pointer to the created scene node.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual IDummyTransformationSceneNode *addDummyTransformationSceneNode(
			ISceneNode *parent = 0, s32 id = -1) = 0;

	//! Gets the root scene node.
	/** This is the scene node which is parent
	of all scene nodes. The root scene node is a special scene node which
	only exists to manage all scene nodes. It will not be rendered and cannot
	be removed from the scene.
	\return Pointer to the root scene node.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ISceneNode *getRootSceneNode() = 0;

	//! Get the first scene node with the specified id.
	/** \param id: The id to search for
	\param start: Scene node to start from. All children of this scene
	node are searched. If null is specified, the root scene node is
	taken.
	\return Pointer to the first scene node with this id,
	and null if no scene node could be found.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ISceneNode *getSceneNodeFromId(s32 id, ISceneNode *start = 0) = 0;

	//! Get the first scene node with the specified name.
	/** \param name: The name to search for
	\param start: Scene node to start from. All children of this scene
	node are searched. If null is specified, the root scene node is
	taken.
	\return Pointer to the first scene node with this id,
	and null if no scene node could be found.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ISceneNode *getSceneNodeFromName(const c8 *name, ISceneNode *start = 0) = 0;

	//! Get the first scene node with the specified type.
	/** \param type: The type to search for
	\param start: Scene node to start from. All children of this scene
	node are searched. If null is specified, the root scene node is
	taken.
	\return Pointer to the first scene node with this type,
	and null if no scene node could be found.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ISceneNode *getSceneNodeFromType(scene::ESCENE_NODE_TYPE type, ISceneNode *start = 0) = 0;

	//! Get scene nodes by type.
	/** \param type: Type of scene node to find (ESNT_ANY will return all child nodes).
	\param outNodes: results will be added to this array (outNodes is not cleared).
	\param start: Scene node to start from. This node and all children of this scene
	node are checked (recursively, so also children of children, etc). If null is specified,
	the root scene node is taken as start-node. */
	virtual void getSceneNodesFromType(ESCENE_NODE_TYPE type,
			core::array<scene::ISceneNode *> &outNodes,
			ISceneNode *start = 0) = 0;

	//! Get the current active camera.
	/** \return The active camera is returned. Note that this can
	be NULL, if there was no camera created yet.
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ICameraSceneNode *getActiveCamera() const = 0;

	//! Sets the currently active camera.
	/** The previous active camera will be deactivated.
	\param camera: The new camera which should be active. */
	virtual void setActiveCamera(ICameraSceneNode *camera) = 0;

	//! Registers a node for rendering it at a specific time.
	/** This method should only be used by SceneNodes when they get a
	ISceneNode::OnRegisterSceneNode() call.
	\param node: Node to register for drawing. Usually scene nodes would set 'this'
	as parameter here because they want to be drawn.
	\param pass: Specifies when the node wants to be drawn in relation to the other nodes.
	For example, if the node is a shadow, it usually wants to be drawn after all other nodes
	and will use ESNRP_SHADOW for this. See scene::E_SCENE_NODE_RENDER_PASS for details.
	Note: This is _not_ a bitfield. If you want to register a note for several render passes, then
	call this function once for each pass.
	\return scene will be rendered ( passed culling ) */
	virtual u32 registerNodeForRendering(ISceneNode *node,
			E_SCENE_NODE_RENDER_PASS pass = ESNRP_AUTOMATIC) = 0;

	//! Clear all nodes which are currently registered for rendering
	/** Usually you don't have to care about this as drawAll will clear nodes
	after rendering them. But sometimes you might have to manually reset this.
	For example when you deleted nodes between registering and rendering. */
	virtual void clearAllRegisteredNodesForRendering() = 0;

	//! Draws all the scene nodes.
	/** This can only be invoked between
	IVideoDriver::beginScene() and IVideoDriver::endScene(). Please note that
	the scene is not only drawn when calling this, but also animated
	by existing scene node animators, culling of scene nodes is done, etc. */
	virtual void drawAll() = 0;

	//! Adds an external mesh loader for extending the engine with new file formats.
	/** If you want the engine to be extended with
	file formats it currently is not able to load (e.g. .cob), just implement
	the IMeshLoader interface in your loading class and add it with this method.
	Using this method it is also possible to override built-in mesh loaders with
	newer or updated versions without the need to recompile the engine.
	\param externalLoader: Implementation of a new mesh loader. */
	virtual void addExternalMeshLoader(IMeshLoader *externalLoader) = 0;

	//! Returns the number of mesh loaders supported by Irrlicht at this time
	virtual u32 getMeshLoaderCount() const = 0;

	//! Retrieve the given mesh loader
	/** \param index The index of the loader to retrieve. This parameter is an 0-based
	array index.
	\return A pointer to the specified loader, 0 if the index is incorrect. */
	virtual IMeshLoader *getMeshLoader(u32 index) const = 0;

	//! Get pointer to the scene collision manager.
	/** \return Pointer to the collision manager
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ISceneCollisionManager *getSceneCollisionManager() = 0;

	//! Get pointer to the mesh manipulator.
	/** \return Pointer to the mesh manipulator
	This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual IMeshManipulator *getMeshManipulator() = 0;

	//! Adds a scene node to the deletion queue.
	/** The scene node is immediately
	deleted when it's secure. Which means when the scene node does not
	execute animators and things like that. This method is for example
	used for deleting scene nodes by their scene node animators. In
	most other cases, a ISceneNode::remove() call is enough, using this
	deletion queue is not necessary.
	See ISceneManager::createDeleteAnimator() for details.
	\param node: Node to delete. */
	virtual void addToDeletionQueue(ISceneNode *node) = 0;

	//! Posts an input event to the environment.
	/** Usually you do not have to
	use this method, it is used by the internal engine. */
	virtual bool postEventFromUser(const SEvent &event) = 0;

	//! Clears the whole scene.
	/** All scene nodes are removed. */
	virtual void clear() = 0;

	//! Get current render pass.
	/** All scene nodes are being rendered in a specific order.
	First lights, cameras, sky boxes, solid geometry, and then transparent
	stuff. During the rendering process, scene nodes may want to know what the scene
	manager is rendering currently, because for example they registered for rendering
	twice, once for transparent geometry and once for solid. When knowing what rendering
	pass currently is active they can render the correct part of their geometry. */
	virtual E_SCENE_NODE_RENDER_PASS getSceneNodeRenderPass() const = 0;

	/**
	 * Sets debug data flags that will be set on every rendered scene node.
	 * Refer to `E_DEBUG_SCENE_TYPE`.
	 * @param setBits bit mask of types to enable
	 * @param unsetBits bit mask of types to disable
	 */
	virtual void setGlobalDebugData(u16 setBits, u16 unsetBits) = 0;

	//! Creates a new scene manager.
	/** This can be used to easily draw and/or store two
	independent scenes at the same time. The mesh cache will be
	shared between all existing scene managers, which means if you
	load a mesh in the original scene manager using for example
	getMesh(), the mesh will be available in all other scene
	managers too, without loading.
	The original/main scene manager will still be there and
	accessible via IrrlichtDevice::getSceneManager(). If you need
	input event in this new scene manager, for example for FPS
	cameras, you'll need to forward input to this manually: Just
	implement an IEventReceiver and call
	yourNewSceneManager->postEventFromUser(), and return true so
	that the original scene manager doesn't get the event.
	Otherwise, all input will go to the main scene manager
	automatically.
	If you no longer need the new scene manager, you should call
	ISceneManager::drop().
	See IReferenceCounted::drop() for more information. */
	virtual ISceneManager *createNewSceneManager(bool cloneContent = false) = 0;

	//! Get a skinned mesh, which is not available as header-only code
	/** Note: You need to drop() the pointer after use again, see IReferenceCounted::drop()
	for details. */
	virtual SkinnedMesh *createSkinnedMesh() = 0;

	//! Get current render pass.
	virtual E_SCENE_NODE_RENDER_PASS getCurrentRenderPass() const = 0;

	//! Set current render pass.
	virtual void setCurrentRenderPass(E_SCENE_NODE_RENDER_PASS nextPass) = 0;

	//! Check if node is culled in current view frustum
	/** Please note that depending on the used culling method this
	check can be rather coarse, or slow. A positive result is
	correct, though, i.e. if this method returns true the node is
	positively not visible. The node might still be invisible even
	if this method returns false.
	\param node The scene node which is checked for culling.
	\return True if node is not visible in the current scene, else
	false. */
	virtual bool isCulled(const ISceneNode *node) const = 0;
};

} // end namespace scene
