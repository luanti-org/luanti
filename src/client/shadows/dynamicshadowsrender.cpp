// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#include <cstring>
#include <cmath>
#include "client/shadows/dynamicshadowsrender.h"
#include "client/shadows/shadowsScreenQuad.h"
#include "client/shadows/shadowsshadercallbacks.h"
#include "settings.h"
#include "util/string.h"
#include "client/shader.h"
#include "client/client.h"
#include "client/clientmap.h"
#include "IGPUProgrammingServices.h"
#include "IVideoDriver.h"

ShadowRenderer::ShadowRenderer(IrrlichtDevice *device, Client *client) :
		m_smgr(device->getSceneManager()), m_driver(device->getVideoDriver()),
		m_client(client), m_shadow_strength(0.0f), m_shadow_tint(255, 0, 0, 0),
		m_time_day(0.0f), m_force_update_shadow_map(false), m_current_frame(0),
		m_perspective_bias_xy(0.8f), m_perspective_bias_z(0.5f)
{
	m_shadows_supported = true; // we will check actual support in initialize()
	m_shadows_enabled = false;

	m_shadow_strength_gamma = g_settings->getFloat("shadow_strength_gamma");
	if (std::isnan(m_shadow_strength_gamma))
		m_shadow_strength_gamma = 1.0f;
	m_shadow_strength_gamma = core::clamp(m_shadow_strength_gamma, 0.1f, 10.0f);

	m_shadow_map_max_distance = g_settings->getFloat("shadow_map_max_distance");

	m_shadow_map_texture_size = g_settings->getU32("shadow_map_texture_size");

	m_shadow_map_texture_32bit = g_settings->getBool("shadow_map_texture_32bit");
	m_shadow_map_colored = g_settings->getBool("shadow_map_color");
	m_map_shadow_update_frames = g_settings->getS16("shadow_update_frames");

	m_screen_quad = new ShadowScreenQuad();

	// add at least one light
	addDirectionalLight();
}

ShadowRenderer::~ShadowRenderer()
{
	// call to disable releases dynamically allocated resources
	disable();

	for (auto *ptr : m_shadow_depth_cb) {
		if (ptr)
			ptr->drop();
	}
	m_shadow_depth_cb.clear();

	auto *gpu = m_driver->getGPUProgrammingServices();
	for (auto id : {depth_shader, depth_shader_a, depth_shader_trans, depth_shader_trans_a}) {
		if (id != video::EMT_INVALID)
			gpu->deleteShaderMaterial(id);
	}

	delete m_screen_quad;
	m_screen_quad = nullptr;
}

void ShadowRenderer::disable()
{
	m_shadows_enabled = false;

	if (shadowMapTextureFinal) {
		m_driver->setRenderTarget(shadowMapTextureFinal, true, true,
			video::SColor(255, 255, 255, 255));
		m_driver->setRenderTarget(0, false, false);
	}

	if (shadowMapTextureDynamicObjects) {
		m_driver->removeTexture(shadowMapTextureDynamicObjects);
		shadowMapTextureDynamicObjects = nullptr;
	}

	if (shadowMapTextureFinal) {
		for (auto &node : m_shadow_node_array) {
			node.node->forEachMaterial([] (auto &mat) {
				mat.setTexture(TEXTURE_LAYER_SHADOW, nullptr);
			});
		}
		m_driver->removeTexture(shadowMapTextureFinal);
		shadowMapTextureFinal = nullptr;
	}

	if (shadowMapTextureColors) {
		m_driver->removeTexture(shadowMapTextureColors);
		shadowMapTextureColors = nullptr;
	}

	if (shadowMapClientMap) {
		m_driver->removeTexture(shadowMapClientMap);
		shadowMapClientMap = nullptr;
	}

	if (shadowMapClientMapFuture) {
		m_driver->removeTexture(shadowMapClientMapFuture);
		shadowMapClientMapFuture = nullptr;
	}
}

