// Copyright (C) 2024 sfan5
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "Common.h"
#include <cstddef>

namespace video
{

class OGLBufferObject
{
public:
	enum Target : GLenum {
		TARGET_VBO = GL_ARRAY_BUFFER,
		TARGET_UBO = GL_UNIFORM_BUFFER,
		TARGET_EBO = GL_ELEMENT_ARRAY_BUFFER,
	};

	/// @note does not create on GL side
	OGLBufferObject(Target target) : m_target(target) {}
	/// @note does not free on GL side
	~OGLBufferObject() {}
	// ^ Do not use `= default;`. These should be equivalent here, but `= default`
	// results in a linking error for obscure configurations
	// (Edison Design Group C++ frontend + MCST LCC 1.29.16 and similar), see #17232.

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
	 * @param orphan force re-specifying (orphaning) the GL storage via
	 *   BufferData instead of reusing the existing allocation via
	 *   BufferSubData, even if the size is unchanged. Only valid with
	 *   `offset == 0`. BufferSubData can make the driver stall the CPU
	 *   until the GPU is done reading the old contents; orphaning avoids
	 *   that at the cost of a fresh allocation. Only worth it for buffers
	 *   whose *entire* contents are replaced on every upload and that are
	 *   uploaded frequently (e.g. every frame or draw call) -- pass this
	 *   explicitly per call site, don't flip the default.
	 * @note modifies GL_ARRAY_BUFFER binding
	 */
	void upload(const void *data, size_t size, size_t offset,
		GLenum usage, bool mustShrink = false, bool orphan = false);

	/**
	 * Free buffer in GL.
	 * @note modifies GL_ARRAY_BUFFER binding
	 */
	void destroy();

private:

	GLuint m_name = 0;
	size_t m_size = 0;
	Target m_target;
};

}
