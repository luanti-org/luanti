// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#include <cstring>
#include <cmath>
#include "client/shadows/dynamicshadowsrender.h"
#include "client/shadows/shadowsScreenQuad.h"
#include "client/shadows/shadowsshadercallbacks.h"
#include "settings.h"
#include "filesys.h"
#include "util/string.h"
#include "client/shader.h"
#include "client/client.h"
#include "client/clientmap.h"
#include "profiler.h"
#include "IGPUProgrammingServices.h"
#include "IMaterialRenderer.h"
#include "IVideoDriver.h"

ShadowRenderer::ShadowRenderer(IrrlichtDevice *device, Client *client) :
		m_smgr(device->getSceneManager()), m_driver(device->getVideoDriver()),
		m_client(client), m_shadow_strength(0.0f), m_shadow_tint(255, 0, 0, 0),
		m_time_day(0.0f), m_force_update_shadow_map(false), m_current_frame(0),
		m_perspective_bias_xy(0.8f), m_perspective_bias_z(0.5f)
{
	(void) m_client;

	m_shadows_supported = true; // assume shadows supported. We will check actual support in initialize
	m_shadows_enabled = true;

	m_shadow_strength_gamma = g_settings->getFloat("shadow_strength_gamma");
	if (std::isnan(m_shadow_strength_gamma))
		m_shadow_strength_gamma = 1.0f;
	m_shadow_strength_gamma = core::clamp(m_shadow_strength_gamma, 0.1f, 10.0f);

	m_shadow_map_max_distance = g_settings->getFloat("shadow_map_max_distance");

	m_shadow_map_texture_size = g_settings->getU32("shadow_map_texture_size");

	m_shadow_map_texture_32bit = g_settings->getBool("shadow_map_texture_32bit");
	m_shadow_map_colored = g_settings->getBool("shadow_map_color");
	m_shadow_samples = g_settings->getS32("shadow_filters");
	m_map_shadow_update_frames = g_settings->getS16("shadow_update_frames");

	// add at least one light
	addDirectionalLight();
}

ShadowRenderer::~ShadowRenderer()
{
	// call to disable releases dynamically allocated resources
	disable();

	if (m_shadow_depth_cb)
		delete m_shadow_depth_cb;
	if (m_shadow_depth_entity_cb)
		delete m_shadow_depth_entity_cb;
	if (m_shadow_depth_trans_cb)
		delete m_shadow_depth_trans_cb;
	if (m_shadow_mix_cb)
		delete m_shadow_mix_cb;
	m_shadow_node_array.clear();
	m_light_list.clear();
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

	for (auto node : m_shadow_node_array)
		node.node->forEachMaterial([] (auto &mat) {
			mat.setTexture(TEXTURE_LAYER_SHADOW, nullptr);
		});
}

void ShadowRenderer::preInit(IWritableShaderSource *shsrc)
{
	if (g_settings->getBool("enable_dynamic_shadows")) {
		shsrc->addShaderUniformSetterFactory(std::make_unique<ShadowUniformSetterFactory>());
	}
}

