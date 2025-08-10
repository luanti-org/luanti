// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <string>
#include <vector>
#include "EMaterialTypes.h"
#include "IMaterialRenderer.h"
#include "IMaterialRendererServices.h"
#include "IGPUProgrammingServices.h"

#include "Common.h"

namespace video
{

class COpenGL3DriverBase;

class COpenGL3MaterialRenderer : public IMaterialRenderer, public IMaterialRendererServices
{
public:
	COpenGL3MaterialRenderer(
			COpenGL3DriverBase *driver,
			s32 &outMaterialTypeNr,
			const c8 *vertexShaderProgram = 0,
			const c8 *pixelShaderProgram = 0,
			const c8 *debugName = nullptr,
			IShaderConstantSetCallBack *callback = 0,
			E_MATERIAL_TYPE baseMaterial = EMT_SOLID,
			s32 userData = 0);

	virtual ~COpenGL3MaterialRenderer();

	GLuint getProgram() const;

	virtual void OnSetMaterial(const SMaterial &material, const SMaterial &lastMaterial,
			bool resetAllRenderstates, IMaterialRendererServices *services) override;

	virtual bool OnRender(IMaterialRendererServices *service, E_VERTEX_TYPE vtxtype) override;

	virtual void OnUnsetMaterial() override;

	virtual bool isTransparent() const override;

	virtual s32 getRenderCapability() const override;

	void setBasicRenderStates(const SMaterial &material, const SMaterial &lastMaterial, bool resetAllRenderstates) override;

	s32 getVertexShaderConstantID(const c8 *name) override;
	s32 getPixelShaderConstantID(const c8 *name) override;
	bool setVertexShaderConstant(s32 index, const f32 *floats, int count) override;
	bool setVertexShaderConstant(s32 index, const s32 *ints, int count) override;
	bool setVertexShaderConstant(s32 index, const u32 *ints, int count) override;
	bool setPixelShaderConstant(s32 index, const f32 *floats, int count) override;
	bool setPixelShaderConstant(s32 index, const s32 *ints, int count) override;
	bool setPixelShaderConstant(s32 index, const u32 *ints, int count) override;

	IVideoDriver *getVideoDriver() override;

protected:
	COpenGL3MaterialRenderer(COpenGL3DriverBase *driver,
			IShaderConstantSetCallBack *callback = 0,
			E_MATERIAL_TYPE baseMaterial = EMT_SOLID,
			s32 userData = 0);

	void init(s32 &outMaterialTypeNr, const c8 *vertexShaderProgram,
		const c8 *pixelShaderProgram, const c8 *debugName = nullptr,
		bool addMaterial = true);

	bool createShader(GLenum shaderType, const char *shader);
	bool linkProgram();

	COpenGL3DriverBase *Driver;
	IShaderConstantSetCallBack *CallBack;

	bool Alpha;
	bool Blending;

	struct SUniformInfo
	{
		std::string name;
		GLenum type;
		GLint location;
	};

	GLuint Program;
	std::vector<SUniformInfo> UniformInfo;
	s32 UserData;
};

}
