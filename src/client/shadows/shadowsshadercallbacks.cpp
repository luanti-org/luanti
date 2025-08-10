// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#include "client/shadows/shadowsshadercallbacks.h"
#include "client/renderingengine.h"

void ShadowUniformSetter::onSetUniforms(video::IMaterialRendererServices *services)
{
	auto *shadow = RenderingEngine::get_shadow_renderer();
	if (!shadow)
		return;

	const auto &light = shadow->getDirectionalLight();

	core::matrix4 shadowViewProj = light.getProjectionMatrix();
	shadowViewProj *= light.getViewMatrix();
	m_shadow_view_proj.set(shadowViewProj.pointer(), services);

	m_light_direction.set(light.getDirection(), services);

	f32 TextureResolution = light.getMapResolution();
	m_texture_res.set(&TextureResolution, services);

	f32 ShadowStrength = shadow->getShadowStrength();
	m_shadow_strength.set(&ShadowStrength, services);

	video::SColor ShadowTint = shadow->getShadowTint();
	m_shadow_tint.set(ShadowTint, services);

	f32 timeOfDay = shadow->getTimeOfDay();
	m_time_of_day.set(&timeOfDay, services);

	f32 shadowFar = shadow->getMaxShadowFar();
	m_shadowfar.set(&shadowFar, services);

	f32 cam_pos[4];
	shadowViewProj.transformVect(cam_pos, light.getPlayerPos());
	m_camera_pos.set(cam_pos, services);

	s32 TextureLayerID = ShadowRenderer::TEXTURE_LAYER_SHADOW;
	m_shadow_texture.set(&TextureLayerID, services);

	f32 bias0 = shadow->getPerspectiveBiasXY();
	m_perspective_bias0_vertex.set(&bias0, services);
	m_perspective_bias0_pixel.set(&bias0, services);
	f32 bias1 = 1.0f - bias0 + 1e-5f;
	m_perspective_bias1_vertex.set(&bias1, services);
	m_perspective_bias1_pixel.set(&bias1, services);
	f32 zbias = shadow->getPerspectiveBiasZ();
	m_perspective_zbias_vertex.set(&zbias, services);
	m_perspective_zbias_pixel.set(&zbias, services);
}

void ShadowDepthShaderCB::OnSetConstants(
		video::IMaterialRendererServices *services, s32 userData)
{
	video::IVideoDriver *driver = services->getVideoDriver();

	core::matrix4 lightMVP = driver->getTransform(video::ETS_PROJECTION);
	lightMVP *= driver->getTransform(video::ETS_VIEW);

	f32 cam_pos[4];
	lightMVP.transformVect(cam_pos, CameraPos);

	lightMVP *= driver->getTransform(video::ETS_WORLD);

	m_light_mvp_setting.set(lightMVP, services);
	m_map_resolution_setting.set(&MapRes, services);
	m_max_far_setting.set(&MaxFar, services);
	s32 TextureId = 0;
	m_color_map_sampler_setting.set(&TextureId, services);
	f32 bias0 = PerspectiveBiasXY;
	m_perspective_bias0.set(&bias0, services);
	f32 bias1 = 1.0f - bias0 + 1e-5f;
	m_perspective_bias1.set(&bias1, services);
	f32 zbias = PerspectiveBiasZ;
	m_perspective_zbias.set(&zbias, services);

	m_cam_pos_setting.set(cam_pos, services);
}
