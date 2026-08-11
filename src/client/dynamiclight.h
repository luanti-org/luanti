// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include <SColor.h>
#include <unordered_map>
#include <vector>

class Camera;

// Must match MAX_DYNAMIC_LIGHTS in nodes_shader/object_shader opengl_fragment.glsl.
// This is a hard ceiling on the array size. The actual number of lights
// rendered is further capped at runtime by the dynamic_lights_limit setting.
constexpr size_t MAX_DYNAMIC_LIGHTS = 20;

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

	// Selects the nearest lights intersecting the frustum out of the full pool,
	// up to the dynamic_lights_limit setting.
	void cull(const Camera &camera);

	const std::vector<DynamicLight> &getVisibleLights() const { return m_visible_lights; }

private:
	std::unordered_map<u32, DynamicLight> m_lights;
	std::vector<DynamicLight> m_visible_lights;
};
