// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <IrrlichtDevice.h>
#include "client/shadows/dynamicshadows.h"
#include <ISceneNode.h>
#include <ISceneManager.h>

class ShadowDepthShaderCB;
class shadowScreenQuad;
class shadowScreenQuadCB;
class IWritableShaderSource;

enum E_SHADOW_MODE : u8
{
	ESM_RECEIVE = 0,
	ESM_BOTH,
};

struct NodeToApply
{
	NodeToApply(scene::ISceneNode *n,
			E_SHADOW_MODE m = E_SHADOW_MODE::ESM_BOTH) :
			node(n),
			shadowMode(m){};

	bool operator==(scene::ISceneNode *n) const { return node == n; }

	scene::ISceneNode *node;

	E_SHADOW_MODE shadowMode{E_SHADOW_MODE::ESM_BOTH};
	bool dirty{false};
};

class ShadowRenderer
{
public:
	static const int TEXTURE_LAYER_SHADOW = 3;

	ShadowRenderer(IrrlichtDevice *device, Client *client);
	~ShadowRenderer();

	// Call before generating any shaders
	// This is required because this class is initialized much later than all
	// the shaders are dealt with.
	static void preInit(IWritableShaderSource *shsrc);

	void initialize();

	/// Adds a directional light shadow map (Usually just one (the sun) except in
	/// Tattoine ).
	size_t addDirectionalLight();
	DirectionalLight &getDirectionalLight(u32 index = 0);
	size_t getDirectionalLightCount() const;
	f32 getMaxShadowFar() const;

	/// Adds a shadow to the scene node.
	/// The shadow mode can be ESM_BOTH, or ESM_RECEIVE.
	/// ESM_BOTH casts and receives shadows
	/// ESM_RECEIVE only receives but does not cast shadows.
	///
	void addNodeToShadowList(scene::ISceneNode *node,
			E_SHADOW_MODE shadowMode = ESM_BOTH);
	void removeNodeFromShadowList(scene::ISceneNode *node);

	void update(video::ITexture *outputTarget = nullptr);
	/// Force shadow map to be re-drawn in one go next frame
	void setForceUpdateShadowMap() { m_force_update_shadow_map = true; }
	void drawDebug();

	video::ITexture *get_texture()
	{
		return shadowMapTextureFinal;
	}


	bool is_active() const { return m_shadows_enabled && shadowMapTextureFinal != nullptr; }
	void setTimeOfDay(float isDay) { m_time_day = isDay; };
	void setShadowIntensity(float shadow_intensity);
	void setShadowTint(video::SColor shadow_tint) { m_shadow_tint = shadow_tint; }

	s32 getShadowSamples() const { return m_shadow_samples; }
	float getShadowStrength() const { return m_shadows_enabled ? m_shadow_strength : 0.0f; }
	video::SColor getShadowTint() const { return m_shadow_tint; }
	float getTimeOfDay() const { return m_time_day; }

	f32 getPerspectiveBiasXY() { return m_perspective_bias_xy; }
	f32 getPerspectiveBiasZ() { return m_perspective_bias_z; }

private:
	video::ITexture *getSMTexture(const std::string &shadow_map_name,
			video::ECOLOR_FORMAT texture_format,
			bool force_creation = false);

	void renderShadowMap(video::ITexture *target, DirectionalLight &light,
			scene::E_SCENE_NODE_RENDER_PASS pass =
					scene::ESNRP_SOLID);
	void renderShadowObjects(video::ITexture *target, DirectionalLight &light);
	void mixShadowsQuad();
	void updateSMTextures();

	void disable();
	void enable() { m_shadows_enabled = m_shadows_supported; }

	// a bunch of variables
	scene::ISceneManager *m_smgr{nullptr};
	video::IVideoDriver *m_driver{nullptr};
	Client *m_client{nullptr};
	video::ITexture *shadowMapClientMap{nullptr};
	video::ITexture *shadowMapClientMapFuture{nullptr};
	video::ITexture *shadowMapTextureFinal{nullptr};
	video::ITexture *shadowMapTextureDynamicObjects{nullptr};
	video::ITexture *shadowMapTextureColors{nullptr};

	std::vector<DirectionalLight> m_light_list;
	std::vector<NodeToApply> m_shadow_node_array;

	float m_shadow_strength;
	video::SColor m_shadow_tint;
	float m_shadow_strength_gamma;
	float m_shadow_map_max_distance;
	u32 m_shadow_map_texture_size;
	float m_time_day;
	int m_shadow_samples;
	bool m_shadow_map_texture_32bit;
	bool m_shadows_enabled;
	bool m_shadows_supported;
	bool m_shadow_map_colored;
	bool m_force_update_shadow_map;
	u8 m_map_shadow_update_frames; /* Use this number of frames to update map shaodw */
	u8 m_current_frame; /* Current frame */
	f32 m_perspective_bias_xy;
	f32 m_perspective_bias_z;

	video::ECOLOR_FORMAT m_texture_format{video::ECOLOR_FORMAT::ECF_R16F};
	video::ECOLOR_FORMAT m_texture_format_color{video::ECOLOR_FORMAT::ECF_R16G16};

	// Shadow Shader stuff

	void createShaders();
	std::string readShaderFile(const std::string &path);

	s32 depth_shader{-1};
	s32 depth_shader_entities{-1};
	s32 depth_shader_trans{-1};
	s32 mixcsm_shader{-1};

	ShadowDepthShaderCB *m_shadow_depth_cb{nullptr};
	ShadowDepthShaderCB *m_shadow_depth_entity_cb{nullptr};
	ShadowDepthShaderCB *m_shadow_depth_trans_cb{nullptr};

	shadowScreenQuad *m_screen_quad{nullptr};
	shadowScreenQuadCB *m_shadow_mix_cb{nullptr};
};

/**
 * @brief Create a shadow renderer if settings allow this.
 *
 * @param device Device to be used to render shadows.
 * @param client Reference to the client context.
 * @return A new ShadowRenderer instance or nullptr if shadows are disabled or not supported.
 */
std::unique_ptr<ShadowRenderer> createShadowRenderer(IrrlichtDevice *device, Client *client);
