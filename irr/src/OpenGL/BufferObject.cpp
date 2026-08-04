// Copyright (C) 2024 sfan5
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "BufferObject.h"

#include <cassert>
#include <mt_opengl.h>

namespace video
{

void OGLBufferObject::upload(const void *data, size_t size, size_t offset,
		GLenum usage, bool mustShrink)
{
	assert(!(mustShrink && offset > 0)); // forbidden usage
	if (!m_name) {
		GL.GenBuffers(1, &m_name);
		if (!m_name)
			return;
	}

	GL.BindBuffer(m_target, m_name);

	// Every caller replaces the buffer's entire contents on every call
	// (offset is always 0 in practice), so always re-specify storage via
	// BufferData rather than BufferSubData into the existing allocation.
	// BufferSubData can force the driver to stall the CPU until the GPU is
	// done reading the old contents; BufferData (even at an unchanged size)
	// instead orphans the old storage and hands back a fresh allocation,
	// letting the GPU keep draining the old one asynchronously. This matters
	// most for buffers re-uploaded every frame or even multiple times per
	// frame (joint transforms, streamed/dynamic meshes, the Core-profile
	// client-array scratch buffers).
	assert(offset == 0);
	GL.BufferData(m_target, size, data, usage);
	m_size = size;

	GL.BindBuffer(m_target, 0);
}

void OGLBufferObject::destroy()
{
	if (m_name)
		GL.DeleteBuffers(1, &m_name);
	m_name = 0;
	m_size = 0;
}

}