void ShadowRenderer::preInit(IWritableShaderSource *shsrc)
{
	if (g_settings->getBool("enable_dynamic_shadows")) {
		shsrc->addShaderUniformSetterFactory(std::make_unique<ShadowUniformSetterFactory>());
	}
}

bool ShadowRenderer::initialize()
{
	m_shadows_supported = ShadowRenderer::isSupported(m_driver);
	if (!m_shadows_supported)
		return false;

	/* Set up texture formats */
	auto &fmt1 = m_texture_format;
	auto &fmt2 = m_texture_format_color;

	if (m_shadow_map_texture_32bit && m_driver->queryTextureFormat(video::ECF_R32F))
		fmt1 = video::ECF_R32F;
	else if (m_driver->queryTextureFormat(video::ECF_R16F))
		fmt1 = video::ECF_R16F;

	if (m_shadow_map_texture_32bit && m_driver->queryTextureFormat(video::ECF_G32R32F))
		fmt2 = video::ECF_G32R32F;
	else if (m_driver->queryTextureFormat(video::ECF_G16R16F))
		fmt2 = video::ECF_G16R16F;

	infostream << "ShadowRenderer: color format = " << video::ColorFormatName(fmt1)
		<< " or " << video::ColorFormatName(fmt2) << std::endl;

	// Note: this is just a sanity check since the version checks in isSupported()
	// should already guarantee availability
	if (fmt1 == video::ECF_UNKNOWN || fmt2 == video::ECF_UNKNOWN)
		m_shadows_supported = false;
	if (!m_shadows_supported)
		return false;

	createShaders();
	return true;
}


size_t ShadowRenderer::addDirectionalLight()
{
	m_light_list.emplace_back(m_shadow_map_texture_size,
			v3f(0.f, 0.f, 0.f),
			video::SColor(255, 255, 255, 255), m_shadow_map_max_distance);
	return m_light_list.size() - 1;
}

DirectionalLight &ShadowRenderer::getDirectionalLight(u32 index)
{
	return m_light_list[index];
}

size_t ShadowRenderer::getDirectionalLightCount() const
{
	return m_light_list.size();
}

f32 ShadowRenderer::getMaxShadowFar() const
{
	float zMax = m_light_list[0].getFarValue();
	return zMax;
}

void ShadowRenderer::setShadowIntensity(float shadow_intensity)
{
	m_shadow_strength = std::pow(shadow_intensity, 1.0f / m_shadow_strength_gamma);
	if (m_shadow_strength > 1e-2f)
		enable();
	else
		disable();
}

void ShadowRenderer::addNodeToShadowList(
		scene::ISceneNode *node, E_SHADOW_MODE shadowMode)
{
	m_shadow_node_array.emplace_back(node, shadowMode);
	// node should never be ClientMap
	assert(!node->getName().has_value() || *node->getName() != "ClientMap");
	node->forEachMaterial([this] (auto &mat) {
		mat.setTexture(TEXTURE_LAYER_SHADOW, shadowMapTextureFinal);
	});
}

void ShadowRenderer::removeNodeFromShadowList(scene::ISceneNode *node)
{
	node->forEachMaterial([] (auto &mat) {
		mat.setTexture(TEXTURE_LAYER_SHADOW, nullptr);
	});

	auto it = std::find(m_shadow_node_array.begin(), m_shadow_node_array.end(), node);
	if (it == m_shadow_node_array.end()) {
		infostream << "removeNodeFromShadowList: " << node << " not found" << std::endl;
		return;
	}
	// swap with last, then remove
	*it = m_shadow_node_array.back();
	m_shadow_node_array.pop_back();
}

