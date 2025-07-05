// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#include "MaterialRenderer.h"

namespace video
{

class COpenGL3Renderer2D : public COpenGL3MaterialRenderer
{
public:
	COpenGL3Renderer2D(const c8 *vertexShaderProgram, const c8 *pixelShaderProgram, COpenGL3DriverBase *driver, bool withTexture);
	~COpenGL3Renderer2D();

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services);

	virtual bool OnRender(IMaterialRendererServices *service, E_VERTEX_TYPE vtxtype);

protected:
	bool WithTexture;
	s32 ProjectionID;
	s32 ThicknessID;
	s32 TextureUsageID;
};

}
