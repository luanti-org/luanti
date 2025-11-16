// Copyright (C) 2023 Vitaliy Lobachevskiy
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "DriverGL3.h"
#include <cassert>
#include <stdexcept>
#include "mt_opengl.h"

namespace video
{

E_DRIVER_TYPE COpenGL3Driver::getDriverType() const
{
	return EDT_OPENGL3;
}

OpenGLVersion COpenGL3Driver::getVersionFromOpenGL() const
{
	GLint major = 0, minor = 0, profile = 0;
	GL.GetIntegerv(GL_MAJOR_VERSION, &major);
	GL.GetIntegerv(GL_MINOR_VERSION, &minor);
	GL.GetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
	// The spec is clear a context can’t be both core and compatibility at the same time.
	// However, the returned value is a mask. Ask Khronos why. -- numzero
	if (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
		return {OpenGLSpec::Compat, (u8)major, (u8)minor, 0};
	if (profile & GL_CONTEXT_CORE_PROFILE_BIT)
		return {OpenGLSpec::Core, (u8)major, (u8)minor, 0};
	os::Printer::log("Got unrecognized OpenGL profile", ELL_ERROR);
	return {OpenGLSpec::Core, (u8)major, (u8)minor, 0};
}

void COpenGL3Driver::initFeatures()
{
	if (Version.Spec != OpenGLSpec::Compat) {
		auto msg = "OpenGL 3 driver requires Compatibility context";
		os::Printer::log(msg, ELL_ERROR);
		throw std::runtime_error(msg);
	}
	if (!isVersionAtLeast(3, 2)) {
		auto msg = "OpenGL 3 driver requires OpenGL >= 3.2";
		os::Printer::log(msg, ELL_ERROR);
		throw std::runtime_error(msg);
	}
	initExtensions();

	TextureFormats[ECF_A1R5G5B5] = {GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV}; // WARNING: may not be renderable
	TextureFormats[ECF_R5G6B5] = {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5};              // GL_RGB565 is an extension until 4.1
	TextureFormats[ECF_R8G8B8] = {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE};                    // WARNING: may not be renderable
	TextureFormats[ECF_A8R8G8B8] = {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE};
	TextureFormats[ECF_R16F] = {GL_R16F, GL_RED, GL_HALF_FLOAT};
	TextureFormats[ECF_G16R16F] = {GL_RG16F, GL_RG, GL_HALF_FLOAT};
	TextureFormats[ECF_A16B16G16R16F] = {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
	TextureFormats[ECF_R32F] = {GL_R32F, GL_RED, GL_FLOAT};
	TextureFormats[ECF_G32R32F] = {GL_RG32F, GL_RG, GL_FLOAT};
	TextureFormats[ECF_A32B32G32R32F] = {GL_RGBA32F, GL_RGBA, GL_FLOAT};
	TextureFormats[ECF_R8] = {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
	TextureFormats[ECF_R8G8] = {GL_RG8, GL_RG, GL_UNSIGNED_BYTE};
	TextureFormats[ECF_R16] = {GL_R16, GL_RED, GL_UNSIGNED_SHORT};
	TextureFormats[ECF_R16G16] = {GL_RG16, GL_RG, GL_UNSIGNED_SHORT};
	TextureFormats[ECF_A2R10G10B10] = {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV};
	TextureFormats[ECF_D16] = {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
	TextureFormats[ECF_D24] = {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT};
	TextureFormats[ECF_D32] = {GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT}; // WARNING: may not be renderable (?!)
	TextureFormats[ECF_D24S8] = {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};

	AnisotropicFilterSupported = isVersionAtLeast(4, 6) || queryExtension("GL_ARB_texture_filter_anisotropic") || queryExtension("GL_EXT_texture_filter_anisotropic");
	LODBiasSupported = true;
	BlendMinMaxSupported = true;
	TextureMultisampleSupported = true;
	Texture2DArraySupported = Version.Major >= 3 || queryExtension("GL_EXT_texture_array");
	KHRDebugSupported = isVersionAtLeast(4, 6) || queryExtension("GL_KHR_debug");
	if (KHRDebugSupported)
		MaxLabelLength = GetInteger(GL.MAX_LABEL_LENGTH);

	// COGLESCoreExtensionHandler::Feature
	static_assert(MATERIAL_MAX_TEXTURES <= 16, "Only up to 16 textures are guaranteed");
	Feature.BlendOperation = true;
	Feature.TexStorage = isVersionAtLeast(4, 2) || queryExtension("GL_ARB_texture_storage");
	Feature.ColorAttachment = GetInteger(GL_MAX_COLOR_ATTACHMENTS);
	Feature.MaxTextureUnits = MATERIAL_MAX_TEXTURES;
	Feature.MultipleRenderTarget = GetInteger(GL_MAX_DRAW_BUFFERS);

	// COGLESCoreExtensionHandler
	if (AnisotropicFilterSupported)
		MaxAnisotropy = GetInteger(GL.MAX_TEXTURE_MAX_ANISOTROPY);
	MaxIndices = GetInteger(GL_MAX_ELEMENTS_INDICES);
	MaxTextureSize = GetInteger(GL_MAX_TEXTURE_SIZE);
	if (Texture2DArraySupported)
		MaxArrayTextureLayers = GetInteger(GL_MAX_ARRAY_TEXTURE_LAYERS);
	GL.GetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &MaxTextureLODBias);
	GL.GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, DimAliasedLine);
	DimAliasedPoint[0] = 1.0f;
	DimAliasedPoint[1] = 1.0f;
}

IVideoDriver *createOpenGL3Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager)
{
	os::Printer::log("Using COpenGL3Driver", ELL_INFORMATION);
	COpenGL3Driver *driver = new COpenGL3Driver(params, io, contextManager);
	driver->genericDriverInit(params.WindowSize, params.Stencilbuffer); // don't call in constructor, it uses virtual function calls of driver
	return driver;
}

}
