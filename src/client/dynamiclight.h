// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes_bloated.h"
#include <SColor.h>
#include <unordered_map>
#include <vector>

class Camera;

// Must match MAX_DYNAMIC_LIGHTS in nodes_shader/object_shader opengl_fragment.glsl.
constexpr size_t MAX_DYNAMIC_LIGHTS = 8;

struct DynamicLight
{
	u32 id = 0;
	v3f pos;
	float radius = 0.0f;
	video::SColorf color;
	// Exponent shaping the radial falloff curve, brighten = t^falloff.
	// Higher values keep the light near-full-strength longer before dropping off.
	float falloff = 2.0f;
};

// Client-side, purely additive point lights on top of the real baked lighting
class DynamicLightManager
{
public:
	void addOrUpdate(u32 id, v3f pos, float radius, video::SColorf color, float falloff = 2.0f);
	void remove(u32 id);

	// Selects the nearest MAX_DYNAMIC_LIGHTS lights intersecting the frustum out of the full pool
	void cull(const Camera &camera);

	const std::vector<DynamicLight> &getVisibleLights() const { return m_visible_lights; }

private:
	std::unordered_map<u32, DynamicLight> m_lights;
	std::vector<DynamicLight> m_visible_lights;
};
