// Copyright (C) 2023 Vitaliy Lobachevskiy
// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "FixedPipelineRenderer.h"
#include "os.h"

#include "IVideoDriver.h"

namespace irr
{
namespace video
{

// Base callback

COpenGL3MaterialBaseCB::COpenGL3MaterialBaseCB() :
		FirstUpdateBase(true), WVPMatrixID(-1), WVMatrixID(-1),
		FogEnableID(-1), FogTypeID(-1), FogColorID(-1), FogStartID(-1),
		FogEndID(-1), FogDensityID(-1), ThicknessID(-1), Thickness(1.f), FogEnable(false)
{
}

void COpenGL3MaterialBaseCB::OnSetMaterial(const SMaterial &material)
{
	FogEnable = material.FogEnable;
	Thickness = (material.Thickness > 0.f) ? material.Thickness : 1.f;
}

void COpenGL3MaterialBaseCB::OnSetConstants(IMaterialRendererServices *services, s32 userData)
{
	IVideoDriver *driver = services->getVideoDriver();

	if (FirstUpdateBase) {
		WVPMatrixID = services->getVertexShaderConstantID("uWVPMatrix");
		WVMatrixID = services->getVertexShaderConstantID("uWVMatrix");

		FogEnableID = services->getVertexShaderConstantID("uFogEnable");
		FogTypeID = services->getVertexShaderConstantID("uFogType");
		FogColorID = services->getVertexShaderConstantID("uFogColor");
		FogStartID = services->getVertexShaderConstantID("uFogStart");
		FogEndID = services->getVertexShaderConstantID("uFogEnd");
		FogDensityID = services->getVertexShaderConstantID("uFogDensity");
		ThicknessID = services->getVertexShaderConstantID("uThickness");

		FirstUpdateBase = false;
	}

	const core::matrix4 &W = driver->getTransform(ETS_WORLD);
	const core::matrix4 &V = driver->getTransform(ETS_VIEW);
	const core::matrix4 &P = driver->getTransform(ETS_PROJECTION);

	core::matrix4 Matrix = V * W;
	services->setPixelShaderConstant(WVMatrixID, Matrix.pointer(), 16);

	Matrix = P * Matrix;
	services->setPixelShaderConstant(WVPMatrixID, Matrix.pointer(), 16);

	s32 TempEnable = FogEnable ? 1 : 0;
	services->setPixelShaderConstant(FogEnableID, &TempEnable, 1);

	if (FogEnable) {
		SColor TempColor(0);
		E_FOG_TYPE TempType = EFT_FOG_LINEAR;
		f32 FogStart, FogEnd, FogDensity;
		bool unused = false;

		driver->getFog(TempColor, TempType, FogStart, FogEnd, FogDensity, unused, unused);

		s32 FogType = (s32)TempType;
		SColorf FogColor(TempColor);

		services->setPixelShaderConstant(FogTypeID, &FogType, 1);
		services->setPixelShaderConstant(FogColorID, reinterpret_cast<f32 *>(&FogColor), 4);
		services->setPixelShaderConstant(FogStartID, &FogStart, 1);
		services->setPixelShaderConstant(FogEndID, &FogEnd, 1);
		services->setPixelShaderConstant(FogDensityID, &FogDensity, 1);
	}

	services->setPixelShaderConstant(ThicknessID, &Thickness, 1);
}

// EMT_SOLID + EMT_TRANSPARENT_ALPHA_CHANNEL + EMT_TRANSPARENT_VERTEX_ALPHA

COpenGL3MaterialSolidCB::COpenGL3MaterialSolidCB() :
		FirstUpdate(true), TMatrix0ID(-1), AlphaRefID(-1), TextureUsage0ID(-1), TextureUnit0ID(-1), AlphaRef(0.5f), TextureUsage0(0), TextureUnit0(0)
{
}

void COpenGL3MaterialSolidCB::OnSetMaterial(const SMaterial &material)
{
	COpenGL3MaterialBaseCB::OnSetMaterial(material);

	AlphaRef = material.MaterialTypeParam;
	TextureUsage0 = (material.TextureLayers[0].Texture) ? 1 : 0;
}

void COpenGL3MaterialSolidCB::OnSetConstants(IMaterialRendererServices *services, s32 userData)
{
	COpenGL3MaterialBaseCB::OnSetConstants(services, userData);

	IVideoDriver *driver = services->getVideoDriver();

	if (FirstUpdate) {
		TMatrix0ID = services->getVertexShaderConstantID("uTMatrix0");
		AlphaRefID = services->getVertexShaderConstantID("uAlphaRef");
		TextureUsage0ID = services->getVertexShaderConstantID("uTextureUsage0");
		TextureUnit0ID = services->getVertexShaderConstantID("uTextureUnit0");

		FirstUpdate = false;
	}

	core::matrix4 Matrix = driver->getTransform(ETS_TEXTURE_0);
	services->setPixelShaderConstant(TMatrix0ID, Matrix.pointer(), 16);

	services->setPixelShaderConstant(AlphaRefID, &AlphaRef, 1);
	services->setPixelShaderConstant(TextureUsage0ID, &TextureUsage0, 1);
	services->setPixelShaderConstant(TextureUnit0ID, &TextureUnit0, 1);
}

// EMT_ONETEXTURE_BLEND

COpenGL3MaterialOneTextureBlendCB::COpenGL3MaterialOneTextureBlendCB() :
		FirstUpdate(true), TMatrix0ID(-1), BlendTypeID(-1), TextureUsage0ID(-1), TextureUnit0ID(-1), BlendType(0), TextureUsage0(0), TextureUnit0(0)
{
}

void COpenGL3MaterialOneTextureBlendCB::OnSetMaterial(const SMaterial &material)
{
	COpenGL3MaterialBaseCB::OnSetMaterial(material);

	BlendType = 0;

	E_BLEND_FACTOR srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact;
	E_MODULATE_FUNC modulate;
	u32 alphaSource;
	unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulate, alphaSource, material.MaterialTypeParam);

	if (textureBlendFunc_hasAlpha(srcRGBFact) || textureBlendFunc_hasAlpha(dstRGBFact) || textureBlendFunc_hasAlpha(srcAlphaFact) || textureBlendFunc_hasAlpha(dstAlphaFact)) {
		if (alphaSource == EAS_VERTEX_COLOR) {
			BlendType = 1;
		} else if (alphaSource == EAS_TEXTURE) {
			BlendType = 2;
		}
	}

	TextureUsage0 = (material.TextureLayers[0].Texture) ? 1 : 0;
}

void COpenGL3MaterialOneTextureBlendCB::OnSetConstants(IMaterialRendererServices *services, s32 userData)
{
	COpenGL3MaterialBaseCB::OnSetConstants(services, userData);

	IVideoDriver *driver = services->getVideoDriver();

	if (FirstUpdate) {
		TMatrix0ID = services->getVertexShaderConstantID("uTMatrix0");
		BlendTypeID = services->getVertexShaderConstantID("uBlendType");
		TextureUsage0ID = services->getVertexShaderConstantID("uTextureUsage0");
		TextureUnit0ID = services->getVertexShaderConstantID("uTextureUnit0");

		FirstUpdate = false;
	}

	core::matrix4 Matrix = driver->getTransform(ETS_TEXTURE_0);
	services->setPixelShaderConstant(TMatrix0ID, Matrix.pointer(), 16);

	services->setPixelShaderConstant(BlendTypeID, &BlendType, 1);
	services->setPixelShaderConstant(TextureUsage0ID, &TextureUsage0, 1);
	services->setPixelShaderConstant(TextureUnit0ID, &TextureUnit0, 1);
}

}
}