void ShadowRenderer::updateSMTextures()
{
	if (!m_shadows_enabled || m_smgr->getActiveCamera() == nullptr) {
		return;
	}

	if (!shadowMapTextureDynamicObjects) {

		shadowMapTextureDynamicObjects = getSMTexture(
			std::string("shadow_dynamic_") + itos(m_shadow_map_texture_size),
			m_texture_format, true);
		assert(shadowMapTextureDynamicObjects != nullptr);
	}

	if (!shadowMapClientMap) {

		shadowMapClientMap = getSMTexture(
			std::string("shadow_clientmap_") + itos(m_shadow_map_texture_size),
			m_shadow_map_colored ? m_texture_format_color : m_texture_format,
			true);
		assert(shadowMapClientMap != nullptr);
	}

	if (!shadowMapClientMapFuture && m_map_shadow_update_frames > 1) {
		shadowMapClientMapFuture = getSMTexture(
			std::string("shadow_clientmap_bb_") + itos(m_shadow_map_texture_size),
			m_shadow_map_colored ? m_texture_format_color : m_texture_format,
			true);
		assert(shadowMapClientMapFuture != nullptr);
	}

	if (m_shadow_map_colored && !shadowMapTextureColors) {
		shadowMapTextureColors = getSMTexture(
			std::string("shadow_colored_") + itos(m_shadow_map_texture_size),
			m_shadow_map_colored ? m_texture_format_color : m_texture_format,
			true);
		assert(shadowMapTextureColors != nullptr);
	}

	// Then merge all shadowmap textures
	if (!shadowMapTextureFinal) {
		video::ECOLOR_FORMAT frt;
		if (m_shadow_map_texture_32bit) {
			if (m_shadow_map_colored)
				frt = video::ECF_A32B32G32R32F;
			else
				frt = video::ECF_R32F;
		} else {
			if (m_shadow_map_colored)
				frt = video::ECF_A16B16G16R16F;
			else
				frt = video::ECF_R16F;
		}
		shadowMapTextureFinal = getSMTexture(
			std::string("shadowmap_final_") + itos(m_shadow_map_texture_size),
			frt, true);
		assert(shadowMapTextureFinal != nullptr);

		for (auto &node : m_shadow_node_array) {
			node.node->forEachMaterial([this] (auto &mat) {
				mat.setTexture(TEXTURE_LAYER_SHADOW, shadowMapTextureFinal);
			});
		}
	}

	if (!m_shadow_node_array.empty()) {
		bool reset_sm_texture = false;

		// clear texture if requested
		for (DirectionalLight &light : m_light_list) {
			reset_sm_texture |= light.should_update_map_shadow;
			light.should_update_map_shadow = false;
		}

		if (reset_sm_texture || m_force_update_shadow_map)
			m_current_frame = 0;

		video::ITexture* shadowMapTargetTexture = shadowMapClientMapFuture;
		if (shadowMapTargetTexture == nullptr)
			shadowMapTargetTexture = shadowMapClientMap;

		// Update SM incrementally:
		for (DirectionalLight &light : m_light_list) {
			// Static shader values.
			for (auto *cb : m_shadow_depth_cb) {
				if (cb) {
					cb->MapRes = (u32)m_shadow_map_texture_size;
					cb->MaxFar = (f32)m_shadow_map_max_distance * BS;
					cb->PerspectiveBiasXY = getPerspectiveBiasXY();
					cb->PerspectiveBiasZ = getPerspectiveBiasZ();
					cb->CameraPos = light.getFuturePlayerPos();
				}
			}

			// Note that force_update means we're drawing everything one go.

			if (m_current_frame < m_map_shadow_update_frames || m_force_update_shadow_map) {
				m_driver->setRenderTarget(shadowMapTargetTexture, reset_sm_texture, true,
						video::SColor(255, 255, 255, 255));
				renderShadowMap(shadowMapTargetTexture, light);

				// Render transparent part in one pass.
				// This is also handled in ClientMap.
				if (m_current_frame == m_map_shadow_update_frames - 1 || m_force_update_shadow_map) {
					if (m_shadow_map_colored) {
						m_driver->setRenderTarget(shadowMapTextureColors,
								true, false, video::SColor(255, 255, 255, 255));
					}
					renderShadowMap(shadowMapTextureColors, light,
							scene::ESNRP_TRANSPARENT);
				}
				m_driver->setRenderTarget(0, false, false);
			}

			reset_sm_texture = false;
		} // end for lights

		// move to the next section
		if (m_current_frame <= m_map_shadow_update_frames)
			++m_current_frame;

		// pass finished, swap textures and commit light changes
		if (m_current_frame == m_map_shadow_update_frames || m_force_update_shadow_map) {
			if (shadowMapClientMapFuture != nullptr)
				std::swap(shadowMapClientMapFuture, shadowMapClientMap);

			// Let all lights know that maps are updated
			for (DirectionalLight &light : m_light_list)
				light.commitFrustum();
		}
		m_force_update_shadow_map = false;
	}
}

