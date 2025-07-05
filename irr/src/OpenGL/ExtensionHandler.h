// Copyright (C) 2023 Vitaliy Lobachevskiy
// Copyright (C) 2015 Patryk Nadrowski
// Copyright (C) 2009-2010 Amundis
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#include <unordered_set>

#include "EDriverFeatures.h"
#include "irrTypes.h"
#include "os.h"

#include "Common.h"
#include <mt_opengl.h> // must be after Common.h

#include "COGLESCoreExtensionHandler.h"

namespace video
{

class COpenGL3ExtensionHandler : public COGLESCoreExtensionHandler
{
public:
	COpenGL3ExtensionHandler() :
			COGLESCoreExtensionHandler() {}

	void initExtensions();

	/// Checks whether a named extension is present
	inline bool queryExtension(const std::string &name) const noexcept
	{
		return GL.IsExtensionPresent(name);
	}

	bool queryFeature(video::E_VIDEO_DRIVER_FEATURE feature) const
	{
		switch (feature) {
		case EVDF_RENDER_TO_TARGET:
		case EVDF_HARDWARE_TL:
		case EVDF_MULTITEXTURE:
		case EVDF_BILINEAR_FILTER:
		case EVDF_MIP_MAP:
		case EVDF_MIP_MAP_AUTO_UPDATE:
		case EVDF_VERTEX_SHADER_1_1:
		case EVDF_PIXEL_SHADER_1_1:
		case EVDF_PIXEL_SHADER_1_2:
		case EVDF_PIXEL_SHADER_2_0:
		case EVDF_VERTEX_SHADER_2_0:
		case EVDF_ARB_GLSL:
		case EVDF_TEXTURE_NSQUARE:
		case EVDF_TEXTURE_NPOT:
		case EVDF_FRAMEBUFFER_OBJECT:
		case EVDF_VERTEX_BUFFER_OBJECT:
		case EVDF_COLOR_MASK:
		case EVDF_ALPHA_TO_COVERAGE:
		case EVDF_POLYGON_OFFSET:
		case EVDF_BLEND_OPERATIONS:
		case EVDF_BLEND_SEPARATE:
		case EVDF_TEXTURE_MATRIX:
		case EVDF_TEXTURE_CUBEMAP:
			return true;
		case EVDF_ARB_VERTEX_PROGRAM_1:
		case EVDF_ARB_FRAGMENT_PROGRAM_1:
		case EVDF_GEOMETRY_SHADER:
		case EVDF_MULTIPLE_RENDER_TARGETS:
		case EVDF_MRT_BLEND:
		case EVDF_MRT_COLOR_MASK:
		case EVDF_MRT_BLEND_FUNC:
		case EVDF_OCCLUSION_QUERY:
			return false;
		case EVDF_STENCIL_BUFFER:
			return StencilBuffer;
		case EVDF_TEXTURE_MULTISAMPLE:
			return TextureMultisampleSupported;
		case EVDF_TEXTURE_2D_ARRAY:
			return Texture2DArraySupported;
		default:
			return false;
		};
	}

	static GLint GetInteger(GLenum key)
	{
		GLint val = 0;
		GL.GetIntegerv(key, &val);
		return val;
	};

	inline void irrGlActiveTexture(GLenum texture)
	{
		GL.ActiveTexture(texture);
	}

	inline void irrGlCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border,
			GLsizei imageSize, const void *data)
	{
		os::Printer::log("Compressed textures aren't supported", ELL_ERROR);
	}

	inline void irrGlCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
			GLenum format, GLsizei imageSize, const void *data)
	{
		os::Printer::log("Compressed textures aren't supported", ELL_ERROR);
	}

	inline void irrGlUseProgram(GLuint prog)
	{
		GL.UseProgram(prog);
	}

	inline void irrGlBindFramebuffer(GLenum target, GLuint framebuffer)
	{
		GL.BindFramebuffer(target, framebuffer);
	}

	inline void irrGlDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
	{
		GL.DeleteFramebuffers(n, framebuffers);
	}

	inline void irrGlGenFramebuffers(GLsizei n, GLuint *framebuffers)
	{
		GL.GenFramebuffers(n, framebuffers);
	}

	inline GLenum irrGlCheckFramebufferStatus(GLenum target)
	{
		return GL.CheckFramebufferStatus(target);
	}

	inline void irrGlFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
	{
		GL.FramebufferTexture2D(target, attachment, textarget, texture, level);
	}

	inline void irrGlGenerateMipmap(GLenum target)
	{
		GL.GenerateMipmap(target);
	}

	inline void irrGlDrawBuffer(GLenum mode)
	{
		// GLES only has DrawBuffers, so use that
		GL.DrawBuffers(1, &mode);
	}

	inline void irrGlDrawBuffers(GLsizei n, const GLenum *bufs)
	{
		GL.DrawBuffers(n, bufs);
	}

	inline void irrGlBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
	{
		GL.BlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
	}

	inline void irrGlBlendEquation(GLenum mode)
	{
		GL.BlendEquation(mode);
	}

	inline void irrGlObjectLabel(GLenum identifier, GLuint name, const char *label)
	{
		if (KHRDebugSupported) {
			u32 len = static_cast<u32>(strlen(label));
			// Since our texture strings can get quite long we also truncate
			// to a hardcoded limit of 82
			len = std::min(len, std::min(MaxLabelLength, 82U));
			GL.ObjectLabel(identifier, name, len, label);
		}
	}

	bool LODBiasSupported = false;
	bool AnisotropicFilterSupported = false;
	bool BlendMinMaxSupported = false;
	bool TextureMultisampleSupported = false;
	bool Texture2DArraySupported = false;
	bool KHRDebugSupported = false;
	u32 MaxLabelLength = 0;
};

}
