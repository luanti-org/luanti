// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "SMaterial.h"
#include "ITexture.h"

#include "mt_opengl.h"

#include <cassert>

namespace video
{

enum ESetTextureActive
{
	EST_ACTIVE_ALWAYS,   // texture unit always active after set call
	EST_ACTIVE_ON_CHANGE // texture unit only active after call when texture changed in cache
};

template <class TOpenGLDriver, class TOpenGLTexture>
class COpenGLCoreCacheHandler
{
	class STextureCache
	{
	public:
		STextureCache(COpenGLCoreCacheHandler &cacheHandler, E_DRIVER_TYPE driverType, u32 textureCount) :
				CacheHandler(cacheHandler), DriverType(driverType), TextureCount(textureCount)
		{
			for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
				Texture[i] = 0;
			}
		}

		~STextureCache()
		{
			clear();
		}

		const TOpenGLTexture *operator[](int index) const
		{
			if (static_cast<u32>(index) < MATERIAL_MAX_TEXTURES)
				return Texture[static_cast<u32>(index)];

			return 0;
		}

		const TOpenGLTexture *get(u32 index) const
		{
			if (index < MATERIAL_MAX_TEXTURES)
				return Texture[index];

			return 0;
		}

		bool set(u32 index, const ITexture *texture, ESetTextureActive esa = EST_ACTIVE_ALWAYS)
		{
			bool status = false;

			E_DRIVER_TYPE type = DriverType;

			if (index < MATERIAL_MAX_TEXTURES && index < TextureCount) {
				if (esa == EST_ACTIVE_ALWAYS)
					CacheHandler.setActiveTexture(GL_TEXTURE0 + index);

				const TOpenGLTexture *prevTexture = Texture[index];

				if (texture != prevTexture) {
					if (esa == EST_ACTIVE_ON_CHANGE)
						CacheHandler.setActiveTexture(GL_TEXTURE0 + index);

					if (texture) {
						type = texture->getDriverType();

						if (type == DriverType) {
							texture->grab();

							const TOpenGLTexture *curTexture = static_cast<const TOpenGLTexture *>(texture);
							const GLenum curTextureType = curTexture->getOpenGLTextureType();
							const GLenum prevTextureType = (prevTexture) ? prevTexture->getOpenGLTextureType() : curTextureType;

							if (curTextureType != prevTextureType) {
								GL.BindTexture(prevTextureType, 0);

#if defined(IRR_COMPILE_GL_COMMON)
								// The "enable/disable texture" stuff is so legacy that
								// it's not even allowed for multisample textures.
								// (IRR_COMPILE_GL_COMMON is for the legacy driver.)
								if (prevTextureType != GL_TEXTURE_2D_MULTISAMPLE)
									GL.Disable(prevTextureType);
								if (curTextureType != GL_TEXTURE_2D_MULTISAMPLE)
									GL.Enable(curTextureType);
#endif
							}
#if defined(IRR_COMPILE_GL_COMMON)
							else if (!prevTexture)
								if (curTextureType != GL_TEXTURE_2D_MULTISAMPLE)
									GL.Enable(curTextureType);
#endif

							auto name = static_cast<const TOpenGLTexture *>(texture)->getOpenGLTextureName();
							assert(name != 0);
							GL.BindTexture(curTextureType, name);
						} else {
							texture = 0;

							os::Printer::log("Fatal Error: Tried to set a texture not owned by this driver.", ELL_ERROR);
							os::Printer::log("Texture type", core::stringc((int)type), ELL_ERROR);
							os::Printer::log("Driver (or cache handler) type", core::stringc((int)DriverType), ELL_ERROR);
						}
					}

					if (!texture && prevTexture) {
						const GLenum prevTextureType = prevTexture->getOpenGLTextureType();

						GL.BindTexture(prevTextureType, 0);

#if defined(IRR_COMPILE_GL_COMMON)
						if (prevTextureType != GL_TEXTURE_2D_MULTISAMPLE)
							GL.Disable(prevTextureType);
#endif
					}

					Texture[index] = static_cast<const TOpenGLTexture *>(texture);

					if (prevTexture)
						prevTexture->drop();
				}

				status = true;
			}

			return (status && type == DriverType);
		}

		void remove(ITexture *texture)
		{
			if (!texture)
				return;

			for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
				if (Texture[i] == texture) {
					Texture[i] = 0;

					texture->drop();
				}
			}
		}

		void clear()
		{
			for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
				if (Texture[i]) {
					const TOpenGLTexture *prevTexture = Texture[i];

					Texture[i] = 0;

					prevTexture->drop();
				}
			}
		}

	private:
		COpenGLCoreCacheHandler &CacheHandler;

		E_DRIVER_TYPE DriverType;

		const TOpenGLTexture *Texture[MATERIAL_MAX_TEXTURES];
		u32 TextureCount;
	};

