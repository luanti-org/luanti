// Copyright (C) 2026 VaiTon
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "ShaderCache.h"
#include "SDL_opengl.h"
#include "mt_opengl.h"
#include "os.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace {

constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;
constexpr std::string_view kSeparator{"\0", 1};

/// FNV-1a 64-bit hash function
constexpr uint64_t fnv1a_64(std::string_view str, uint64_t hash) {
  for (unsigned char c : str) {
    hash = (hash ^ c) * kFnvPrime;
  }
  return hash;
};

/// Get a string from OpenGL and return it as a std::string_view
std::string_view gl_string(GLenum name) {
  auto s = GL.GetString(name);
  return s ? std::string_view(reinterpret_cast<const char *>(s))
           : std::string_view{};
}

} // namespace

namespace video {

uint64_t
COpenGL3ShaderCache::calculateHash(std::string_view vertexShaderProgram,
                                   std::string_view pixelShaderProgram) {

  auto vendor = gl_string(GL_VENDOR);
  auto renderer = gl_string(GL_RENDERER);
  auto version = gl_string(GL_VERSION);
  auto sl_version = gl_string(GL_SHADING_LANGUAGE_VERSION);

  uint64_t h = kFnvOffsetBasis;
  h = fnv1a_64(vertexShaderProgram, h);
  h = fnv1a_64(kSeparator, h);
  h = fnv1a_64(pixelShaderProgram, h);
  h = fnv1a_64(vendor, h);
  h = fnv1a_64(kSeparator, h);
  h = fnv1a_64(renderer, h);
  h = fnv1a_64(kSeparator, h);
  h = fnv1a_64(version, h);
  h = fnv1a_64(kSeparator, h);
  h = fnv1a_64(sl_version, h);

  return h;
}

std::optional<GLuint>
COpenGL3ShaderCache::load(std::string_view vertexShaderProgram,
                          std::string_view pixelShaderProgram) const {
  if (m_cache_dir.empty()) {
    return std::nullopt;
  }

  if (!GL.GetProgramBinary || !GL.ProgramBinary) {
    return std::nullopt;
  }

  // Check if the driver supports program binaries
  GLint numFormats = 0;
  GL.GetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numFormats);
  if (numFormats <= 0) {
    return std::nullopt; // Driver does not support program binaries
  }

  // Calculate the hash and the corresponding cache file path
  uint64_t h = calculateHash(vertexShaderProgram, pixelShaderProgram);
  std::filesystem::path cache_dir{m_cache_dir.c_str()};
  std::filesystem::path filepath = cache_dir / (std::to_string(h) + ".bin");

  if (!std::filesystem::exists(filepath)) {
    return std::nullopt; // Cache file does not exist
  }

  std::ifstream file{filepath, std::ios::binary};
  if (!file) {
    os::Printer::log("Failed to open shader cache file", filepath.string(),
                     ELL_WARNING);
    return std::nullopt;
  }

  GLenum format = 0;
  if (!file.read(reinterpret_cast<char *>(&format), sizeof(format))) {
    os::Printer::log("Failed to read shader cache format", filepath.string(),
                     ELL_WARNING);
    return std::nullopt;
  }

  auto size = std::filesystem::file_size(filepath) - sizeof(format);
  if (size <= 0) {
    os::Printer::log("Invalid shader cache file size", filepath.string(),
                     ELL_WARNING);
    return std::nullopt;
  }

  // Load the binary data in memory
  std::vector<char> binary(size);
  if (!file.read(binary.data(), size)) {
    os::Printer::log("Failed to read shader cache binary data",
                     filepath.string(), ELL_WARNING);
    return false;
  }
  file.close();

  // Create the program and load the binary data
  GLuint program = GL.CreateProgram();
  if (!program) {
    os::Printer::log("Failed to create shader program", ELL_WARNING);
    return false;
  }

  GLint linkStatus = 0;
  GL.ProgramBinary(program, format, binary.data(), size);
  GL.GetProgramiv(program, GL_LINK_STATUS, &linkStatus);
  if (linkStatus != GL_TRUE) {
    os::Printer::log("Failed to load shader program from cache",
                     filepath.string(), ELL_WARNING);
    GL.DeleteProgram(program);
    program = 0;
    // No need to keep an invalid cache file around
    std::filesystem::remove(filepath);
    return std::nullopt;
  }

  return std::make_optional(program);
}

bool COpenGL3ShaderCache::save(std::string_view vertexShaderProgram,
                               std::string_view pixelShaderProgram,
                               GLuint program) const {
  if (m_cache_dir.empty()) {
    return false;
  }

  if (!GL.GetProgramBinary || !GL.ProgramBinary) {
    return false;
  }

  GLint numFormats = 0;
  GL.GetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numFormats);
  if (numFormats <= 0) {
    // We don't log here because this can be expected on some platforms
    return false;
  }

  GLint binaryLength = 0;
  GL.GetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
  if (binaryLength <= 0) {
    os::Printer::log("Failed to get shader program binary length", ELL_WARNING);
    return false;
  }

  std::vector<char> binary(binaryLength);
  GLenum format = 0;
  GLsizei length = 0;
  GL.GetProgramBinary(program, binaryLength, &length, &format, binary.data());
  if (length <= 0) {
    os::Printer::log("Failed to retrieve shader program binary", ELL_WARNING);
    return false;
  }

  uint64_t h = calculateHash(vertexShaderProgram, pixelShaderProgram);
  std::filesystem::path cache_dir(m_cache_dir.c_str());
  std::filesystem::path filepath = cache_dir / (std::to_string(h) + ".bin");

  std::ofstream file(filepath, std::ios::binary);
  if (!file) {
    os::Printer::log("Failed to open shader cache file for writing",
                     filepath.string(), ELL_WARNING);
    return false;
  }

  file.write(reinterpret_cast<const char *>(&format), sizeof(format));
  file.write(binary.data(), length);
  file.close();
  return true;
}

std::string COpenGL3ShaderCache::getCacheFilename(uint64_t hash) const {
  std::filesystem::path cache_dir(m_cache_dir.c_str());
  return (cache_dir / (std::to_string(hash) + ".bin")).string();
}

} // end namespace video
