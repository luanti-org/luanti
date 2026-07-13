// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dynamiclight.h"
#include "camera.h"
#include <algorithm>

void DynamicLightManager::addOrUpdate(u32 id, v3f pos, float radius, video::SColorf color)
{
	DynamicLight &light = m_lights[id];
	light.id = id;
	light.pos = pos;
	light.radius = radius;
	light.color = color;
}

void DynamicLightManager::remove(u32 id)
{
	m_lights.erase(id);
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

	if (m_visible_lights.size() > MAX_DYNAMIC_LIGHTS) {
		v3f cam_pos = camera.getPosition();
		std::nth_element(m_visible_lights.begin(),
				m_visible_lights.begin() + MAX_DYNAMIC_LIGHTS,
				m_visible_lights.end(),
				[&cam_pos](const DynamicLight &a, const DynamicLight &b) {
					return a.pos.getDistanceFromSQ(cam_pos) < b.pos.getDistanceFromSQ(cam_pos);
				});
		m_visible_lights.resize(MAX_DYNAMIC_LIGHTS);
	}
}