public:
	COpenGLCoreCacheHandler(TOpenGLDriver *driver) :
			Driver(driver),
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4355) // Warning: "'this' : used in base member initializer list. ". It's OK, we don't use the reference in STextureCache constructor.
#endif
			TextureCache(STextureCache(*this, driver->getDriverType(), driver->getFeature().MaxTextureUnits)),
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
			FrameBufferCount(0), BlendEquation(0), BlendSourceRGB(0),
			BlendDestinationRGB(0), BlendSourceAlpha(0), BlendDestinationAlpha(0), Blend(0), BlendEquationInvalid(false), BlendFuncInvalid(false), BlendInvalid(false),
			ColorMask(0), ColorMaskInvalid(false), CullFaceMode(GL_BACK), CullFace(false), DepthFunc(GL_LESS), DepthMask(true), DepthTest(false), FrameBufferID(0),
			ProgramID(0), ActiveTexture(GL_TEXTURE0), ViewportX(0), ViewportY(0)
	{
		const COpenGLCoreFeature &feature = Driver->getFeature();

		FrameBufferCount = core::max_(static_cast<GLuint>(1), static_cast<GLuint>(feature.MultipleRenderTarget));

		BlendEquation = new GLenum[FrameBufferCount];
		BlendSourceRGB = new GLenum[FrameBufferCount];
		BlendDestinationRGB = new GLenum[FrameBufferCount];
		BlendSourceAlpha = new GLenum[FrameBufferCount];
		BlendDestinationAlpha = new GLenum[FrameBufferCount];
		Blend = new bool[FrameBufferCount];
		ColorMask = new u8[FrameBufferCount];

		// Initial OpenGL values from specification.

		if (feature.BlendOperation) {
			Driver->irrGlBlendEquation(GL_FUNC_ADD);
		}

		for (u32 i = 0; i < FrameBufferCount; ++i) {
			BlendEquation[i] = GL_FUNC_ADD;

			BlendSourceRGB[i] = GL_ONE;
			BlendDestinationRGB[i] = GL_ZERO;
			BlendSourceAlpha[i] = GL_ONE;
			BlendDestinationAlpha[i] = GL_ZERO;

			Blend[i] = false;
			ColorMask[i] = ECP_ALL;
		}

		GL.BlendFunc(GL_ONE, GL_ZERO);
		GL.Disable(GL_BLEND);

		GL.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		GL.CullFace(CullFaceMode);
		GL.Disable(GL_CULL_FACE);

		GL.DepthFunc(DepthFunc);
		GL.DepthMask(GL_TRUE);
		GL.Disable(GL_DEPTH_TEST);

		Driver->irrGlActiveTexture(ActiveTexture);

#if defined(IRR_COMPILE_GL_COMMON)
		GL.Disable(GL_TEXTURE_2D);
#endif

		const core::dimension2d<u32> ScreenSize = Driver->getScreenSize();
		ViewportWidth = ScreenSize.Width;
		ViewportHeight = ScreenSize.Height;
		GL.Viewport(ViewportX, ViewportY, ViewportWidth, ViewportHeight);
	}

	virtual ~COpenGLCoreCacheHandler()
	{
		delete[] BlendEquation;
		delete[] BlendSourceRGB;
		delete[] BlendDestinationRGB;
		delete[] BlendSourceAlpha;
		delete[] BlendDestinationAlpha;
		delete[] Blend;

		delete[] ColorMask;
	}

	E_DRIVER_TYPE getDriverType() const
	{
		return Driver->getDriverType();
	}

	STextureCache &getTextureCache()
	{
		return TextureCache;
	}

	// Blending calls.

	void setBlendEquation(GLenum mode)
	{
		if (BlendEquation[0] != mode || BlendEquationInvalid) {
			Driver->irrGlBlendEquation(mode);

			for (GLuint i = 0; i < FrameBufferCount; ++i)
				BlendEquation[i] = mode;

			BlendEquationInvalid = false;
		}
	}

	void setBlendEquationIndexed(GLuint index, GLenum mode)
	{
		if (index < FrameBufferCount && BlendEquation[index] != mode) {
			Driver->irrGlBlendEquationIndexed(index, mode);

			BlendEquation[index] = mode;
			BlendEquationInvalid = true;
		}
	}

	void setBlendFunc(GLenum source, GLenum destination)
	{
		if (BlendSourceRGB[0] != source || BlendDestinationRGB[0] != destination ||
				BlendSourceAlpha[0] != source || BlendDestinationAlpha[0] != destination ||
				BlendFuncInvalid) {
			GL.BlendFunc(source, destination);

			for (GLuint i = 0; i < FrameBufferCount; ++i) {
				BlendSourceRGB[i] = source;
				BlendDestinationRGB[i] = destination;
				BlendSourceAlpha[i] = source;
				BlendDestinationAlpha[i] = destination;
			}

			BlendFuncInvalid = false;
		}
	}

	void setBlendFuncSeparate(GLenum sourceRGB, GLenum destinationRGB, GLenum sourceAlpha, GLenum destinationAlpha)
	{
		if (sourceRGB != sourceAlpha || destinationRGB != destinationAlpha) {
			if (BlendSourceRGB[0] != sourceRGB || BlendDestinationRGB[0] != destinationRGB ||
					BlendSourceAlpha[0] != sourceAlpha || BlendDestinationAlpha[0] != destinationAlpha ||
					BlendFuncInvalid) {
				Driver->irrGlBlendFuncSeparate(sourceRGB, destinationRGB, sourceAlpha, destinationAlpha);

				for (GLuint i = 0; i < FrameBufferCount; ++i) {
					BlendSourceRGB[i] = sourceRGB;
					BlendDestinationRGB[i] = destinationRGB;
					BlendSourceAlpha[i] = sourceAlpha;
					BlendDestinationAlpha[i] = destinationAlpha;
				}

				BlendFuncInvalid = false;
			}
		} else {
			setBlendFunc(sourceRGB, destinationRGB);
		}
	}

	void setBlendFuncIndexed(GLuint index, GLenum source, GLenum destination)
	{
		if (index < FrameBufferCount && (BlendSourceRGB[index] != source || BlendDestinationRGB[index] != destination ||
												BlendSourceAlpha[index] != source || BlendDestinationAlpha[index] != destination)) {
			Driver->irrGlBlendFuncIndexed(index, source, destination);

			BlendSourceRGB[index] = source;
			BlendDestinationRGB[index] = destination;
			BlendSourceAlpha[index] = source;
			BlendDestinationAlpha[index] = destination;
			BlendFuncInvalid = true;
		}
	}

	void setBlendFuncSeparateIndexed(GLuint index, GLenum sourceRGB, GLenum destinationRGB, GLenum sourceAlpha, GLenum destinationAlpha)
	{
		if (sourceRGB != sourceAlpha || destinationRGB != destinationAlpha) {
			if (index < FrameBufferCount && (BlendSourceRGB[index] != sourceRGB || BlendDestinationRGB[index] != destinationRGB ||
													BlendSourceAlpha[index] != sourceAlpha || BlendDestinationAlpha[index] != destinationAlpha)) {
				Driver->irrGlBlendFuncSeparateIndexed(index, sourceRGB, destinationRGB, sourceAlpha, destinationAlpha);

				BlendSourceRGB[index] = sourceRGB;
				BlendDestinationRGB[index] = destinationRGB;
				BlendSourceAlpha[index] = sourceAlpha;
				BlendDestinationAlpha[index] = destinationAlpha;
				BlendFuncInvalid = true;
			}
		} else {
			setBlendFuncIndexed(index, sourceRGB, destinationRGB);
		}
	}

	void setBlend(bool enable)
	{
		if (Blend[0] != enable || BlendInvalid) {
			if (enable)
				GL.Enable(GL_BLEND);
			else
				GL.Disable(GL_BLEND);

			for (GLuint i = 0; i < FrameBufferCount; ++i)
				Blend[i] = enable;

			BlendInvalid = false;
		}
	}

	void setBlendIndexed(GLuint index, bool enable)
	{
		if (index < FrameBufferCount && Blend[index] != enable) {
			if (enable)
				Driver->irrGlEnableIndexed(GL_BLEND, index);
			else
				Driver->irrGlDisableIndexed(GL_BLEND, index);

			Blend[index] = enable;
			BlendInvalid = true;
		}
	}

	// Color Mask.

	void getColorMask(u8 &mask)
	{
		mask = ColorMask[0];
	}

	void setColorMask(u8 mask)
	{
		if (ColorMask[0] != mask || ColorMaskInvalid) {
			GL.ColorMask((mask & ECP_RED) ? GL_TRUE : GL_FALSE, (mask & ECP_GREEN) ? GL_TRUE : GL_FALSE, (mask & ECP_BLUE) ? GL_TRUE : GL_FALSE, (mask & ECP_ALPHA) ? GL_TRUE : GL_FALSE);

			for (GLuint i = 0; i < FrameBufferCount; ++i)
				ColorMask[i] = mask;

			ColorMaskInvalid = false;
		}
	}

	void setColorMaskIndexed(GLuint index, u8 mask)
	{
		if (index < FrameBufferCount && ColorMask[index] != mask) {
			Driver->irrGlColorMaskIndexed(index, (mask & ECP_RED) ? GL_TRUE : GL_FALSE, (mask & ECP_GREEN) ? GL_TRUE : GL_FALSE, (mask & ECP_BLUE) ? GL_TRUE : GL_FALSE, (mask & ECP_ALPHA) ? GL_TRUE : GL_FALSE);

			ColorMask[index] = mask;
			ColorMaskInvalid = true;
		}
	}

	// Cull face calls.

	void setCullFaceFunc(GLenum mode)
	{
		if (CullFaceMode != mode) {
			GL.CullFace(mode);
			CullFaceMode = mode;
		}
	}

	void setCullFace(bool enable)
	{
		if (CullFace != enable) {
			if (enable)
				GL.Enable(GL_CULL_FACE);
			else
				GL.Disable(GL_CULL_FACE);

			CullFace = enable;
		}
	}

	// Depth calls.

	void setDepthFunc(GLenum mode)
	{
		if (DepthFunc != mode) {
			GL.DepthFunc(mode);
			DepthFunc = mode;
		}
	}

	void getDepthMask(bool &depth)
	{
		depth = DepthMask;
	}

	void setDepthMask(bool enable)
	{
		if (DepthMask != enable) {
			if (enable)
				GL.DepthMask(GL_TRUE);
			else
				GL.DepthMask(GL_FALSE);

			DepthMask = enable;
		}
	}

	void getDepthTest(bool &enable)
	{
		enable = DepthTest;
	}

	void setDepthTest(bool enable)
	{
		if (DepthTest != enable) {
			if (enable)
				GL.Enable(GL_DEPTH_TEST);
			else
				GL.Disable(GL_DEPTH_TEST);

			DepthTest = enable;
		}
	}

	// FBO calls.

	void getFBO(GLuint &frameBufferID) const
	{
		frameBufferID = FrameBufferID;
	}

	void setFBO(GLuint frameBufferID)
	{
		if (FrameBufferID != frameBufferID) {
			Driver->irrGlBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
			FrameBufferID = frameBufferID;
		}
	}

	// Shaders calls.

	void getProgram(GLuint &programID) const
	{
		programID = ProgramID;
	}

	void setProgram(GLuint programID)
	{
		if (ProgramID != programID) {
			Driver->irrGlUseProgram(programID);
			ProgramID = programID;
		}
	}

	// Texture calls.

	void getActiveTexture(GLenum &texture) const
	{
		texture = ActiveTexture;
	}

	void setActiveTexture(GLenum texture)
	{
		if (ActiveTexture != texture) {
			Driver->irrGlActiveTexture(texture);
			ActiveTexture = texture;
		}
	}

	// Viewport calls.

	void getViewport(GLint &viewportX, GLint &viewportY, GLsizei &viewportWidth, GLsizei &viewportHeight) const
	{
		viewportX = ViewportX;
		viewportY = ViewportY;
		viewportWidth = ViewportWidth;
		viewportHeight = ViewportHeight;
	}

	void setViewport(GLint viewportX, GLint viewportY, GLsizei viewportWidth, GLsizei viewportHeight)
	{
		if (ViewportX != viewportX || ViewportY != viewportY || ViewportWidth != viewportWidth || ViewportHeight != viewportHeight) {
			GL.Viewport(viewportX, viewportY, viewportWidth, viewportHeight);
			ViewportX = viewportX;
			ViewportY = viewportY;
			ViewportWidth = viewportWidth;
			ViewportHeight = viewportHeight;
		}
	}

	//! Compare material to current cache and update it when there are differences
	// Some material renderers do change the cache beyond the original material settings
	// This corrects the material to represent the current cache state again.
	void correctCacheMaterial(video::SMaterial &material)
	{
		// Fix textures which got removed
		for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
			if (material.TextureLayers[i].Texture && !TextureCache[i]) {
				material.TextureLayers[i].Texture = 0;
			}
		}
	}

protected:
	TOpenGLDriver *Driver;

	STextureCache TextureCache;

	GLuint FrameBufferCount;

	GLenum *BlendEquation;
	GLenum *BlendSourceRGB;
	GLenum *BlendDestinationRGB;
	GLenum *BlendSourceAlpha;
	GLenum *BlendDestinationAlpha;
	bool *Blend;
	bool BlendEquationInvalid;
	bool BlendFuncInvalid;
	bool BlendInvalid;

	u8 *ColorMask;
	bool ColorMaskInvalid;

	GLenum CullFaceMode;
	bool CullFace;

	GLenum DepthFunc;
	bool DepthMask;
	bool DepthTest;

	GLuint FrameBufferID;

	GLuint ProgramID;

	GLenum ActiveTexture;

	GLint ViewportX;
	GLint ViewportY;
	GLsizei ViewportWidth;
	GLsizei ViewportHeight;
};

}
