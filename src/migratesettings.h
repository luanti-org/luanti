// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "settings.h"
#include "server.h"

void migrate_settings()
{
	// Converts opaque_water to translucent_liquids
	if (g_settings->existsLocal("opaque_water")) {
		g_settings->set("translucent_liquids",
				g_settings->getBool("opaque_water") ? "false" : "true");
		g_settings->remove("opaque_water");
	}

	// Converts enable_touch to touch_controls/touch_gui
	if (g_settings->existsLocal("enable_touch")) {
		bool value = g_settings->getBool("enable_touch");
		g_settings->setBool("touch_controls", value);
		g_settings->setBool("touch_gui", value);
		g_settings->remove("enable_touch");
	}

	// Disables anticheat
	if (g_settings->existsLocal("disable_anticheat")) {
		if (g_settings->getBool("disable_anticheat")) {
			g_settings->setFlagStr("anticheat_flags", 0, flagdesc_anticheat);
		}
		g_settings->remove("disable_anticheat");
	}

	// Convert touch_use_crosshair to touch_interaction_style
	if (g_settings->existsLocal("touch_use_crosshair")) {
		bool value = g_settings->getBool("touch_use_crosshair");
		g_settings->set("touch_interaction_style", value ? "tap_crosshair" : "tap");
		g_settings->remove("touch_use_crosshair");
	}
}
