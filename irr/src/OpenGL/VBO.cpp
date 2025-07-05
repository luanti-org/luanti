// Copyright (C) 2024 sfan5
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "VBO.h"

#include <cassert>
#include <mt_opengl.h>

namespace video
{

void OpenGLVBO::upload(const void *data, size_t size, size_t offset,
		GLenum usage, bool mustShrink)
{
	bool newBuffer = false;
	assert(!(mustShrink && offset > 0)); // forbidden usage
	if (!m_name) {
		GL.GenBuffers(1, &m_name);
		if (!m_name)
			return;
		newBuffer = true;
	} else if (size > m_size || mustShrink) {
		newBuffer = size != m_size;
	}

	GL.BindBuffer(GL_ARRAY_BUFFER, m_name);

	if (newBuffer) {
		assert(offset == 0);
		GL.BufferData(GL_ARRAY_BUFFER, size, data, usage);
		m_size = size;
	} else {
		GL.BufferSubData(GL_ARRAY_BUFFER, offset, size, data);
	}

	GL.BindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLVBO::destroy()
{
	if (m_name)
		GL.DeleteBuffers(1, &m_name);
	m_name = 0;
	m_size = 0;
}

}
