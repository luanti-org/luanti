// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022-3 rubenwardy <rw@rubenwardy.com>

#include "clientdynamicinfo.h"

#if CHECK_CLIENT_BUILD()

#include "settings.h"
#include "client/renderingengine.h"
#include "gui/guiFormSpecMenu.h"
#include "gui/touchcontrols.h"

ClientDynamicInfo ClientDynamicInfo::getCurrent()
{
	v2u32 screen_size = RenderingEngine::getWindowSize();
	f32 density = RenderingEngine::getDisplayDensity();
	f32 gui_scaling = g_settings->getFloat("gui_scaling", 0.5f, 20.0f);
	f32 hud_scaling = g_settings->getFloat("hud_scaling", 0.5f, 20.0f);
	f32 real_gui_scaling = gui_scaling * density;
	f32 real_hud_scaling = hud_scaling * density;
	bool touch_controls = g_touchcontrols;

	return {
			screen_size, real_gui_scaling, real_hud_scaling,
			ClientDynamicInfo::calculateMaxFSSize(screen_size, density, gui_scaling),
			touch_controls
	};
}

v2f32 ClientDynamicInfo::calculateMaxFSSize(v2u32 render_target_size, f32 density, f32 gui_scaling)
{
	// must stay in sync with GUIFormSpecMenu::calculateImgsize

	const double screen_dpi = density * 96;

	// assume padding[0,0] since max_formspec_size is used for fullscreen formspecs
	double prefer_imgsize = GUIFormSpecMenu::getImgsize(render_target_size,
			screen_dpi, gui_scaling);
	return v2f32(render_target_size.X / prefer_imgsize,
			render_target_size.Y / prefer_imgsize);
}

#endif
