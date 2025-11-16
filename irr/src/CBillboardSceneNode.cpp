// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CBillboardSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "ICameraSceneNode.h"
#include "os.h"

namespace scene
{

//! constructor
CBillboardSceneNode::CBillboardSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id,
		const core::vector3df &position, const core::dimension2d<f32> &size,
		video::SColor colorTop, video::SColor colorBottom) :
		IBillboardSceneNode(parent, mgr, id, position),
		Buffer(new SMeshBuffer())
{
	setSize(size);

	auto &Vertices = Buffer->Vertices->Data;
	auto &Indices = Buffer->Indices->Data;

	Vertices.resize(4);
	Indices.resize(6);

	Indices[0] = 0;
	Indices[1] = 2;
	Indices[2] = 1;
	Indices[3] = 0;
	Indices[4] = 3;
	Indices[5] = 2;

	Vertices[0].TCoords.set(1.0f, 1.0f);
	Vertices[0].Color = colorBottom;

	Vertices[1].TCoords.set(1.0f, 0.0f);
	Vertices[1].Color = colorTop;

	Vertices[2].TCoords.set(0.0f, 0.0f);
	Vertices[2].Color = colorTop;

	Vertices[3].TCoords.set(0.0f, 1.0f);
	Vertices[3].Color = colorBottom;
}

CBillboardSceneNode::~CBillboardSceneNode()
{
	Buffer->drop();
}

//! pre render event
void CBillboardSceneNode::OnRegisterSceneNode()
{
	if (IsVisible)
		SceneManager->registerNodeForRendering(this);

	ISceneNode::OnRegisterSceneNode();
}

//! render
void CBillboardSceneNode::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	ICameraSceneNode *camera = SceneManager->getActiveCamera();

	if (!camera || !driver)
		return;

	// make billboard look to camera
	updateMesh(camera);

	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->setMaterial(Buffer->Material);
	driver->drawMeshBuffer(Buffer);

	if (DebugDataVisible & scene::EDS_BBOX) {
		driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
		video::SMaterial m;
		driver->setMaterial(m);
		driver->draw3DBox(BBoxSafe, video::SColor(0, 208, 195, 152));
	}
}

void CBillboardSceneNode::updateMesh(const scene::ICameraSceneNode *camera)
{
	// billboard looks toward camera
	core::vector3df pos = getAbsolutePosition();

	core::vector3df campos = camera->getAbsolutePosition();
	core::vector3df target = camera->getTarget();
	core::vector3df up = camera->getUpVector();
	core::vector3df view = target - campos;
	view.normalize();

	core::vector3df horizontal = up.crossProduct(view);
	if (horizontal.getLength() == 0) {
		horizontal.set(up.Y, up.X, up.Z);
	}
	horizontal.normalize();
	core::vector3df topHorizontal = horizontal * 0.5f * TopEdgeWidth;
	horizontal *= 0.5f * Size.Width;

	// pointing down!
	core::vector3df vertical = horizontal.crossProduct(view);
	vertical.normalize();
	vertical *= 0.5f * Size.Height;

	view *= -1.0f;

	auto &vertices = Buffer->Vertices->Data;

	for (s32 i = 0; i < 4; ++i)
		vertices[i].Normal = view;

	/* Vertices are:
	2--1
	|\ |
	| \|
	3--0
	*/
	vertices[0].Pos = pos + horizontal + vertical;
	vertices[1].Pos = pos + topHorizontal - vertical;
	vertices[2].Pos = pos - topHorizontal - vertical;
	vertices[3].Pos = pos - horizontal + vertical;

	Buffer->setDirty(EBT_VERTEX);
	Buffer->recalculateBoundingBox();
}

//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32> &CBillboardSceneNode::getBoundingBox() const
{
	// Really wrong when scaled (as the node does not scale it's vertices - maybe it should?)
	return BBoxSafe;
}

const core::aabbox3d<f32> &CBillboardSceneNode::getTransformedBillboardBoundingBox(const scene::ICameraSceneNode *camera)
{
	updateMesh(camera);
	return Buffer->BoundingBox;
}

