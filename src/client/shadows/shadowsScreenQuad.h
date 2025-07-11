// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#pragma once
#include <IMaterialRendererServices.h>
#include <IShaderConstantSetCallBack.h>
#include "client/shader.h"

class shadowScreenQuad
{
public:
	shadowScreenQuad();

	void render(video::IVideoDriver *driver);
	video::SMaterial &getMaterial() { return Material; }

private:
	video::S3DVertex Vertices[6];
	video::SMaterial Material;
};

class ShadowScreenQuadUniformSetter : public IShaderUniformSetter
{
public:
	ShadowScreenQuadUniformSetter() = default;
	~ShadowScreenQuadUniformSetter() = default;

	virtual void onSetUniforms(video::IMaterialRendererServices* services) override;

private:
	CachedPixelShaderSetting<s32> m_sm_client_map_setting{"ShadowMapClientMap"};
	CachedPixelShaderSetting<s32>
		m_sm_client_map_trans_setting{"ShadowMapClientMapTraslucent"};
	CachedPixelShaderSetting<s32>
		m_sm_dynamic_sampler_setting{"ShadowMapSamplerdynamic"};
};

class ShadowScreenQuadUniformSetterFactory : public IShaderUniformSetterFactory
{
public:
	virtual IShaderUniformSetter* create() {
		return new ShadowScreenQuadUniformSetter();
	}
};
