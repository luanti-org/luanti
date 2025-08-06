// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

namespace scene
{
class ICameraSceneNode;
class IMeshBuffer;

//! A billboard scene node.
/** A billboard is like a 3d sprite: A 2d element,
which always looks to the camera. It is usually used for explosions, fire,
lensflares, particles and things like that.
*/
class IBillboardSceneNode : public ISceneNode
{
public:
	//! Constructor
	IBillboardSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id,
			const core::vector3df &position = core::vector3df(0, 0, 0)) :
			ISceneNode(parent, mgr, id, position) {}

	//! Sets the size of the billboard, making it rectangular.
	virtual void setSize(const core::dimension2d<f32> &size) = 0;

	//! Sets the size of the billboard with independent widths of the bottom and top edges.
	/** \param[in] height The height of the billboard.
	\param[in] bottomEdgeWidth The width of the bottom edge of the billboard.
	\param[in] topEdgeWidth The width of the top edge of the billboard.
	*/
	virtual void setSize(f32 height, f32 bottomEdgeWidth, f32 topEdgeWidth) = 0;

	//! Returns the size of the billboard.
	/** This will return the width of the bottom edge of the billboard.
	Use getWidths() to retrieve the bottom and top edges independently.
	\return Size of the billboard.
	*/
	virtual const core::dimension2d<f32> &getSize() const = 0;

	//! Gets the size of the the billboard and handles independent top and bottom edge widths correctly.
	/** \param[out] height The height of the billboard.
	\param[out] bottomEdgeWidth The width of the bottom edge of the billboard.
	\param[out] topEdgeWidth The width of the top edge of the billboard.
	*/
	virtual void getSize(f32 &height, f32 &bottomEdgeWidth, f32 &topEdgeWidth) const = 0;

	//! Set the color of all vertices of the billboard
	/** \param[in] overallColor Color to set */
	virtual void setColor(const video::SColor &overallColor) = 0;

	//! Set the color of the top and bottom vertices of the billboard
	/** \param[in] topColor Color to set the top vertices
	\param[in] bottomColor Color to set the bottom vertices */
	virtual void setColor(const video::SColor &topColor,
			const video::SColor &bottomColor) = 0;

	//! Gets the color of the top and bottom vertices of the billboard
	/** \param[out] topColor Stores the color of the top vertices
	\param[out] bottomColor Stores the color of the bottom vertices */
	virtual void getColor(video::SColor &topColor,
			video::SColor &bottomColor) const = 0;

	//! Get the real boundingbox used by the billboard, which can depend on the active camera.
	/** The boundingbox returned will use absolute coordinates.
		The billboard orients itself toward the camera and some only update in render().
		So we don't know the real boundingboxes before that. Which would be too late for culling.
		That is why the usual getBoundingBox will return a "safe" boundingbox which is guaranteed
		to contain the billboard. While this function can return the real one. */
	virtual const core::aabbox3d<f32> &getTransformedBillboardBoundingBox(const scene::ICameraSceneNode *camera) = 0;

	//! Get the amount of mesh buffers.
	/** \return Amount of mesh buffers (IMeshBuffer) in this mesh. */
	virtual u32 getMeshBufferCount() const = 0;

	//! Get pointer to a mesh buffer.
	/** NOTE: Positions and normals of this meshbuffers are re-calculated before rendering.
	So this is mainly useful to access/modify the uv-coordinates.
	\param nr: Zero based index of the mesh buffer.
	\return Pointer to the mesh buffer or 0 if there is no such	mesh buffer. */
	virtual IMeshBuffer *getMeshBuffer(u32 nr) const = 0;
};

} // end namespace scene
