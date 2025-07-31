// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CMeshSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "IMeshCache.h"
#include "IMeshBuffer.h"
#include "IMaterialRenderer.h"
#include "IFileSystem.h"

namespace scene
{

//! constructor
CMeshSceneNode::CMeshSceneNode(IMesh *mesh, ISceneNode *parent, ISceneManager *mgr, s32 id,
		const core::vector3df &position, const core::vector3df &rotation,
		const core::vector3df &scale) :
		IMeshSceneNode(parent, mgr, id, position, rotation, scale),
		Mesh(0),
		PassCount(0), SharedMaterials(false)
{
	setMesh(mesh);
}

//! destructor
CMeshSceneNode::~CMeshSceneNode()
{
	if (Mesh)
		Mesh->drop();
}

//! frame
void CMeshSceneNode::OnRegisterSceneNode()
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
		const u32 numMaterials = SharedMaterials ? Mesh->getMeshBufferCount() : Materials.size();
		for (u32 i = 0; i < numMaterials; ++i) {
			const auto &material = SharedMaterials ? Mesh->getMeshBuffer(i)->getMaterial() : Materials[i];

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

//! renders the node.
void CMeshSceneNode::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();

	if (!Mesh || !driver)
		return;

	const bool isTransparentPass =
			SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT;

	++PassCount;

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	Box = Mesh->getBoundingBox();

	for (u32 i = 0; i < Mesh->getMeshBufferCount(); ++i) {
		scene::IMeshBuffer *mb = Mesh->getMeshBuffer(i);
		if (mb) {
			const auto &material = SharedMaterials ? mb->getMaterial() : Materials[i];

			const bool transparent = driver->needsTransparentRenderPass(material);

			// only render transparent buffer if this is the transparent render pass
			// and solid only in solid pass
			if (transparent == isTransparentPass) {
				driver->setMaterial(material);
				driver->drawMeshBuffer(mb);
			}
		}
	}

	// for debug purposes only:
	if (DebugDataVisible && PassCount == 1) {
		video::SMaterial m;
		m.AntiAliasing = video::EAAM_OFF;
		m.ZBuffer = video::ECFN_DISABLED;
		driver->setMaterial(m);

		if (DebugDataVisible & scene::EDS_BBOX_BUFFERS) {
			for (u32 g = 0; g < Mesh->getMeshBufferCount(); ++g) {
				driver->draw3DBox(
						Mesh->getMeshBuffer(g)->getBoundingBox(),
						video::SColor(255, 190, 128, 128));
			}
		}

		if (DebugDataVisible & scene::EDS_BBOX) {
			driver->draw3DBox(Box, video::SColor(255, 255, 255, 255));
		}

		if (DebugDataVisible & scene::EDS_NORMALS) {
			// draw normals
			const f32 debugNormalLength = 1.f;
			const video::SColor debugNormalColor = video::SColor(255, 34, 221, 221);
			const u32 count = Mesh->getMeshBufferCount();

			for (u32 i = 0; i != count; ++i) {
				driver->drawMeshBufferNormals(Mesh->getMeshBuffer(i), debugNormalLength, debugNormalColor);
			}
		}

		// show mesh
		if (DebugDataVisible & scene::EDS_MESH_WIRE_OVERLAY) {
			m.Wireframe = true;
			driver->setMaterial(m);

			for (u32 g = 0; g < Mesh->getMeshBufferCount(); ++g) {
				driver->drawMeshBuffer(Mesh->getMeshBuffer(g));
			}
		}
	}
}

//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
bool CMeshSceneNode::removeChild(ISceneNode *child)
{
	return ISceneNode::removeChild(child);
}

//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32> &CMeshSceneNode::getBoundingBox() const
{
	return Mesh ? Mesh->getBoundingBox() : Box;
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use getMaterialCount().
video::SMaterial &CMeshSceneNode::getMaterial(u32 i)
{
	if (Mesh && SharedMaterials && i < Mesh->getMeshBufferCount()) {
		return Mesh->getMeshBuffer(i)->getMaterial();
	}

	if (i >= Materials.size())
		return ISceneNode::getMaterial(i);

	return Materials[i];
}

//! returns amount of materials used by this scene node.
u32 CMeshSceneNode::getMaterialCount() const
{
	if (Mesh && SharedMaterials)
		return Mesh->getMeshBufferCount();

	return Materials.size();
}

//! Sets a new mesh
void CMeshSceneNode::setMesh(IMesh *mesh)
{
	if (mesh) {
		mesh->grab();
		if (Mesh)
			Mesh->drop();

		Mesh = mesh;
		copyMaterials();
	}
}

void CMeshSceneNode::copyMaterials()
{
	Materials.clear();

	if (Mesh && !SharedMaterials) {
		video::SMaterial mat;

		Materials.reserve(Mesh->getMeshBufferCount());
		for (u32 i = 0; i < Mesh->getMeshBufferCount(); ++i) {
			IMeshBuffer *mb = Mesh->getMeshBuffer(i);
			if (mb)
				mat = mb->getMaterial();

			Materials.push_back(mat);
		}
	}
}

//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
/* In this way it is possible to change the materials a mesh causing all mesh scene nodes
referencing this mesh to change too. */
void CMeshSceneNode::setSharedMaterials(bool shared)
{
	if (SharedMaterials != shared) {
		SharedMaterials = shared;
		copyMaterials();
	}
}

//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool CMeshSceneNode::isSharedMaterials() const
{
	return SharedMaterials;
}

//! Creates a clone of this scene node and its children.
ISceneNode *CMeshSceneNode::clone(ISceneNode *newParent, ISceneManager *newManager)
{
	if (!newParent)
		newParent = Parent;
	if (!newManager)
		newManager = SceneManager;

	CMeshSceneNode *nb = new CMeshSceneNode(Mesh, newParent,
			newManager, ID, RelativeTranslation, RelativeRotation, RelativeScale);

	nb->cloneMembers(this, newManager);
	nb->SharedMaterials = SharedMaterials;
	nb->Materials = Materials;

	if (newParent)
		nb->drop();
	return nb;
}

} // end namespace scene