void ShadowRenderer::update(video::ITexture *outputTarget)
{
	if (!m_shadows_enabled || m_smgr->getActiveCamera() == nullptr) {
		return;
	}

	updateSMTextures();

	if (shadowMapTextureFinal == nullptr) {
		return;
	}


	if (!m_shadow_node_array.empty()) {
		for (DirectionalLight &light : m_light_list) {
			// Static shader values for entities are set in updateSMTextures
			// SM texture for entities is not updated incrementally and
			// must by updated using current player position.
			for (auto *cb : m_shadow_depth_cb) {
				if (cb)
					cb->CameraPos = light.getPlayerPos();
			}

			// render shadows for the non-map objects.
			m_driver->setRenderTarget(shadowMapTextureDynamicObjects, true,
					true, video::SColor(255, 255, 255, 255));
			renderShadowObjects(shadowMapTextureDynamicObjects, light);
			// clear the Render Target
			m_driver->setRenderTarget(0, false, false);

			// in order to avoid too many map shadow renders,
			// we should make a second pass to mix clientmap shadows and
			// entities shadows :(
			m_screen_quad->getMaterial().setTexture(0, shadowMapClientMap);
			// dynamic objs shadow texture.
			if (m_shadow_map_colored)
				m_screen_quad->getMaterial().setTexture(1, shadowMapTextureColors);
			m_screen_quad->getMaterial().setTexture(2, shadowMapTextureDynamicObjects);

			m_driver->setRenderTarget(shadowMapTextureFinal, false, false,
					video::SColor(255, 255, 255, 255));
			m_screen_quad->render(m_driver);
			m_driver->setRenderTarget(0, false, false);

		} // end for lights
	}
}

void ShadowRenderer::drawDebug()
{
	/* this code just shows shadows textures in screen and in ONLY for debugging*/
#if 0
	// this is debug, ignore for now.
	if (shadowMapTextureFinal)
		m_driver->draw2DImage(shadowMapTextureFinal,
				core::rect<s32>(0, 50, 128, 128 + 50),
				core::rect<s32>({0, 0}, shadowMapTextureFinal->getSize()));

	if (shadowMapClientMap)
		m_driver->draw2DImage(shadowMapClientMap,
				core::rect<s32>(0, 50 + 128, 128, 128 + 50 + 128),
				core::rect<s32>({0, 0}, shadowMapTextureFinal->getSize()));

	if (shadowMapTextureDynamicObjects)
		m_driver->draw2DImage(shadowMapTextureDynamicObjects,
				core::rect<s32>(0, 128 + 50 + 128, 128,
						128 + 50 + 128 + 128),
				core::rect<s32>({0, 0}, shadowMapTextureDynamicObjects->getSize()));

	if (m_shadow_map_colored && shadowMapTextureColors) {

		m_driver->draw2DImage(shadowMapTextureColors,
				core::rect<s32>(128,128 + 50 + 128 + 128,
						128 + 128, 128 + 50 + 128 + 128 + 128),
				core::rect<s32>({0, 0}, shadowMapTextureColors->getSize()));
	}
#endif
}


video::ITexture *ShadowRenderer::getSMTexture(const std::string &shadow_map_name,
		video::ECOLOR_FORMAT texture_format, bool force_creation)
{
	if (force_creation) {
		return m_driver->addRenderTargetTexture(
				core::dimension2du(m_shadow_map_texture_size,
						m_shadow_map_texture_size),
				shadow_map_name.c_str(), texture_format);
	}

	return m_driver->findTexture(shadow_map_name.c_str());
}

