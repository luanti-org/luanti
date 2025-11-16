// Copyright (C) 2024 sfan5
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "Common.h"
#include <cstddef>

namespace video
{

class OpenGLVBO
{
public:
	/// @note does not create on GL side
	OpenGLVBO() = default;
	/// @note does not free on GL side
	~OpenGLVBO() = default;

	/// @return "name" (ID) of this buffer in GL
	GLuint getName() const { return m_name; }
	/// @return does this refer to an existing GL buffer?
	bool exists() const { return m_name != 0; }

	/// @return size of this buffer in bytes
	size_t getSize() const { return m_size; }

	/**
	 * Upload buffer data to GL.
	 *
	 * Changing the size of the buffer is only possible when `offset == 0`.
	 * @param data data pointer
	 * @param size number of bytes
	 * @param offset offset to upload at
	 * @param usage usage pattern passed to GL (only if buffer is new)
	 * @param mustShrink force re-create of buffer if it became smaller
	 * @note modifies GL_ARRAY_BUFFER binding
	 */
	void upload(const void *data, size_t size, size_t offset,
		GLenum usage, bool mustShrink = false);

	/**
	 * Free buffer in GL.
	 * @note modifies GL_ARRAY_BUFFER binding
	 */
	void destroy();

private:
	GLuint m_name = 0;
	size_t m_size = 0;
};

}
