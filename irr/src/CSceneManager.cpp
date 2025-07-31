// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include <algorithm>
#include <cassert>

#include "CSceneManager.h"
#include "IVideoDriver.h"
#include "IFileSystem.h"
#include "CMeshCache.h"
#include "IGUIEnvironment.h"
#include "IMaterialRenderer.h"
#include "IReadFile.h"
#include "IWriteFile.h"

#include "os.h"

#include "SkinnedMesh.h"
#include "CXMeshFileLoader.h"
#include "COBJMeshFileLoader.h"
#include "CB3DMeshFileLoader.h"
#include "CGLTFMeshFileLoader.h"
#include "CBillboardSceneNode.h"
#include "CAnimatedMeshSceneNode.h"
#include "CCameraSceneNode.h"
#include "CMeshSceneNode.h"
#include "CDummyTransformationSceneNode.h"
#include "CEmptySceneNode.h"

#include "CSceneCollisionManager.h"

namespace scene
{

//! constructor
CSceneManager::CSceneManager(video::IVideoDriver *driver,
		gui::ICursorControl *cursorControl, IMeshCache *cache) :
		ISceneNode(0, 0),
		Driver(driver),
		CursorControl(cursorControl),
		ActiveCamera(0),
		MeshCache(cache), CurrentRenderPass(ESNRP_NONE)
{
	// root node's scene manager
	SceneManager = this;

	if (Driver)
		Driver->grab();

	if (CursorControl)
		CursorControl->grab();

	// create mesh cache if not there already
	if (!MeshCache)
		MeshCache = new CMeshCache();
	else
		MeshCache->grab();

	// create collision manager
	CollisionManager = new CSceneCollisionManager(this, Driver);

	// add file format loaders. add the least commonly used ones first,
	// as these are checked last

	// TODO: now that we have multiple scene managers, these should be
	// shallow copies from the previous manager if there is one.

	MeshLoaderList.push_back(new CXMeshFileLoader(this));
	MeshLoaderList.push_back(new COBJMeshFileLoader(this));
	MeshLoaderList.push_back(new CB3DMeshFileLoader(this));
	MeshLoaderList.push_back(new CGLTFMeshFileLoader());
}

//! destructor
CSceneManager::~CSceneManager()
{
	clearDeletionList();

	//! force to remove hardwareTextures from the driver
	//! because Scenes may hold internally data bounded to sceneNodes
	//! which may be destroyed twice
	if (Driver)
		Driver->removeAllHardwareBuffers();

	if (CursorControl)
		CursorControl->drop();

	if (CollisionManager)
		CollisionManager->drop();

	for (auto *loader : MeshLoaderList)
		loader->drop();

	if (ActiveCamera)
		ActiveCamera->drop();
	ActiveCamera = 0;

	if (MeshCache)
		MeshCache->drop();

	// remove all nodes before dropping the driver
	// as render targets may be destroyed twice

	removeAll();

	if (Driver)
		Driver->drop();
}

//! gets an animatable mesh. loads it if needed. returned pointer must not be dropped.
IAnimatedMesh *CSceneManager::getMesh(io::IReadFile *file)
{
	if (!file)
		return 0;

	io::path name = file->getFileName();
	IAnimatedMesh *msh = MeshCache->getMeshByName(name);
	if (msh)
		return msh;

	msh = getUncachedMesh(file, name, name);

	return msh;
}

// load and create a mesh which we know already isn't in the cache and put it in there
IAnimatedMesh *CSceneManager::getUncachedMesh(io::IReadFile *file, const io::path &filename, const io::path &cachename)
{
	// iterate the list in reverse order so user-added loaders can override the built-in ones

	bool unsupported = true;
	for (auto it = MeshLoaderList.rbegin(); it != MeshLoaderList.rend(); it++) {
		if ((*it)->isALoadableFileExtension(filename)) {
			unsupported = false;
			// reset file to avoid side effects of previous calls to createMesh
			file->seek(0);
			IAnimatedMesh *msh = (*it)->createMesh(file);
			if (msh) {
				MeshCache->addMesh(cachename, msh);
				msh->drop();
				os::Printer::log("Loaded mesh", filename, ELL_DEBUG);
				return msh;
			}
		}
	}

	os::Printer::log(unsupported
			? "Could not load mesh, file format seems to be unsupported"
			: "Attempt to load mesh failed",
			filename, ELL_ERROR);
	return nullptr;
}

//! returns the video driver
video::IVideoDriver *CSceneManager::getVideoDriver()
{
	return Driver;
}

//! adds a scene node for rendering a static mesh
//! the returned pointer must not be dropped.
IMeshSceneNode *CSceneManager::addMeshSceneNode(IMesh *mesh, ISceneNode *parent, s32 id,
		const core::vector3df &position, const core::vector3df &rotation,
		const core::vector3df &scale, bool alsoAddIfMeshPointerZero)
{
	if (!alsoAddIfMeshPointerZero && !mesh)
		return 0;

	if (!parent)
		parent = this;

	IMeshSceneNode *node = new CMeshSceneNode(mesh, parent, this, id, position, rotation, scale);
	node->drop();

	return node;
}

//! adds a scene node for rendering an animated mesh model
IAnimatedMeshSceneNode *CSceneManager::addAnimatedMeshSceneNode(IAnimatedMesh *mesh, ISceneNode *parent, s32 id,
		const core::vector3df &position, const core::vector3df &rotation,
		const core::vector3df &scale, bool alsoAddIfMeshPointerZero)
{
	if (!alsoAddIfMeshPointerZero && !mesh)
		return 0;

	if (!parent)
		parent = this;

	IAnimatedMeshSceneNode *node =
			new CAnimatedMeshSceneNode(mesh, parent, this, id, position, rotation, scale);
	node->drop();

	return node;
}

//! Adds a camera scene node to the tree and sets it as active camera.
//! \param position: Position of the space relative to its parent where the camera will be placed.
//! \param lookat: Position where the camera will look at. Also known as target.
//! \param parent: Parent scene node of the camera. Can be null. If the parent moves,
//! the camera will move too.
//! \return Returns pointer to interface to camera
ICameraSceneNode *CSceneManager::addCameraSceneNode(ISceneNode *parent,
		const core::vector3df &position, const core::vector3df &lookat, s32 id,
		bool makeActive)
{
	if (!parent)
		parent = this;

	ICameraSceneNode *node = new CCameraSceneNode(parent, this, id, position, lookat);

	if (makeActive)
		setActiveCamera(node);
	node->drop();

	return node;
}

//! Adds a billboard scene node to the scene. A billboard is like a 3d sprite: A 2d element,
//! which always looks to the camera. It is usually used for things like explosions, fire,
//! lensflares and things like that.
IBillboardSceneNode *CSceneManager::addBillboardSceneNode(ISceneNode *parent,
		const core::dimension2d<f32> &size, const core::vector3df &position, s32 id,
		video::SColor colorTop, video::SColor colorBottom)
{
	if (!parent)
		parent = this;

	IBillboardSceneNode *node = new CBillboardSceneNode(parent, this, id, position, size,
			colorTop, colorBottom);
	node->drop();

	return node;
}

//! Adds an empty scene node.
ISceneNode *CSceneManager::addEmptySceneNode(ISceneNode *parent, s32 id)
{
	if (!parent)
		parent = this;

	ISceneNode *node = new CEmptySceneNode(parent, this, id);
	node->drop();

	return node;
}

//! Adds a dummy transformation scene node to the scene graph.
IDummyTransformationSceneNode *CSceneManager::addDummyTransformationSceneNode(
		ISceneNode *parent, s32 id)
{
	if (!parent)
		parent = this;

	IDummyTransformationSceneNode *node = new CDummyTransformationSceneNode(
			parent, this, id);
	node->drop();

	return node;
}

//! Returns the root scene node. This is the scene node which is parent
//! of all scene nodes. The root scene node is a special scene node which
//! only exists to manage all scene nodes. It is not rendered and cannot
//! be removed from the scene.
//! \return Returns a pointer to the root scene node.
ISceneNode *CSceneManager::getRootSceneNode()
{
	return this;
}

//! Returns the current active camera.
//! \return The active camera is returned. Note that this can be NULL, if there
//! was no camera created yet.
ICameraSceneNode *CSceneManager::getActiveCamera() const
{
	return ActiveCamera;
}

//! Sets the active camera. The previous active camera will be deactivated.
//! \param camera: The new camera which should be active.
void CSceneManager::setActiveCamera(ICameraSceneNode *camera)
{
	if (camera)
		camera->grab();
	if (ActiveCamera)
		ActiveCamera->drop();

	ActiveCamera = camera;
}

//! renders the node.
void CSceneManager::render()
{
}

//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32> &CSceneManager::getBoundingBox() const
{
	assert(false); // Bounding Box of Scene Manager should never be used.

	static const core::aabbox3d<f32> dummy{{0.0f, 0.0f, 0.0f}};
	return dummy;
}

//! returns if node is culled
bool CSceneManager::isCulled(const ISceneNode *node) const
{
	const ICameraSceneNode *cam = getActiveCamera();
	if (!cam) {
		return false;
	}
	bool result = false;

	// has occlusion query information
	if (node->getAutomaticCulling() & scene::EAC_OCC_QUERY) {
		result = (Driver->getOcclusionQueryResult(const_cast<ISceneNode *>(node)) == 0);
	}

	// can be seen by a bounding box ?
	if (!result && (node->getAutomaticCulling() & scene::EAC_BOX)) {
		core::aabbox3d<f32> tbox = node->getBoundingBox();
		node->getAbsoluteTransformation().transformBoxEx(tbox);
		result = !(tbox.intersectsWithBox(cam->getViewFrustum()->getBoundingBox()));
	}

	// can be seen by a bounding sphere
	if (!result && (node->getAutomaticCulling() & scene::EAC_FRUSTUM_SPHERE)) {
		const core::aabbox3df nbox = node->getTransformedBoundingBox();
		const float rad = nbox.getRadius();
		const core::vector3df center = nbox.getCenter();

		const float camrad = cam->getViewFrustum()->getBoundingRadius();
		const core::vector3df camcenter = cam->getViewFrustum()->getBoundingCenter();

		const float dist = (center - camcenter).getLengthSQ();
		const float maxdist = (rad + camrad) * (rad + camrad);

		result = dist > maxdist;
	}

	// can be seen by cam pyramid planes ?
	if (!result && (node->getAutomaticCulling() & scene::EAC_FRUSTUM_BOX)) {
		SViewFrustum frust = *cam->getViewFrustum();

		// transform the frustum to the node's current absolute transformation
		core::matrix4 invTrans(node->getAbsoluteTransformation(), core::matrix4::EM4CONST_INVERSE);
		// invTrans.makeInverse();
		frust.transform(invTrans);

		core::vector3df edges[8];
		node->getBoundingBox().getEdges(edges);

		for (s32 i = 0; i < scene::SViewFrustum::VF_PLANE_COUNT; ++i) {
			bool boxInFrustum = false;
			for (u32 j = 0; j < 8; ++j) {
				if (frust.planes[i].classifyPointRelation(edges[j]) != core::ISREL3D_FRONT) {
					boxInFrustum = true;
					break;
				}
			}

			if (!boxInFrustum) {
				result = true;
				break;
			}
		}
	}

	return result;
}

//! registers a node for rendering it at a specific time.
u32 CSceneManager::registerNodeForRendering(ISceneNode *node, E_SCENE_NODE_RENDER_PASS pass)
{
	u32 taken = 0;

	switch (pass) {
		// take camera if it is not already registered
	case ESNRP_CAMERA: {
		if (std::find(CameraList.begin(), CameraList.end(), node) == CameraList.end()) {
			taken = 1;
			CameraList.push_back(node);
		}
	} break;
	case ESNRP_SKY_BOX:
		SkyBoxList.push_back(node);
		taken = 1;
		break;
	case ESNRP_SOLID:
		if (!isCulled(node)) {
			SolidNodeList.emplace_back(node);
			taken = 1;
		}
		break;
	case ESNRP_TRANSPARENT:
		if (!isCulled(node)) {
			TransparentNodeList.emplace_back(node, camWorldPos);
			taken = 1;
		}
		break;
	case ESNRP_TRANSPARENT_EFFECT:
		if (!isCulled(node)) {
			TransparentEffectNodeList.emplace_back(node, camWorldPos);
			taken = 1;
		}
		break;
	case ESNRP_AUTOMATIC:
		if (!isCulled(node)) {
			const u32 count = node->getMaterialCount();

			taken = 0;
			for (u32 i = 0; i < count; ++i) {
				if (Driver->needsTransparentRenderPass(node->getMaterial(i))) {
					// register as transparent node
					TransparentNodeList.emplace_back(node, camWorldPos);
					taken = 1;
					break;
				}
			}

			// not transparent, register as solid
			if (!taken) {
				SolidNodeList.emplace_back(node);
				taken = 1;
			}
		}
		break;
	case ESNRP_GUI:
		if (!isCulled(node)) {
			GuiNodeList.push_back(node);
			taken = 1;
		}

	case ESNRP_NONE: // ignore this one
		break;
	}

	return taken;
}

void CSceneManager::clearAllRegisteredNodesForRendering()
{
	CameraList.clear();
	SkyBoxList.clear();
	SolidNodeList.clear();
	TransparentNodeList.clear();
	TransparentEffectNodeList.clear();
	GuiNodeList.clear();
}

//! This method is called just before the rendering process of the whole scene.
//! draws all scene nodes
void CSceneManager::drawAll()
{
	if (!Driver)
		return;

	// reset all transforms
	Driver->setMaterial(video::SMaterial());
	Driver->setTransform(video::ETS_PROJECTION, core::IdentityMatrix);
	Driver->setTransform(video::ETS_VIEW, core::IdentityMatrix);
	Driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	for (u32 i = video::ETS_COUNT - 1; i >= video::ETS_TEXTURE_0; --i)
		Driver->setTransform((video::E_TRANSFORMATION_STATE)i, core::IdentityMatrix);
	Driver->setAllowZWriteOnTransparent(true);

	// do animations and other stuff.
	OnAnimate(os::Timer::getTime());

	/*!
		First Scene Node for prerendering should be the active camera
		consistent Camera is needed for culling
	*/
	camWorldPos.set(0, 0, 0);
	if (ActiveCamera) {
		ActiveCamera->render();
		camWorldPos = ActiveCamera->getAbsolutePosition();
	}

	// let all nodes register themselves
	OnRegisterSceneNode();

	const auto &render_node = [this] (ISceneNode *node) {
		u32 flags = node->isDebugDataVisible();
		node->setDebugDataVisible((flags & DebugDataMask) | DebugDataBits);
		node->render();
	};

	// render camera scenes
	{
		CurrentRenderPass = ESNRP_CAMERA;
		Driver->getOverrideMaterial().Enabled = ((Driver->getOverrideMaterial().EnablePasses & CurrentRenderPass) != 0);

		for (auto *node : CameraList)
			render_node(node);

		CameraList.clear();
	}

	// render skyboxes
	{
		CurrentRenderPass = ESNRP_SKY_BOX;
		Driver->getOverrideMaterial().Enabled = ((Driver->getOverrideMaterial().EnablePasses & CurrentRenderPass) != 0);

		for (auto *node : SkyBoxList)
			render_node(node);

		SkyBoxList.clear();
	}

	// render default objects
	{
		CurrentRenderPass = ESNRP_SOLID;
		Driver->getOverrideMaterial().Enabled = ((Driver->getOverrideMaterial().EnablePasses & CurrentRenderPass) != 0);

		std::sort(SolidNodeList.begin(), SolidNodeList.end());

		for (auto &it : SolidNodeList)
			render_node(it.Node);

		SolidNodeList.clear();
	}

	// render transparent objects.
	{
		CurrentRenderPass = ESNRP_TRANSPARENT;
		Driver->getOverrideMaterial().Enabled = ((Driver->getOverrideMaterial().EnablePasses & CurrentRenderPass) != 0);

		std::sort(TransparentNodeList.begin(), TransparentNodeList.end());

		for (auto &it : TransparentNodeList)
			render_node(it.Node);

		TransparentNodeList.clear();
	}

	// render transparent effect objects.
	{
		CurrentRenderPass = ESNRP_TRANSPARENT_EFFECT;
		Driver->getOverrideMaterial().Enabled = ((Driver->getOverrideMaterial().EnablePasses & CurrentRenderPass) != 0);

		std::sort(TransparentEffectNodeList.begin(), TransparentEffectNodeList.end());

		for (auto &it : TransparentEffectNodeList)
			render_node(it.Node);

		TransparentEffectNodeList.clear();
	}

	// render custom gui nodes
	{
		CurrentRenderPass = ESNRP_GUI;
		Driver->getOverrideMaterial().Enabled = ((Driver->getOverrideMaterial().EnablePasses & CurrentRenderPass) != 0);

		for (auto *node : GuiNodeList)
			render_node(node);

		GuiNodeList.clear();
	}
	clearDeletionList();

	CurrentRenderPass = ESNRP_NONE;
}

//! Adds an external mesh loader.
void CSceneManager::addExternalMeshLoader(IMeshLoader *externalLoader)
{
	if (!externalLoader)
		return;

	externalLoader->grab();
	MeshLoaderList.push_back(externalLoader);
}

//! Returns the number of mesh loaders supported by Irrlicht at this time
u32 CSceneManager::getMeshLoaderCount() const
{
	return static_cast<u32>(MeshLoaderList.size());
}

//! Retrieve the given mesh loader
IMeshLoader *CSceneManager::getMeshLoader(u32 index) const
{
	if (index < MeshLoaderList.size())
		return MeshLoaderList[index];
	else
		return 0;
}

//! Returns a pointer to the scene collision manager.
ISceneCollisionManager *CSceneManager::getSceneCollisionManager()
{
	return CollisionManager;
}

//! Returns a pointer to the mesh manipulator.
IMeshManipulator *CSceneManager::getMeshManipulator()
{
	return Driver->getMeshManipulator();
}

//! Adds a scene node to the deletion queue.
void CSceneManager::addToDeletionQueue(ISceneNode *node)
{
	if (!node)
		return;

	node->grab();
	DeletionList.push_back(node);
}

//! clears the deletion list
void CSceneManager::clearDeletionList()
{
	for (auto *node : DeletionList) {
		node->remove();
		node->drop();
	}

	DeletionList.clear();
}

//! Returns the first scene node with the specified name.
ISceneNode *CSceneManager::getSceneNodeFromName(const char *name, ISceneNode *start)
{
	if (start == 0)
		start = getRootSceneNode();

	auto startName = start->getName();
	if (startName.has_value() && startName == name)
		return start;

	ISceneNode *node = 0;

	const ISceneNodeList &list = start->getChildren();
	ISceneNodeList::const_iterator it = list.begin();
	for (; it != list.end(); ++it) {
		node = getSceneNodeFromName(name, *it);
		if (node)
			return node;
	}

	return 0;
}

//! Returns the first scene node with the specified id.
ISceneNode *CSceneManager::getSceneNodeFromId(s32 id, ISceneNode *start)
{
	if (start == 0)
		start = getRootSceneNode();

	if (start->getID() == id)
		return start;

	ISceneNode *node = 0;

	const ISceneNodeList &list = start->getChildren();
	ISceneNodeList::const_iterator it = list.begin();
	for (; it != list.end(); ++it) {
		node = getSceneNodeFromId(id, *it);
		if (node)
			return node;
	}

	return 0;
}

//! Returns the first scene node with the specified type.
ISceneNode *CSceneManager::getSceneNodeFromType(scene::ESCENE_NODE_TYPE type, ISceneNode *start)
{
	if (start == 0)
		start = getRootSceneNode();

	if (start->getType() == type || ESNT_ANY == type)
		return start;

	ISceneNode *node = 0;

	const ISceneNodeList &list = start->getChildren();
	ISceneNodeList::const_iterator it = list.begin();
	for (; it != list.end(); ++it) {
		node = getSceneNodeFromType(type, *it);
		if (node)
			return node;
	}

	return 0;
}

//! returns scene nodes by type.
void CSceneManager::getSceneNodesFromType(ESCENE_NODE_TYPE type, core::array<scene::ISceneNode *> &outNodes, ISceneNode *start)
{
	if (start == 0)
		start = getRootSceneNode();

	if (start->getType() == type || ESNT_ANY == type)
		outNodes.push_back(start);

	const ISceneNodeList &list = start->getChildren();
	ISceneNodeList::const_iterator it = list.begin();

	for (; it != list.end(); ++it) {
		getSceneNodesFromType(type, outNodes, *it);
	}
}

//! Posts an input event to the environment. Usually you do not have to
//! use this method, it is used by the internal engine.
bool CSceneManager::postEventFromUser(const SEvent &event)
{
	bool ret = false;
	ICameraSceneNode *cam = getActiveCamera();
	if (cam)
		ret = cam->OnEvent(event);

	return ret;
}

//! Removes all children of this scene node
void CSceneManager::removeAll()
{
	ISceneNode::removeAll();
	setActiveCamera(0);
	// Make sure the driver is reset, might need a more complex method at some point
	if (Driver)
		Driver->setMaterial(video::SMaterial());
}

//! Clears the whole scene. All scene nodes are removed.
void CSceneManager::clear()
{
	removeAll();
}

//! Returns current render pass.
E_SCENE_NODE_RENDER_PASS CSceneManager::getSceneNodeRenderPass() const
{
	return CurrentRenderPass;
}

//! Returns an interface to the mesh cache which is shared between all existing scene managers.
IMeshCache *CSceneManager::getMeshCache()
{
	return MeshCache;
}

//! Creates a new scene manager.
ISceneManager *CSceneManager::createNewSceneManager(bool cloneContent)
{
	CSceneManager *manager = new CSceneManager(Driver, CursorControl, MeshCache);

	if (cloneContent)
		manager->cloneMembers(this, manager);

	return manager;
}

//! Get a skinned mesh, which is not available as header-only code
SkinnedMesh *CSceneManager::createSkinnedMesh()
{
	return new SkinnedMesh(SkinnedMesh::SourceFormat::OTHER);
}

// creates a scenemanager
ISceneManager *createSceneManager(video::IVideoDriver *driver, gui::ICursorControl *cursorcontrol)
{
	return new CSceneManager(driver, cursorcontrol, nullptr);
}

} // end namespace scene
