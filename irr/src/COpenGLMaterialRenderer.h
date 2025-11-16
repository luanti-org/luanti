// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "IMaterialRenderer.h"

#include "COpenGLDriver.h"
#include "COpenGLCacheHandler.h"

namespace video
{

//! Solid material renderer
class COpenGLMaterialRenderer_SOLID : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_SOLID(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Generic Texture Blend
class COpenGLMaterialRenderer_ONETEXTURE_BLEND : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_ONETEXTURE_BLEND(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		//		if (material.MaterialType != lastMaterial.MaterialType ||
		//			material.MaterialTypeParam != lastMaterial.MaterialTypeParam ||
		//			resetAllRenderstates)
		{
			E_BLEND_FACTOR srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact;
			E_MODULATE_FUNC modulate;
			u32 alphaSource;
			unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulate, alphaSource, material.MaterialTypeParam);

			Driver->getCacheHandler()->setBlend(true);

			if (Driver->queryFeature(EVDF_BLEND_SEPARATE)) {
				Driver->getCacheHandler()->setBlendFuncSeparate(Driver->getGLBlend(srcRGBFact), Driver->getGLBlend(dstRGBFact),
						Driver->getGLBlend(srcAlphaFact), Driver->getGLBlend(dstAlphaFact));
			} else {
				Driver->getCacheHandler()->setBlendFunc(Driver->getGLBlend(srcRGBFact), Driver->getGLBlend(dstRGBFact));
			}

			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

#ifdef GL_ARB_texture_env_combine
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (f32)modulate);
#else
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, (f32)modulate);
#endif

			if (textureBlendFunc_hasAlpha(srcRGBFact) || textureBlendFunc_hasAlpha(dstRGBFact) ||
					textureBlendFunc_hasAlpha(srcAlphaFact) || textureBlendFunc_hasAlpha(dstAlphaFact)) {
				if (alphaSource == EAS_VERTEX_COLOR) {
#ifdef GL_ARB_texture_env_combine
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PRIMARY_COLOR_ARB);
#else
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_PRIMARY_COLOR_EXT);
#endif
				} else if (alphaSource == EAS_TEXTURE) {
#ifdef GL_ARB_texture_env_combine
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
#else
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
#endif
				} else {
#ifdef GL_ARB_texture_env_combine
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PRIMARY_COLOR_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
#else
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_PRIMARY_COLOR_EXT);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_TEXTURE);
#endif
				}
			}
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

#ifdef GL_ARB_texture_env_combine
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.f);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
#else
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);
		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.f);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
#endif

		Driver->getCacheHandler()->setBlend(false);
	}

	//! Returns if the material is transparent.
	/** Is not always transparent, but mostly. */
	bool isTransparent() const override
	{
		return true;
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Transparent vertex alpha material renderer
class COpenGLMaterialRenderer_TRANSPARENT_VERTEX_ALPHA : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_TRANSPARENT_VERTEX_ALPHA(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		Driver->getCacheHandler()->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		Driver->getCacheHandler()->setBlend(true);

		if (material.MaterialType != lastMaterial.MaterialType || resetAllRenderstates) {
			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

#ifdef GL_ARB_texture_env_combine
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PRIMARY_COLOR_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
#else
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_PRIMARY_COLOR_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
#endif
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#ifdef GL_ARB_texture_env_combine
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
#else
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
#endif

		Driver->getCacheHandler()->setBlend(false);
	}

	//! Returns if the material is transparent.
	bool isTransparent() const override
	{
		return true;
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Transparent alpha channel material renderer
class COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		Driver->getCacheHandler()->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		Driver->getCacheHandler()->setBlend(true);
		Driver->getCacheHandler()->setAlphaTest(true);
		Driver->getCacheHandler()->setAlphaFunc(GL_GREATER, material.MaterialTypeParam);

		if (material.MaterialType != lastMaterial.MaterialType || resetAllRenderstates || material.MaterialTypeParam != lastMaterial.MaterialTypeParam) {
			Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

#ifdef GL_ARB_texture_env_combine
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
#else
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
#endif
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setActiveTexture(GL_TEXTURE0_ARB);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#ifdef GL_ARB_texture_env_combine
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
#else
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);
#endif
		Driver->getCacheHandler()->setAlphaTest(false);
		Driver->getCacheHandler()->setBlend(false);
	}

	//! Returns if the material is transparent.
	bool isTransparent() const override
	{
		return true;
	}

protected:
	video::COpenGLDriver *Driver;
};

//! Transparent alpha channel material renderer
class COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL_REF : public IMaterialRenderer
{
public:
	COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL_REF(video::COpenGLDriver *d) :
			Driver(d) {}

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override
	{
		if (Driver->getFixedPipelineState() == COpenGLDriver::EOFPS_DISABLE)
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_DISABLE_TO_ENABLE);
		else
			Driver->setFixedPipelineState(COpenGLDriver::EOFPS_ENABLE);

		Driver->disableTextures(1);
		Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

		if (material.MaterialType != lastMaterial.MaterialType || resetAllRenderstates) {
			Driver->getCacheHandler()->setAlphaTest(true);
			Driver->getCacheHandler()->setAlphaFunc(GL_GREATER, 0.5f);
		}
	}

	void OnUnsetMaterial() override
	{
		Driver->getCacheHandler()->setAlphaTest(false);
	}

	//! Returns if the material is transparent.
	bool isTransparent() const override
	{
		return false; // this material is not really transparent because it does no blending.
	}

protected:
	video::COpenGLDriver *Driver;
};

} // end namespace video

#endif