void ShadowRenderer::renderShadowMap(video::ITexture *target,
		DirectionalLight &light, scene::E_SCENE_NODE_RENDER_PASS pass)
{
	bool is_transparent_pass = pass != scene::ESNRP_SOLID;

	m_driver->setTransform(video::ETS_VIEW, light.getFutureViewMatrix());
	m_driver->setTransform(video::ETS_PROJECTION, light.getFutureProjectionMatrix());

	// ClientMap will call this for every material it renders
	ModifyMaterialCallback cb = [&] (video::SMaterial &mat, bool foliage) {
		// Do not override culling if the original material renders both back
		// and front faces in solid mode (e.g. plantlike)
		// Transparent plants would still render shadows only from one side,
		// but this conflicts with water which occurs much more frequently
		if (is_transparent_pass || mat.BackfaceCulling || mat.FrontfaceCulling) {
			mat.BackfaceCulling = false;
			mat.FrontfaceCulling = true;
		}
		if (foliage) {
			mat.BackfaceCulling = true;
			mat.FrontfaceCulling = false;
		}

		/*
		 * Here we unconditionally replace the material shader with our custom ones
		 * to render the depth map.
		 * Be warned that this is a very flawed approach and the reason why waving
		 * doesn't work or why the node alpha mode is totally ignored.
		 * Array texture support was tacked on but this should really be rewritten:
		 * The shadow map code should be part of nodes_shader and activated on demand.
		 */
		bool array_tex = mat.getTexture(0) && mat.getTexture(0)->getType() == video::ETT_2D_ARRAY;
		if (m_shadow_map_colored && is_transparent_pass) {
			mat.MaterialType = array_tex ? depth_shader_trans_a : depth_shader_trans;
		} else {
			mat.MaterialType = array_tex ? depth_shader_a : depth_shader;
			mat.BlendOperation = video::EBO_MIN;
		}
	};

	ClientMap &map_node = static_cast<ClientMap &>(m_client->getEnv().getMap());

	int frame = m_force_update_shadow_map ? 0 : m_current_frame;
	int total_frames = m_force_update_shadow_map ? 1 : m_map_shadow_update_frames;

	map_node.renderMapShadows(m_driver, cb, pass, frame, total_frames);
}

void ShadowRenderer::renderShadowObjects(
		video::ITexture *target, DirectionalLight &light)
{
	m_driver->setTransform(video::ETS_VIEW, light.getViewMatrix());
	m_driver->setTransform(video::ETS_PROJECTION, light.getProjectionMatrix());

	for (const auto &shadow_node : m_shadow_node_array) {
		// we only take care of the shadow casters and only visible nodes cast shadows
		if (shadow_node.shadowMode == ESM_RECEIVE || !shadow_node.node->isVisible())
			continue;

		// render other objects
		u32 n_node_materials = shadow_node.node->getMaterialCount();
		std::vector<video::E_MATERIAL_TYPE> BufferMaterialList;
		std::vector<std::pair<bool, bool>> BufferMaterialCullingList;
		std::vector<video::E_BLEND_OPERATION> BufferBlendOperationList;
		BufferMaterialList.reserve(n_node_materials);
		BufferMaterialCullingList.reserve(n_node_materials);
		BufferBlendOperationList.reserve(n_node_materials);

		// backup materialtype for each material
		// (aka shader)
		// and replace it by our "depth" shader
		for (u32 m = 0; m < n_node_materials; m++) {
			auto &current_mat = shadow_node.node->getMaterial(m);

			BufferMaterialList.push_back(current_mat.MaterialType);
			// Note: this suffers from the same misdesign and will break once we
			// start doing more special shader things for entities.
			current_mat.MaterialType = depth_shader;

			BufferMaterialCullingList.emplace_back(
				(bool)current_mat.BackfaceCulling, (bool)current_mat.FrontfaceCulling);
			current_mat.BackfaceCulling = true;
			current_mat.FrontfaceCulling = false;

			BufferBlendOperationList.push_back(current_mat.BlendOperation);
			// shouldn't we be setting EBO_MIN here?
		}

		m_driver->setTransform(video::ETS_WORLD,
				shadow_node.node->getAbsoluteTransformation());
		shadow_node.node->render();

		// restore the material.

		for (u32 m = 0; m < n_node_materials; m++) {
			auto &current_mat = shadow_node.node->getMaterial(m);

			current_mat.MaterialType = BufferMaterialList[m];

			current_mat.BackfaceCulling = BufferMaterialCullingList[m].first;
			current_mat.FrontfaceCulling = BufferMaterialCullingList[m].second;

			current_mat.BlendOperation = BufferBlendOperationList[m];
		}

	} // end for caster shadow nodes
}

