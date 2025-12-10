// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2025 Luanti developers

#pragma once

#include "settings.h"
#include <string>


// WARNING: Keep this file lightweight. It is also used by guiFormSpecMenu !


enum class DeprecatedHandlingMode {
	Ignore,
	Log,
	Error
};

/**
 * Reads `deprecated_lua_api_handling` in settings, returns cached value.
 */
static DeprecatedHandlingMode get_deprecated_handling_mode()
{
	static thread_local bool configured = false;
	static thread_local DeprecatedHandlingMode ret = DeprecatedHandlingMode::Ignore;

	// Only read settings on first call
	if (!configured) {
		std::string value = g_settings->get("deprecated_lua_api_handling");
		if (value == "log") {
			ret = DeprecatedHandlingMode::Log;
		} else if (value == "error") {
			ret = DeprecatedHandlingMode::Error;
		}
		configured = true;
	}

	return ret;
}