void ShadowRenderer::initialize()
{
	createShaders();


	m_texture_format = m_shadow_map_texture_32bit
					   ? video::ECOLOR_FORMAT::ECF_R32F
					   : video::ECOLOR_FORMAT::ECF_R16F;

	m_texture_format_color = m_shadow_map_texture_32bit
						 ? video::ECOLOR_FORMAT::ECF_G32R32F
						 : video::ECOLOR_FORMAT::ECF_G16R16F;

	m_shadows_enabled &= m_shadows_supported;
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

	// The merge all shadowmaps texture
	if (!shadowMapTextureFinal) {
		video::ECOLOR_FORMAT frt;
		if (m_shadow_map_texture_32bit) {
			if (m_shadow_map_colored)
				frt = video::ECOLOR_FORMAT::ECF_A32B32G32R32F;
			else
				frt = video::ECOLOR_FORMAT::ECF_R32F;
		} else {
			if (m_shadow_map_colored)
				frt = video::ECOLOR_FORMAT::ECF_A16B16G16R16F;
			else
				frt = video::ECOLOR_FORMAT::ECF_R16F;
		}
		shadowMapTextureFinal = getSMTexture(
			std::string("shadowmap_final_") + itos(m_shadow_map_texture_size),
			frt, true);
		assert(shadowMapTextureFinal != nullptr);

		for (auto &node : m_shadow_node_array)
			node.node->forEachMaterial([this] (auto &mat) {
				mat.setTexture(TEXTURE_LAYER_SHADOW, shadowMapTextureFinal);
			});
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
			for (auto cb : {m_shadow_depth_cb, m_shadow_depth_entity_cb, m_shadow_depth_trans_cb}) {
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
						m_driver->setRenderTarget(0, false, false);
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
			m_shadow_depth_entity_cb->CameraPos = light.getPlayerPos();

			// render shadows for the n0n-map objects.
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
	m_driver->setTransform(video::ETS_VIEW, light.getFutureViewMatrix());
	m_driver->setTransform(video::ETS_PROJECTION, light.getFutureProjectionMatrix());

	ClientMap &map_node = static_cast<ClientMap &>(m_client->getEnv().getMap());

	video::SMaterial material;
	if (map_node.getMaterialCount() > 0) {
		// we only want the first material, which is the one with the albedo info
		material = map_node.getMaterial(0);
	}

	material.BackfaceCulling = false;
	material.FrontfaceCulling = true;

	if (m_shadow_map_colored && pass != scene::ESNRP_SOLID) {
		material.MaterialType = (video::E_MATERIAL_TYPE) depth_shader_trans;
	}
	else {
		material.MaterialType = (video::E_MATERIAL_TYPE) depth_shader;
		material.BlendOperation = video::EBO_MIN;
	}

	m_driver->setTransform(video::ETS_WORLD,
			map_node.getAbsoluteTransformation());

	int frame = m_force_update_shadow_map ? 0 : m_current_frame;
	int total_frames = m_force_update_shadow_map ? 1 : m_map_shadow_update_frames;

	map_node.renderMapShadows(m_driver, material, pass, frame, total_frames);
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
		std::vector<s32> BufferMaterialList;
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
			current_mat.MaterialType =
					(video::E_MATERIAL_TYPE)depth_shader_entities;

			BufferMaterialCullingList.emplace_back(
				(bool)current_mat.BackfaceCulling, (bool)current_mat.FrontfaceCulling);
			BufferBlendOperationList.push_back(current_mat.BlendOperation);

			current_mat.BackfaceCulling = true;
			current_mat.FrontfaceCulling = false;
		}

		m_driver->setTransform(video::ETS_WORLD,
				shadow_node.node->getAbsoluteTransformation());
		shadow_node.node->render();

		// restore the material.

		for (u32 m = 0; m < n_node_materials; m++) {
			auto &current_mat = shadow_node.node->getMaterial(m);

			current_mat.MaterialType = (video::E_MATERIAL_TYPE) BufferMaterialList[m];

			current_mat.BackfaceCulling = BufferMaterialCullingList[m].first;
			current_mat.FrontfaceCulling = BufferMaterialCullingList[m].second;
			current_mat.BlendOperation = BufferBlendOperationList[m];
		}

	} // end for caster shadow nodes
}

void ShadowRenderer::mixShadowsQuad()
{
}

/*
 * @Liso's disclaimer ;) This function loads the Shadow Mapping Shaders.
 * I used a custom loader because I couldn't figure out how to use the base
 * Shaders system with custom IShaderConstantSetCallBack without messing up the
 * code too much. If anyone knows how to integrate this with the standard MT
 * shaders, please feel free to change it.
 *
 * TODO: as of now (2025) it should be possible to hook these up to the normal
 * shader system.
 */

