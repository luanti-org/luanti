// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IBillboardSceneNode.h"
#include "CMeshBuffer.h"

namespace scene
{

//! Scene node which is a billboard. A billboard is like a 3d sprite: A 2d element,
//! which always looks to the camera.
class CBillboardSceneNode : virtual public IBillboardSceneNode
{
public:
	//! constructor
	CBillboardSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id,
			const core::vector3df &position, const core::dimension2d<f32> &size,
			video::SColor colorTop = video::SColor(0xFFFFFFFF),
			video::SColor colorBottom = video::SColor(0xFFFFFFFF));

	virtual ~CBillboardSceneNode();

	//! pre render event
	void OnRegisterSceneNode() override;

	//! render
	void render() override;

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override;

	//! sets the size of the billboard
	void setSize(const core::dimension2d<f32> &size) override;

	//! Sets the widths of the top and bottom edges of the billboard independently.
	void setSize(f32 height, f32 bottomEdgeWidth, f32 topEdgeWidth) override;

	//! gets the size of the billboard
	const core::dimension2d<f32> &getSize() const override;

	//! Gets the widths of the top and bottom edges of the billboard.
	void getSize(f32 &height, f32 &bottomEdgeWidth, f32 &topEdgeWidth) const override;

	video::SMaterial &getMaterial(u32 i) override;

	//! returns amount of materials used by this scene node.
	u32 getMaterialCount() const override;

	//! Set the color of all vertices of the billboard
	//! \param overallColor: the color to set
	void setColor(const video::SColor &overallColor) override;

	//! Set the color of the top and bottom vertices of the billboard
	//! \param topColor: the color to set the top vertices
	//! \param bottomColor: the color to set the bottom vertices
	virtual void setColor(const video::SColor &topColor,
			const video::SColor &bottomColor) override;

	//! Gets the color of the top and bottom vertices of the billboard
	//! \param[out] topColor: stores the color of the top vertices
	//! \param[out] bottomColor: stores the color of the bottom vertices
	virtual void getColor(video::SColor &topColor,
			video::SColor &bottomColor) const override;

	//! Get the real boundingbox used by the billboard (which depends on the active camera)
	const core::aabbox3d<f32> &getTransformedBillboardBoundingBox(const scene::ICameraSceneNode *camera) override;

	//! Get the amount of mesh buffers.
	u32 getMeshBufferCount() const override
	{
		return Buffer ? 1 : 0;
	}

	//! Get pointer to the mesh buffer.
	IMeshBuffer *getMeshBuffer(u32 nr) const override
	{
		if (nr == 0)
			return Buffer;
		return 0;
	}

	//! Returns type of the scene node
	ESCENE_NODE_TYPE getType() const override { return ESNT_BILLBOARD; }

	//! Creates a clone of this scene node and its children.
	ISceneNode *clone(ISceneNode *newParent = 0, ISceneManager *newManager = 0) override;

protected:
	void updateMesh(const scene::ICameraSceneNode *camera);

private:
	//! Size.Width is the bottom edge width
	core::dimension2d<f32> Size;
	f32 TopEdgeWidth;

	//! BoundingBox which is large enough to contain the billboard independent of the camera
	// TODO: BUG - still can be wrong with scaling < 1. Billboards should calculate relative coordinates for their mesh
	// and then use the node-scaling. But needs some work...
	/** Note that we can't use the real boundingbox for culling because at that point
		the camera which is used to calculate the billboard is not yet updated. So we only
		know the real boundingbox after rendering - which is too late for culling. */
	core::aabbox3d<f32> BBoxSafe{{0.0f, 0.0f, 0.0f}};

	scene::SMeshBuffer *Buffer;
};

} // end namespace scene
