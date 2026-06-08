// Copyright (C) 2026 VaiTon
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "ShaderCache.h"
#include "Driver.h"

#include "CReadFile.h"
#include "CWriteFile.h"
#include "coreutil.h"
#include "mt_opengl.h"
#include "os.h"
#include "path.h"

#include "my_sha256.h"

#include <cstddef>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// Used when hashing multiple strings together to separate them
constexpr std::string_view HASH_KEY_SEPARATOR{"\0", 1};

/// Get a string from OpenGL and return it as a std::string_view
std::string_view gl_string(GLenum name)
{
	auto s = GL.GetString(name);
	return s ? std::string_view(reinterpret_cast<const char *>(s))
		: std::string_view{};
}


std::string to_hex(const std::array<std::byte, 32> &bytes)
{
	std::ostringstream oss;
	for (auto b : bytes)
		oss << std::hex << std::setw(2) << std::setfill('0')
			<< std::to_integer<unsigned>(b);
	return oss.str();
}

} // namespace

namespace video
{

COpenGL3ShaderCache::SHA256Hash COpenGL3ShaderCache::calculateHash(
		std::string_view vertexShaderProgram, std::string_view pixelShaderProgram)
{
	auto vendor     = gl_string(GL_VENDOR);
	auto renderer   = gl_string(GL_RENDERER);
	auto version    = gl_string(GL_VERSION);
	auto sl_version = gl_string(GL_SHADING_LANGUAGE_VERSION);

	SHA256_CTX sha_ctx;
	SHA256_Init(&sha_ctx);

	auto update_hash = [&sha_ctx](std::string_view data) {
		SHA256_Update(&sha_ctx, data.data(), data.size());
	};

	update_hash(vertexShaderProgram);
	update_hash(HASH_KEY_SEPARATOR);
	update_hash(pixelShaderProgram);
	update_hash(HASH_KEY_SEPARATOR);
	update_hash(vendor);
	update_hash(HASH_KEY_SEPARATOR);
	update_hash(renderer);
	update_hash(HASH_KEY_SEPARATOR);
	update_hash(version);
	update_hash(HASH_KEY_SEPARATOR);
	update_hash(sl_version);

	SHA256Hash hash;
	SHA256_Final(reinterpret_cast<unsigned char *>(hash.data()), &sha_ctx);

	return hash;
}

std::optional<GLuint> COpenGL3ShaderCache::load(
		std::string_view vertexShaderProgram, std::string_view pixelShaderProgram) const
{
	if (m_cache_dir.empty() || !m_driver->BinaryCacheSupported) {
		return std::nullopt;
	}

	// Calculate the hash and the corresponding cache file path
	auto hash = calculateHash(vertexShaderProgram, pixelShaderProgram);
	io::path filepath = getCacheFilename(hash);

	if (!m_fs->existFile(filepath)) {
		return std::nullopt; // Cache file does not exist
	}

	io::CReadFile reader(filepath.c_str());
	if (!reader.isOpen()) {
		os::Printer::log("Failed to open shader cache file", filepath.c_str(), ELL_WARNING);
		return std::nullopt;
	}

	GLenum file_format = 0;
	size_t file_size = reader.getSize();
	if (file_size <= sizeof(file_format)) {
		os::Printer::log("Invalid shader cache file size", filepath.c_str(), ELL_WARNING);
		// If the file is too small to contain the format, it's invalid
		m_fs->deleteFile(filepath);
		return std::nullopt;
	}

	size_t file_effective_size = file_size - sizeof(file_format);

	// Read the format from the file
	if (reader.read(&file_format, sizeof(file_format)) != sizeof(file_format)) {
		os::Printer::log("Failed to read shader cache format", filepath.c_str(), ELL_WARNING);
		return std::nullopt;
	}

	// Load the binary data in memory
	std::vector<std::byte> binary(file_effective_size);
	if (!reader.read(binary.data(), file_effective_size)) {
		os::Printer::log("Failed to read shader cache binary data", filepath.c_str(), ELL_WARNING);
		return std::nullopt;
	}

	// Create the program and load the binary data
	GLuint program = GL.CreateProgram();
	if (program == 0) {
		os::Printer::log("Failed to create shader program", ELL_WARNING);
		return std::nullopt;
	}

	// Load the binary data into the program
	GL.ProgramBinary(program, file_format, binary.data(), file_effective_size);
	if (m_driver->testGLError(__FILE__, __LINE__)) {
		os::Printer::log("Failed to load shader program binary", filepath.c_str(), ELL_WARNING);
		// If loading the binary fails, cache is invalid (at least for this driver)
		GL.DeleteProgram(program);
		m_fs->deleteFile(filepath);
		return std::nullopt;
	}

	GLint linkStatus = 0;
	GL.GetProgramiv(program, GL_LINK_STATUS, &linkStatus);
	if (m_driver->testGLError(__FILE__, __LINE__)) {
		os::Printer::log("Failed to get shader program link status", filepath.c_str(), ELL_WARNING);
		GL.DeleteProgram(program);
		return std::nullopt;
	}
	if (linkStatus != GL_TRUE) {
		os::Printer::log("Failed to load shader program from cache", filepath.c_str(), ELL_WARNING);
		GL.DeleteProgram(program);
		// If the program fails to link, the cache is invalid (at least for this driver)
		m_fs->deleteFile(filepath);
		return std::nullopt;
	}

	return std::make_optional(program);
}

bool COpenGL3ShaderCache::save(std::string_view vertexShaderProgram,
		std::string_view pixelShaderProgram, GLuint program) const
{
	if (m_cache_dir.empty() || !m_driver->BinaryCacheSupported) {
		return false;
	}

	GLint binaryLength = 0;
	GL.GetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
	if (m_driver->testGLError(__FILE__, __LINE__) || binaryLength <= 0) {
		os::Printer::log("Failed to get shader program binary length", ELL_WARNING);
		return false;
	}

	std::vector<char> binary(binaryLength);
	GLenum file_format = 0;
	GLsizei file_effective_size = 0;

	GL.GetProgramBinary(program, binaryLength, &file_effective_size, &file_format, binary.data());
	if (m_driver->testGLError(__FILE__, __LINE__)) {
		os::Printer::log("Failed to retrieve shader program binary", ELL_WARNING);
		return false;
	}
	if (file_effective_size <= 0) {
		os::Printer::log("Failed to retrieve shader program binary", ELL_WARNING);
		return false;
	}

	io::path filepath = getCacheFilename(calculateHash(vertexShaderProgram, pixelShaderProgram));

	if (!m_fs->existDirectory(m_cache_dir) && !m_fs->createDirectory(m_cache_dir)) {
		os::Printer::log("Failed to create shader cache directory", m_cache_dir.c_str(), ELL_WARNING);
		return false;
	}

	io::CWriteFile writer(filepath.c_str(), false);
	if (!writer.isOpen()) {
		os::Printer::log("Failed to open shader cache file for writing", filepath.c_str(), ELL_WARNING);
		return false;
	}

	if (writer.write(&file_format, sizeof(file_format)) != sizeof(file_format)) {
		os::Printer::log("Failed to write shader cache format", filepath.c_str(), ELL_WARNING);
		return false;
	}

	if (writer.write(binary.data(), file_effective_size) != static_cast<size_t>(file_effective_size)) {
		os::Printer::log("Failed to write shader cache binary data", filepath.c_str(), ELL_WARNING);
		return false;
	}

	if (!writer.flush()) {
		os::Printer::log("Failed to flush shader cache file", filepath.c_str(), ELL_WARNING);
		return false;
	}

	return true;
}

io::path COpenGL3ShaderCache::getCacheFilename(const SHA256Hash &hash) const
{
	return core::mergeFilename(m_cache_dir, to_hex(hash) + ".bin");
}

} // end namespace video
