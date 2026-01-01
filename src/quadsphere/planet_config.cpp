// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Planet configuration implementation

#include "planet_config.h"
#include "quadsphere.h"

namespace quadsphere {

// Global planet configuration instance
PlanetConfig g_planet_config;

// Whether planet mode is enabled (default false for compatibility)
bool g_planet_mode_enabled = false;

v3f PlanetConfig::getSurfacePosition(CubeFace face, f32 u, f32 v) const
{
	return cubeToWorld(face, u, v, radius, 0.0f) + center;
}

} // namespace quadsphere
