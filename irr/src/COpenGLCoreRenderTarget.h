// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IRenderTarget.h"
#include <stdexcept>

#ifndef GL_FRAMEBUFFER_INCOMPLETE_FORMATS
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT
#endif

namespace video
{

template <class TOpenGLDriver, class TOpenGLTexture>
class COpenGLCoreRenderTarget : public IRenderTarget
{
public:
	COpenGLCoreRenderTarget(TOpenGLDriver *driver) :
			AssignedDepth(false), AssignedStencil(false), RequestTextureUpdate(false), RequestDepthStencilUpdate(false),
			BufferID(0), ColorAttachment(0), MultipleRenderTarget(0), Driver(driver)
	{
		DriverType = Driver->getDriverType();

		Size = Driver->getScreenSize();

		ColorAttachment = Driver->getFeature().ColorAttachment;
		MultipleRenderTarget = Driver->getFeature().MultipleRenderTarget;

		if (ColorAttachment > 0) {
			TEST_GL_ERROR(Driver);
			Driver->irrGlGenFramebuffers(1, &BufferID);
			if (!BufferID) {
				os::Printer::log("COpenGLCoreRenderTarget: framebuffer not created", ELL_ERROR);
				return;
			}
		}

		AssignedTextures.set_used(static_cast<u32>(ColorAttachment));

		for (u32 i = 0; i < AssignedTextures.size(); ++i)
			AssignedTextures[i] = GL_NONE;
	}

	virtual ~COpenGLCoreRenderTarget()
	{
		if (ColorAttachment > 0 && BufferID != 0)
			Driver->irrGlDeleteFramebuffers(1, &BufferID);

		for (u32 i = 0; i < Textures.size(); ++i) {
			if (Textures[i])
				Textures[i]->drop();
		}

		if (DepthStencil)
			DepthStencil->drop();
	}

	void setTextures(ITexture *const *textures, u32 numTextures, ITexture *depthStencil, const E_CUBE_SURFACE *cubeSurfaces, u32 numCubeSurfaces) override
	{
		bool needSizeUpdate = false;

		// Set color attachments.
		if (!Textures.equals(textures, numTextures) || !CubeSurfaces.equals(cubeSurfaces, numCubeSurfaces)) {
			needSizeUpdate = true;

			core::array<ITexture *> prevTextures(Textures);

			if (numTextures > static_cast<u32>(ColorAttachment)) {
				core::stringc message = "This GPU supports up to ";
				message += static_cast<u32>(ColorAttachment);
				message += " textures per render target.";

				os::Printer::log(message.c_str(), ELL_WARNING);
			}

			Textures.set_used(core::min_(numTextures, static_cast<u32>(ColorAttachment)));

			for (u32 i = 0; i < Textures.size(); ++i) {
				TOpenGLTexture *currentTexture = (textures[i] && textures[i]->getDriverType() == DriverType) ? static_cast<TOpenGLTexture *>(textures[i]) : 0;

				GLuint textureID = 0;

				if (currentTexture) {
					textureID = currentTexture->getOpenGLTextureName();
				}

				if (textureID != 0) {
					Textures[i] = textures[i];
					Textures[i]->grab();
				} else {
					Textures[i] = 0;
				}
			}

			for (u32 i = 0; i < prevTextures.size(); ++i) {
				if (prevTextures[i])
					prevTextures[i]->drop();
			}

			RequestTextureUpdate = true;
		}

		if (!CubeSurfaces.equals(cubeSurfaces, numCubeSurfaces)) {
			CubeSurfaces.set_data(cubeSurfaces, numCubeSurfaces);
			RequestTextureUpdate = true;
		}

		// Set depth and stencil attachments.
		if (DepthStencil != depthStencil) {
			if (DepthStencil) {
				DepthStencil->drop();
				DepthStencil = 0;
			}

			needSizeUpdate = true;
			TOpenGLTexture *currentTexture = (depthStencil && depthStencil->getDriverType() == DriverType) ? static_cast<TOpenGLTexture *>(depthStencil) : 0;

			if (currentTexture) {
				if (currentTexture->getType() == ETT_2D || currentTexture->getType() == ETT_2D_MS) {
					GLuint textureID = currentTexture->getOpenGLTextureName();

					const ECOLOR_FORMAT textureFormat = (textureID != 0) ? depthStencil->getColorFormat() : ECF_UNKNOWN;
					if (IImage::isDepthFormat(textureFormat)) {
						DepthStencil = depthStencil;
						DepthStencil->grab();
					} else {
						os::Printer::log("Ignoring depth/stencil texture without depth color format.", ELL_WARNING);
					}
				} else {
					os::Printer::log("This driver doesn't support depth/stencil to cubemaps.", ELL_WARNING);
				}
			}

			RequestDepthStencilUpdate = true;
		}

		if (needSizeUpdate) {
			// Set size required for a viewport.

			ITexture *firstTexture = getTexture();

			if (firstTexture)
				Size = firstTexture->getSize();
			else {
				if (DepthStencil)
					Size = DepthStencil->getSize();
				else
					Size = Driver->getScreenSize();
			}
		}
	}

