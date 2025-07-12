// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#pragma once
#include <IMaterialRendererServices.h>
#include <IShaderConstantSetCallBack.h>
#include "client/shader.h"

class ShadowRenderer;

// Used by main game rendering

class ShadowUniformSetter : public IShaderUniformSetter
{
	CachedPixelShaderSetting<f32, 16> m_shadow_view_proj{"m_ShadowViewProj"};
	CachedPixelShaderSetting<f32, 3> m_light_direction{"v_LightDirection"};
	CachedPixelShaderSetting<f32> m_texture_res{"f_textureresolution"};
	CachedPixelShaderSetting<f32> m_shadow_strength{"f_shadow_strength"};
	CachedPixelShaderSetting<f32, 3> m_shadow_tint{ "shadow_tint" };
	CachedPixelShaderSetting<f32> m_time_of_day{"f_timeofday"};
	CachedPixelShaderSetting<f32> m_shadowfar{"f_shadowfar"};
	CachedPixelShaderSetting<f32, 4> m_camera_pos{"CameraPos"};
	CachedPixelShaderSetting<s32> m_shadow_texture{"ShadowMapSampler"};
	CachedVertexShaderSetting<f32>
		m_perspective_bias0_vertex{"xyPerspectiveBias0"};
	CachedPixelShaderSetting<f32>
		m_perspective_bias0_pixel{"xyPerspectiveBias0"};
	CachedVertexShaderSetting<f32>
		m_perspective_bias1_vertex{"xyPerspectiveBias1"};
	CachedPixelShaderSetting<f32>
		m_perspective_bias1_pixel{"xyPerspectiveBias1"};
	CachedVertexShaderSetting<f32>
		m_perspective_zbias_vertex{"zPerspectiveBias"};
	CachedPixelShaderSetting<f32> m_perspective_zbias_pixel{"zPerspectiveBias"};

public:
	ShadowUniformSetter() = default;
	~ShadowUniformSetter() = default;

	virtual void onSetUniforms(video::IMaterialRendererServices *services) override;
};

class ShadowUniformSetterFactory : public IShaderUniformSetterFactory
{
public:
	virtual IShaderUniformSetter *create() {
		return new ShadowUniformSetter();
	}
};

// Used by depth shader

class ShadowDepthUniformSetter : public IShaderUniformSetter
{
public:
	explicit ShadowDepthUniformSetter(ShadowRenderer *shadowRenderer);
	~ShadowDepthUniformSetter() = default;

	virtual void onSetUniforms(video::IMaterialRendererServices* services) override;

private:
	ShadowRenderer* m_shadowRenderer;
	CachedVertexShaderSetting<f32, 16> m_light_mvp_setting{ "LightMVP" };
	CachedVertexShaderSetting<f32> m_map_resolution_setting{ "MapResolution" };
	CachedVertexShaderSetting<f32> m_max_far_setting{ "MaxFar" };
	CachedPixelShaderSetting<s32>
		m_color_map_sampler_setting{ "ColorMapSampler" };
	CachedVertexShaderSetting<f32> m_perspective_bias0{ "xyPerspectiveBias0" };
	CachedVertexShaderSetting<f32> m_perspective_bias1{ "xyPerspectiveBias1" };
	CachedVertexShaderSetting<f32> m_perspective_zbias{ "zPerspectiveBias" };
	CachedVertexShaderSetting<f32, 4> m_cam_pos_setting{ "CameraPos" };
};

class ShadowDepthUniformSetterFactory : public IShaderUniformSetterFactory
{
public:
	explicit ShadowDepthUniformSetterFactory(ShadowRenderer* shadowRenderer) :
		m_shadowRenderer(shadowRenderer)
	{}
	virtual IShaderUniformSetter* create() {
		return new ShadowDepthUniformSetter(m_shadowRenderer);
	}
private:
	ShadowRenderer* m_shadowRenderer;
};
