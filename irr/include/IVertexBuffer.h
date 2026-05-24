// Copyright (C) 2008-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "S3DVertex.h"
#include "HWBuffer.h"

namespace scene
{

struct WeightBuffer;

class IVertexBuffer : public virtual IReferenceCounted, public HWBuffer
{
public:
	//! Get type of vertex data which is stored in this meshbuffer.
	/** \return Vertex type of this buffer. */
	virtual video::E_VERTEX_TYPE getType() const = 0;

	//! Get access to vertex data. The data is an array of vertices.
	/** Which vertex type is used can be determined by getVertexType().
	\return Pointer to array of vertices. */
	virtual const void *getData() const = 0;

	//! Get access to vertex data. The data is an array of vertices.
	/** Which vertex type is used can be determined by getVertexType().
	\return Pointer to array of vertices. */
	virtual void *getData() = 0;

	virtual u32 getCount() const = 0;

	//! returns position of vertex i
	virtual const core::vector3df &getPosition(u32 i) const = 0;

	//! returns position of vertex i
	virtual core::vector3df &getPosition(u32 i) = 0;

	//! returns normal of vertex i
	virtual const core::vector3df &getNormal(u32 i) const = 0;

	//! returns normal of vertex i
	virtual core::vector3df &getNormal(u32 i) = 0;

	//! returns texture coord of vertex i
	virtual const core::vector2df &getTCoords(u32 i) const = 0;

	//! returns texture coord of vertex i
	virtual core::vector2df &getTCoords(u32 i) = 0;

	//! Get weight buffer for upload to the GPU, if any
	virtual const WeightBuffer *getWeightBuffer() const = 0;
	//! Enable software skinning
	virtual void useSwSkinning() = 0;
};

} // end namespace scene
