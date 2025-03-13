// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022-3 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "config.h"


struct ClientDynamicInfo
{
public:
	v2u32 render_target_size;
	f32 real_gui_scaling;
	f32 real_hud_scaling;
	v2f32 max_fs_size;
	bool touch_controls;

	bool equal(const ClientDynamicInfo &other) const {
		return render_target_size == other.render_target_size &&
				std::abs(real_gui_scaling - other.real_gui_scaling) < 0.001f &&
				std::abs(real_hud_scaling - other.real_hud_scaling) < 0.001f &&
				touch_controls == other.touch_controls;
	}

#if CHECK_CLIENT_BUILD()
	static ClientDynamicInfo getCurrent();

private:
	static v2f32 calculateMaxFSSize(v2u32 render_target_size, f32 density, f32 gui_scaling);
#endif
};