void ShadowRenderer::createShaders()
{
	auto *shdsrc = m_client->getShaderSource();

	assert(m_shadow_depth_cb.empty());

	ShaderConstants a_const;
	a_const["USE_ARRAY_TEXTURE"] = 1;

	{
		auto *cb = new ShadowDepthUniformSetter();
		m_shadow_depth_cb.push_back(cb);
		u32 shader_id = shdsrc->getShader("shadow/pass1", {},
			video::EMT_SOLID, cb);
		depth_shader = shdsrc->getShaderInfo(shader_id).material;
	}

	if (shdsrc->supportsSampler2DArray()) {
		auto *cb = new ShadowDepthUniformSetter();
		m_shadow_depth_cb.push_back(cb);
		u32 shader_id = shdsrc->getShader("shadow/pass1", a_const,
			video::EMT_SOLID, cb);
		depth_shader_a = shdsrc->getShaderInfo(shader_id).material;
	}

	if (m_shadow_map_colored) {
		auto *cb = new ShadowDepthUniformSetter();
		m_shadow_depth_cb.push_back(cb);
		u32 shader_id = shdsrc->getShader("shadow/pass1_trans", {},
			video::EMT_SOLID, cb);
		depth_shader_trans = shdsrc->getShaderInfo(shader_id).material;
	}

	if (m_shadow_map_colored && shdsrc->supportsSampler2DArray()) {
		auto *cb = new ShadowDepthUniformSetter();
		m_shadow_depth_cb.push_back(cb);
		u32 shader_id = shdsrc->getShader("shadow/pass1_trans", a_const,
			video::EMT_SOLID, cb);
		depth_shader_trans_a = shdsrc->getShaderInfo(shader_id).material;
	}

	{
		auto *shadow_mix_cb = new ShadowScreenQuadUniformSetter();
		u32 shader_id = shdsrc->getShader("shadow/pass2", {},
			video::EMT_SOLID, shadow_mix_cb);
		shadow_mix_cb->drop();
		m_screen_quad->getMaterial().MaterialType =
			shdsrc->getShaderInfo(shader_id).material;
	}
}

std::unique_ptr<ShadowRenderer> createShadowRenderer(IrrlichtDevice *device, Client *client)
{
	if (!g_settings->getBool("enable_dynamic_shadows"))
		return nullptr;

	auto renderer = std::make_unique<ShadowRenderer>(device, client);
	if (!renderer->initialize()) {
		warningstream << "Disabling dynamic shadows due to being unsupported." << std::endl;
		renderer.reset();
	}
	return renderer;
}

bool ShadowRenderer::isSupported(video::IVideoDriver *driver)
{
	const video::E_DRIVER_TYPE type = driver->getDriverType();
	v2s32 glver = driver->getLimits().GLVersion;

	if (type != video::EDT_OPENGL && type != video::EDT_OPENGL3 &&
			!(type == video::EDT_OGLES2 && glver.X >= 3))
		return false;

	if (!driver->queryFeature(video::EVDF_RENDER_TO_FLOAT_TEXTURE))
		return false;

	return true;
}