void ShadowRenderer::createShaders()
{
	auto *gpu = m_driver->getGPUProgrammingServices();

	if (depth_shader == -1) {
		std::string depth_shader_vs = getShaderPath("shadow_shaders", "pass1_vertex.glsl");
		if (depth_shader_vs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error shadow mapping vs shader not found." << std::endl;
			return;
		}
		std::string depth_shader_fs = getShaderPath("shadow_shaders", "pass1_fragment.glsl");
		if (depth_shader_fs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error shadow mapping fs shader not found." << std::endl;
			return;
		}
		m_shadow_depth_cb = new ShadowDepthShaderCB();

		depth_shader = gpu->addHighLevelShaderMaterial(
				readShaderFile(depth_shader_vs).c_str(),
				readShaderFile(depth_shader_fs).c_str(), nullptr,
				m_shadow_depth_cb, video::EMT_ONETEXTURE_BLEND);

		if (depth_shader == -1) {
			// upsi, something went wrong loading shader.
			delete m_shadow_depth_cb;
			m_shadow_depth_cb = nullptr;
			m_shadows_enabled = false;
			m_shadows_supported = false;
			errorstream << "Error compiling shadow mapping shader." << std::endl;
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		m_driver->getMaterialRenderer(depth_shader)->grab();
	}

	// This creates a clone of depth_shader with base material set to EMT_SOLID,
	// because entities won't render shadows with base material EMP_ONETEXTURE_BLEND
	if (depth_shader_entities == -1) {
		std::string depth_shader_vs = getShaderPath("shadow_shaders", "pass1_vertex.glsl");
		if (depth_shader_vs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error shadow mapping vs shader not found." << std::endl;
			return;
		}
		std::string depth_shader_fs = getShaderPath("shadow_shaders", "pass1_fragment.glsl");
		if (depth_shader_fs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error shadow mapping fs shader not found." << std::endl;
			return;
		}
		m_shadow_depth_entity_cb = new ShadowDepthShaderCB();

		depth_shader_entities = gpu->addHighLevelShaderMaterial(
				readShaderFile(depth_shader_vs).c_str(),
				readShaderFile(depth_shader_fs).c_str(), nullptr,
				m_shadow_depth_entity_cb);

		if (depth_shader_entities == -1) {
			// upsi, something went wrong loading shader.
			delete m_shadow_depth_entity_cb;
			m_shadow_depth_entity_cb = nullptr;
			m_shadows_enabled = false;
			m_shadows_supported = false;
			errorstream << "Error compiling shadow mapping shader (dynamic)." << std::endl;
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		m_driver->getMaterialRenderer(depth_shader_entities)->grab();
	}

	if (mixcsm_shader == -1) {
		std::string depth_shader_vs = getShaderPath("shadow_shaders", "pass2_vertex.glsl");
		if (depth_shader_vs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error cascade shadow mapping fs shader not found." << std::endl;
			return;
		}

		std::string depth_shader_fs = getShaderPath("shadow_shaders", "pass2_fragment.glsl");
		if (depth_shader_fs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error cascade shadow mapping fs shader not found." << std::endl;
			return;
		}
		m_shadow_mix_cb = new shadowScreenQuadCB();
		m_screen_quad = new shadowScreenQuad();
		mixcsm_shader = gpu->addHighLevelShaderMaterial(
				readShaderFile(depth_shader_vs).c_str(),
				readShaderFile(depth_shader_fs).c_str(), nullptr,
				m_shadow_mix_cb);

		m_screen_quad->getMaterial().MaterialType =
				(video::E_MATERIAL_TYPE)mixcsm_shader;

		if (mixcsm_shader == -1) {
			// upsi, something went wrong loading shader.
			delete m_shadow_mix_cb;
			delete m_screen_quad;
			m_shadows_supported = false;
			errorstream << "Error compiling cascade shadow mapping shader." << std::endl;
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		m_driver->getMaterialRenderer(mixcsm_shader)->grab();
	}

	if (m_shadow_map_colored && depth_shader_trans == -1) {
		std::string depth_shader_vs = getShaderPath("shadow_shaders", "pass1_trans_vertex.glsl");
		if (depth_shader_vs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error shadow mapping vs shader not found." << std::endl;
			return;
		}
		std::string depth_shader_fs = getShaderPath("shadow_shaders", "pass1_trans_fragment.glsl");
		if (depth_shader_fs.empty()) {
			m_shadows_supported = false;
			errorstream << "Error shadow mapping fs shader not found." << std::endl;
			return;
		}
		m_shadow_depth_trans_cb = new ShadowDepthShaderCB();

		depth_shader_trans = gpu->addHighLevelShaderMaterial(
				readShaderFile(depth_shader_vs).c_str(),
				readShaderFile(depth_shader_fs).c_str(), nullptr,
				m_shadow_depth_trans_cb);

		if (depth_shader_trans == -1) {
			// upsi, something went wrong loading shader.
			delete m_shadow_depth_trans_cb;
			m_shadow_depth_trans_cb = nullptr;
			m_shadow_map_colored = false;
			m_shadows_supported = false;
			errorstream << "Error compiling colored shadow mapping shader." << std::endl;
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		m_driver->getMaterialRenderer(depth_shader_trans)->grab();
	}
}

std::string ShadowRenderer::readShaderFile(const std::string &path)
{
	std::string prefix;
	if (m_shadow_map_colored)
		prefix.append("#define COLORED_SHADOWS 1\n");
	prefix.append("#line 0\n");

	std::string content;
	if (!fs::ReadFile(path, content, true))
		return "";

	return prefix + content;
}

std::unique_ptr<ShadowRenderer> createShadowRenderer(IrrlichtDevice *device, Client *client)
{
	if (!g_settings->getBool("enable_dynamic_shadows"))
		return nullptr;

	// disable if unsupported
	// See also checks in builtin/mainmenu/settings/dlg_settings.lua
	const video::E_DRIVER_TYPE type = device->getVideoDriver()->getDriverType();
	if (type != video::EDT_OPENGL && type != video::EDT_OPENGL3) {
		warningstream << "Shadows: disabled dynamic shadows due to being unsupported" << std::endl;
		g_settings->setBool("enable_dynamic_shadows", false);
		return nullptr;
	}

	auto shadow_renderer = std::make_unique<ShadowRenderer>(device, client);
	shadow_renderer->initialize();
	return shadow_renderer;
}
