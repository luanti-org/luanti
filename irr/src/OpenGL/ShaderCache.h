// Copyright (C) 2026 VaiTon
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "Common.h"
#include "IFileSystem.h"
#include "path.h"

#include <optional>
#include <string_view>
#include <array>

namespace video
{

class COpenGL3DriverBase;

/// Binary shader cache for OpenGL shader programs.
///
/// This class allows caching compiled shader programs to disk, so that they can
/// be loaded quickly on subsequent runs without needing to recompile the
/// shaders.
///
/// This class is not thread-safe. If multiple threads need to access the shader
/// cache, external synchronization is required.
///
/// The cache key is based on a hash of the shader source code and the OpenGL
/// context (GL_VENDOR, GL_RENDERER, GL_VERSION, GL_SHADING_LANGUAGE_VERSION).
class COpenGL3ShaderCache
{
public:
	COpenGL3ShaderCache(COpenGL3DriverBase *driver, io::IFileSystem *fs, const io::path &cache_dir) :
		m_driver(driver), m_fs(fs), m_cache_dir(cache_dir) {}

	/// Loads a shader program from the cache.
	/// Returns the program ID if successful, std::nullopt otherwise.
	std::optional<GLuint> load(std::string_view vertexShaderProgram,
		std::string_view pixelShaderProgram) const;

	/// Saves a shader program to the cache.
	/// Returns true if successful, false otherwise.
	bool save(std::string_view vertexShaderProgram, std::string_view pixelShaderProgram,
		GLuint program) const;

private:
	typedef std::array<std::byte, 32> SHA256Hash;

	/// Gets the cache filename for a given hash.
	io::path getCacheFilename(const SHA256Hash &hash) const;

	/// Calculates a hash for the given shader programs and the current
	/// OpenGL context.
	static SHA256Hash calculateHash(std::string_view vertexShaderProgram,
		std::string_view pixelShaderProgram);

	COpenGL3DriverBase *m_driver;
	io::IFileSystem *m_fs;
	io::path m_cache_dir;
};

} // end namespace video
