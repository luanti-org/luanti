// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#include "dynamiclight.h"
#include "camera.h"
#include "settings.h"
#include <algorithm>

void DynamicLightManager::addOrUpdate(u32 id, v3f pos, float radius, video::SColorf color, float falloff)
{
	DynamicLight &light = m_lights[id];
	light.id = id;
	light.pos = pos;
	light.radius = radius;
	light.color = color;
	light.falloff = falloff;
}

void DynamicLightManager::remove(u32 id)
{
	m_lights.erase(id);
}

size_t getDynamicLightsLimitSetting()
{
	return std::clamp<size_t>(g_settings->getU16("dynamic_lights_limit"),
			0, DYNAMIC_LIGHTS_CEILING);
}

size_t DynamicLightManager::getEffectiveLimit()
{
	if (!m_limit_resolved) {
		m_limit = getDynamicLightsLimitSetting();
		m_limit_resolved = true;
	}
	return m_limit;
}

void DynamicLightManager::cull(const Camera &camera)
{
	m_visible_lights.clear();

	auto is_culled = camera.getFrustumCuller();
	for (const auto &it : m_lights) {
		const DynamicLight &light = it.second;
		if (!is_culled(light.pos, light.radius))
			m_visible_lights.push_back(light);
	}

	size_t limit = getEffectiveLimit();
	if (m_visible_lights.size() > limit) {
		v3f cam_pos = camera.getPosition();
		std::nth_element(m_visible_lights.begin(),
				m_visible_lights.begin() + limit,
				m_visible_lights.end(),
				[&cam_pos](const DynamicLight &a, const DynamicLight &b) {
					return a.pos.getDistanceFromSQ(cam_pos) < b.pos.getDistanceFromSQ(cam_pos);
				});
		m_visible_lights.resize(limit);
	}
}
