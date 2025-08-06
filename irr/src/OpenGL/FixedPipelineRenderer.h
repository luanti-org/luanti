// Copyright (C) 2023 Vitaliy Lobachevskiy
// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#include "IShaderConstantSetCallBack.h"
#include "IMaterialRendererServices.h"

namespace video
{

class COpenGL3MaterialBaseCB : public IShaderConstantSetCallBack
{
public:
	COpenGL3MaterialBaseCB();

	virtual void OnSetMaterial(const SMaterial &material);
	virtual void OnSetConstants(IMaterialRendererServices *services, s32 userData);

protected:
	bool FirstUpdateBase;

	s32 WVPMatrixID;
	s32 WVMatrixID;

	s32 FogEnableID;
	s32 FogTypeID;
	s32 FogColorID;
	s32 FogStartID;
	s32 FogEndID;
	s32 FogDensityID;

	s32 ThicknessID;

	f32 Thickness;
	bool FogEnable;
};

class COpenGL3MaterialSolidCB : public COpenGL3MaterialBaseCB
{
public:
	COpenGL3MaterialSolidCB();

	virtual void OnSetMaterial(const SMaterial &material);
	virtual void OnSetConstants(IMaterialRendererServices *services, s32 userData);

protected:
	bool FirstUpdate;

	s32 TMatrix0ID;
	s32 AlphaRefID;
	s32 TextureUsage0ID;
	s32 TextureUnit0ID;

	f32 AlphaRef;
	s32 TextureUsage0;
	s32 TextureUnit0;
};

class COpenGL3MaterialOneTextureBlendCB : public COpenGL3MaterialBaseCB
{
public:
	COpenGL3MaterialOneTextureBlendCB();

	virtual void OnSetMaterial(const SMaterial &material);
	virtual void OnSetConstants(IMaterialRendererServices *services, s32 userData);

protected:
	bool FirstUpdate;

	s32 TMatrix0ID;
	s32 BlendTypeID;
	s32 TextureUsage0ID;
	s32 TextureUnit0ID;

	s32 BlendType;
	s32 TextureUsage0;
	s32 TextureUnit0;
};

}
