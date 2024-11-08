// Copyright (C) 2023 Vitaliy Lobachevskiy
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "Driver.h"
#include <cassert>
#include <CColorConverter.h>

namespace irr
{
namespace video
{

E_DRIVER_TYPE COpenGLES2Driver::getDriverType() const
{
	return EDT_OGLES2;
}

OpenGLVersion COpenGLES2Driver::getVersionFromOpenGL() const
{
	auto version_string = reinterpret_cast<const char *>(GL.GetString(GL_VERSION));
	int major, minor;
	if (sscanf(version_string, "OpenGL ES %d.%d", &major, &minor) != 2) {
		os::Printer::log("Failed to parse OpenGL ES version string", version_string, ELL_ERROR);
		return {OpenGLSpec::ES, 0, 0, 0};
	}
	return {OpenGLSpec::ES, (u8)major, (u8)minor, 0};
}

void COpenGLES2Driver::initFeatures()
{
	assert(Version.Spec == OpenGLSpec::ES);
	assert(Version.Major >= 2);
	initExtensions();

	static const GLenum BGRA8_EXT = 0x93A1;

	if (Version.Major >= 3) {
		// NOTE floating-point formats may not be suitable for render targets.
		TextureFormats[ECF_A1R5G5B5] = {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, CColorConverter::convert_A1R5G5B5toR5G5B5A1};
		TextureFormats[ECF_R5G6B5] = {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5};
		TextureFormats[ECF_R8G8B8] = {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE};
		TextureFormats[ECF_A8R8G8B8] = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, CColorConverter::convert_A8R8G8B8toA8B8G8R8};
		TextureFormats[ECF_R16F] = {GL_R16F, GL_RED, GL_HALF_FLOAT};
		TextureFormats[ECF_G16R16F] = {GL_RG16F, GL_RG, GL_HALF_FLOAT};
		TextureFormats[ECF_A16B16G16R16F] = {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
		TextureFormats[ECF_R32F] = {GL_R32F, GL_RED, GL_FLOAT};
		TextureFormats[ECF_G32R32F] = {GL_RG32F, GL_RG, GL_FLOAT};
		TextureFormats[ECF_A32B32G32R32F] = {GL_RGBA32F, GL_RGBA, GL_FLOAT};
		TextureFormats[ECF_R8] = {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
		TextureFormats[ECF_R8G8] = {GL_RG8, GL_RG, GL_UNSIGNED_BYTE};
		TextureFormats[ECF_D16] = {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
		TextureFormats[ECF_D24S8] = {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};

		if (FeatureAvailable[IRR_GL_EXT_texture_format_BGRA8888])
			TextureFormats[ECF_A8R8G8B8] = {GL_BGRA, GL_BGRA, GL_UNSIGNED_BYTE};
		else if (FeatureAvailable[IRR_GL_APPLE_texture_format_BGRA8888])
			TextureFormats[ECF_A8R8G8B8] = {BGRA8_EXT, GL_BGRA, GL_UNSIGNED_BYTE};

		// OpenGL ES 3 doesn't include a GL_DEPTH_COMPONENT32, so still use
		// OES_depth_texture for 32-bit depth texture support.
		// OpenGL ES 3 would allow {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT},
		// but I guess that would have to be called ECF_D32F...
		if (FeatureAvailable[IRR_GL_OES_depth_texture])
			TextureFormats[ECF_D32] = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT};
	} else {
		// NOTE These are *texture* formats. They may or may not be suitable
		// for render targets. The specs only talks on *sized* formats for the
		// latter but forbids creating textures with sized internal formats,
		// reserving them for renderbuffers.

		static const GLenum HALF_FLOAT_OES = 0x8D61; // not equal to GL_HALF_FLOAT
		TextureFormats[ECF_A1R5G5B5] = {GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, CColorConverter::convert_A1R5G5B5toR5G5B5A1};
		TextureFormats[ECF_R5G6B5] = {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5};
		TextureFormats[ECF_R8G8B8] = {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE};
		TextureFormats[ECF_A8R8G8B8] = {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, CColorConverter::convert_A8R8G8B8toA8B8G8R8};

		if (FeatureAvailable[IRR_GL_EXT_texture_format_BGRA8888])
			TextureFormats[ECF_A8R8G8B8] = {GL_BGRA, GL_BGRA, GL_UNSIGNED_BYTE};
		else if (FeatureAvailable[IRR_GL_APPLE_texture_format_BGRA8888])
			TextureFormats[ECF_A8R8G8B8] = {BGRA8_EXT, GL_BGRA, GL_UNSIGNED_BYTE};

		if (FeatureAvailable[IRR_GL_OES_texture_half_float]) {
			TextureFormats[ECF_A16B16G16R16F] = {GL_RGBA, GL_RGBA, HALF_FLOAT_OES};
		}
		if (FeatureAvailable[IRR_GL_OES_texture_float]) {
			TextureFormats[ECF_A32B32G32R32F] = {GL_RGBA, GL_RGBA, GL_FLOAT};
		}
		if (FeatureAvailable[IRR_GL_EXT_texture_rg]) {
			TextureFormats[ECF_R8] = {GL_RED, GL_RED, GL_UNSIGNED_BYTE};
			TextureFormats[ECF_R8G8] = {GL_RG, GL_RG, GL_UNSIGNED_BYTE};

			if (FeatureAvailable[IRR_GL_OES_texture_half_float]) {
				TextureFormats[ECF_R16F] = {GL_RED, GL_RED, HALF_FLOAT_OES};
				TextureFormats[ECF_G16R16F] = {GL_RG, GL_RG, HALF_FLOAT_OES};
			}
			if (FeatureAvailable[IRR_GL_OES_texture_float]) {
				TextureFormats[ECF_R32F] = {GL_RED, GL_RED, GL_FLOAT};
				TextureFormats[ECF_G32R32F] = {GL_RG, GL_RG, GL_FLOAT};
			}
		}

		if (FeatureAvailable[IRR_GL_OES_depth_texture]) {
			TextureFormats[ECF_D16] = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
			// OES_depth_texture includes 32-bit depth texture support.
			TextureFormats[ECF_D32] = {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT};

			if (FeatureAvailable[IRR_GL_OES_packed_depth_stencil])
				TextureFormats[ECF_D24S8] = {GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
		}
	}

	const bool MRTSupported = Version.Major >= 3 || queryExtension("GL_EXT_draw_buffers");
	AnisotropicFilterSupported = queryExtension("GL_EXT_texture_filter_anisotropic");
	BlendMinMaxSupported = (Version.Major >= 3) || FeatureAvailable[IRR_GL_EXT_blend_minmax];
	const bool TextureLODBiasSupported = queryExtension("GL_EXT_texture_lod_bias");

	// COGLESCoreExtensionHandler::Feature
	static_assert(MATERIAL_MAX_TEXTURES <= 8, "Only up to 8 textures are guaranteed");
	Feature.BlendOperation = true;
	Feature.ColorAttachment = 1;
	if (MRTSupported)
		Feature.ColorAttachment = GetInteger(GL_MAX_COLOR_ATTACHMENTS);
	Feature.MaxTextureUnits = MATERIAL_MAX_TEXTURES;
	if (MRTSupported)
		Feature.MultipleRenderTarget = GetInteger(GL_MAX_DRAW_BUFFERS);

	// COGLESCoreExtensionHandler
	if (AnisotropicFilterSupported)
		MaxAnisotropy = GetInteger(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
	if (Version.Major >= 3 || queryExtension("GL_EXT_draw_range_elements"))
		MaxIndices = GetInteger(GL_MAX_ELEMENTS_INDICES);
	MaxTextureSize = GetInteger(GL_MAX_TEXTURE_SIZE);
	if (TextureLODBiasSupported)
		GL.GetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &MaxTextureLODBias);
	GL.GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, DimAliasedLine); // NOTE: this is not in the OpenGL ES 2.0 spec...
	GL.GetFloatv(GL_ALIASED_POINT_SIZE_RANGE, DimAliasedPoint);
}

IVideoDriver *createOGLES2Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager)
{
	os::Printer::log("Using COpenGLES2Driver", ELL_INFORMATION);
	COpenGLES2Driver *driver = new COpenGLES2Driver(params, io, contextManager);
	driver->genericDriverInit(params.WindowSize, params.Stencilbuffer); // don't call in constructor, it uses virtual function calls of driver
	return driver;
}

}
}