	void update()
	{
		if (RequestTextureUpdate || RequestDepthStencilUpdate) {
			// Set color attachments.

			if (RequestTextureUpdate) {
				// Set new color textures.

				const u32 textureSize = core::min_(Textures.size(), AssignedTextures.size());

				for (u32 i = 0; i < textureSize; ++i) {
					TOpenGLTexture *currentTexture = static_cast<TOpenGLTexture *>(Textures[i]);
					GLuint textureID = currentTexture ? currentTexture->getOpenGLTextureName() : 0;

					if (textureID != 0) {
						AssignedTextures[i] = GL_COLOR_ATTACHMENT0 + i;
						GLenum textarget;
						switch (currentTexture->getType()) {
						case ETT_2D:
							textarget = GL_TEXTURE_2D;
							break;
						case ETT_2D_MS:
							textarget = GL_TEXTURE_2D_MULTISAMPLE;
							break;
						case ETT_CUBEMAP:
							textarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)CubeSurfaces[i];
							break;
						default:
							throw std::logic_error("not reachable");
						}
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, AssignedTextures[i], textarget, textureID, 0);
						TEST_GL_ERROR(Driver);
					} else if (AssignedTextures[i] != GL_NONE) {
						AssignedTextures[i] = GL_NONE;
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, AssignedTextures[i], GL_TEXTURE_2D, 0, 0);

						os::Printer::log("Error: Could not set render target.", ELL_ERROR);
					}
				}

				// Reset other render target channels.

				for (u32 i = textureSize; i < AssignedTextures.size(); ++i) {
					if (AssignedTextures[i] != GL_NONE) {
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, AssignedTextures[i], GL_TEXTURE_2D, 0, 0);
						AssignedTextures[i] = GL_NONE;
					}
				}

				RequestTextureUpdate = false;
			}

			// Set depth and stencil attachments.

			if (RequestDepthStencilUpdate) {
				const ECOLOR_FORMAT textureFormat = DepthStencil ? DepthStencil->getColorFormat() : ECF_UNKNOWN;

				if (IImage::isDepthFormat(textureFormat)) {
					GLenum textarget;
					switch (DepthStencil->getType()) {
					case ETT_2D:
						textarget = GL_TEXTURE_2D;
						break;
					case ETT_2D_MS:
						textarget = GL_TEXTURE_2D_MULTISAMPLE;
						break;
					default:
						// ETT_CUBEMAP is rejected for depth/stencil by setTextures
						throw std::logic_error("not reachable");
					}

					GLuint textureID = static_cast<TOpenGLTexture *>(DepthStencil)->getOpenGLTextureName();

#ifdef _IRR_EMSCRIPTEN_PLATFORM_ // The WEBGL_depth_texture extension does not allow attaching stencil+depth separate.
					if (textureFormat == ECF_D24S8) {
						GLenum attachment = 0x821A; // GL_DEPTH_STENCIL_ATTACHMENT
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textarget, textureID, 0);
						AssignedStencil = true;
					} else {
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textarget, textureID, 0);
						AssignedStencil = false;
					}
#else
					Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textarget, textureID, 0);

					if (textureFormat == ECF_D24S8) {
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, textarget, textureID, 0);

						AssignedStencil = true;
					} else {
						if (AssignedStencil)
							Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, textarget, 0, 0);

						AssignedStencil = false;
					}
#endif
					AssignedDepth = true;
				} else {
					// No (valid) depth/stencil texture.
					if (AssignedDepth)
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

					if (AssignedStencil)
						Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

					AssignedDepth = false;
					AssignedStencil = false;
				}
				TEST_GL_ERROR(Driver);

				RequestDepthStencilUpdate = false;
			}

			// Configure drawing operation.

			if (ColorAttachment > 0 && BufferID != 0) {
				const u32 textureSize = Textures.size();

				if (textureSize == 0)
					Driver->irrGlDrawBuffer(GL_NONE);
				else if (textureSize == 1 || MultipleRenderTarget == 0)
					Driver->irrGlDrawBuffer(GL_COLOR_ATTACHMENT0);
				else {
					const u32 bufferCount = core::min_(MultipleRenderTarget, core::min_(textureSize, AssignedTextures.size()));

					Driver->irrGlDrawBuffers(bufferCount, AssignedTextures.pointer());
				}

				TEST_GL_ERROR(Driver);
			}

#ifdef _DEBUG
			checkFBO(Driver);
#endif
		}
	}

	GLuint getBufferID() const
	{
		return BufferID;
	}

	const core::dimension2d<u32> &getSize() const
	{
		return Size;
	}

	ITexture *getTexture() const
	{
		for (u32 i = 0; i < Textures.size(); ++i) {
			if (Textures[i])
				return Textures[i];
		}

		return 0;
	}

protected:
	bool checkFBO(TOpenGLDriver *driver)
	{
		if (ColorAttachment == 0)
			return true;

		GLenum status = driver->irrGlCheckFramebufferStatus(GL_FRAMEBUFFER);

		switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:
			return true;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			os::Printer::log("FBO has invalid read buffer", ELL_ERROR);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			os::Printer::log("FBO has invalid draw buffer", ELL_ERROR);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			os::Printer::log("FBO has one or several incomplete image attachments", ELL_ERROR);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
			os::Printer::log("FBO has one or several image attachments with different internal formats", ELL_ERROR);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
			os::Printer::log("FBO has one or several image attachments with different dimensions", ELL_ERROR);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			os::Printer::log("FBO missing an image attachment", ELL_ERROR);
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			os::Printer::log("FBO format unsupported", ELL_ERROR);
			break;
		default:
			os::Printer::log("FBO error", ELL_ERROR);
			break;
		}

		return false;
	}

	core::array<GLenum> AssignedTextures;
	bool AssignedDepth;
	bool AssignedStencil;

	bool RequestTextureUpdate;
	bool RequestDepthStencilUpdate;

	GLuint BufferID;

	core::dimension2d<u32> Size;

	u32 ColorAttachment;
	u32 MultipleRenderTarget;

	TOpenGLDriver *Driver;
};

}