void CBillboardSceneNode::setSize(const core::dimension2d<f32> &size)
{
	Size = size;

	if (core::equals(Size.Width, 0.0f))
		Size.Width = 1.0f;
	TopEdgeWidth = Size.Width;

	if (core::equals(Size.Height, 0.0f))
		Size.Height = 1.0f;

	const f32 extent = 0.5f * sqrtf(Size.Width * Size.Width + Size.Height * Size.Height);
	BBoxSafe.MinEdge.set(-extent, -extent, -extent);
	BBoxSafe.MaxEdge.set(extent, extent, extent);
}

void CBillboardSceneNode::setSize(f32 height, f32 bottomEdgeWidth, f32 topEdgeWidth)
{
	Size.set(bottomEdgeWidth, height);
	TopEdgeWidth = topEdgeWidth;

	if (core::equals(Size.Height, 0.0f))
		Size.Height = 1.0f;

	if (core::equals(Size.Width, 0.f) && core::equals(TopEdgeWidth, 0.f)) {
		Size.Width = 1.0f;
		TopEdgeWidth = 1.0f;
	}

	const f32 extent = 0.5f * sqrtf(Size.Width * Size.Width + Size.Height * Size.Height);
	BBoxSafe.MinEdge.set(-extent, -extent, -extent);
	BBoxSafe.MaxEdge.set(extent, extent, extent);
}

video::SMaterial &CBillboardSceneNode::getMaterial(u32 i)
{
	return Buffer->Material;
}

//! returns amount of materials used by this scene node.
u32 CBillboardSceneNode::getMaterialCount() const
{
	return 1;
}

//! gets the size of the billboard
const core::dimension2d<f32> &CBillboardSceneNode::getSize() const
{
	return Size;
}

//! Gets the widths of the top and bottom edges of the billboard.
void CBillboardSceneNode::getSize(f32 &height, f32 &bottomEdgeWidth,
		f32 &topEdgeWidth) const
{
	height = Size.Height;
	bottomEdgeWidth = Size.Width;
	topEdgeWidth = TopEdgeWidth;
}

//! Set the color of all vertices of the billboard
//! \param overallColor: the color to set
void CBillboardSceneNode::setColor(const video::SColor &overallColor)
{
	auto &vertices = Buffer->Vertices->Data;
	for (u32 vertex = 0; vertex < 4; ++vertex)
		vertices[vertex].Color = overallColor;
}

//! Set the color of the top and bottom vertices of the billboard
//! \param topColor: the color to set the top vertices
//! \param bottomColor: the color to set the bottom vertices
void CBillboardSceneNode::setColor(const video::SColor &topColor,
		const video::SColor &bottomColor)
{
	auto &vertices = Buffer->Vertices->Data;
	vertices[0].Color = bottomColor;
	vertices[1].Color = topColor;
	vertices[2].Color = topColor;
	vertices[3].Color = bottomColor;
}

//! Gets the color of the top and bottom vertices of the billboard
//! \param[out] topColor: stores the color of the top vertices
//! \param[out] bottomColor: stores the color of the bottom vertices
void CBillboardSceneNode::getColor(video::SColor &topColor,
		video::SColor &bottomColor) const
{
	auto &vertices = Buffer->Vertices->Data;
	bottomColor = vertices[0].Color;
	topColor = vertices[1].Color;
}

//! Creates a clone of this scene node and its children.
ISceneNode *CBillboardSceneNode::clone(ISceneNode *newParent, ISceneManager *newManager)
{
	if (!newParent)
		newParent = Parent;
	if (!newManager)
		newManager = SceneManager;

	CBillboardSceneNode *nb = new CBillboardSceneNode(newParent,
			newManager, ID, RelativeTranslation, Size);

	nb->cloneMembers(this, newManager);
	nb->Buffer->Material = Buffer->Material;
	nb->Size = Size;
	nb->TopEdgeWidth = this->TopEdgeWidth;

	video::SColor topColor, bottomColor;
	getColor(topColor, bottomColor);
	nb->setColor(topColor, bottomColor);

	if (newParent)
		nb->drop();
	return nb;
}

} // end namespace scene
