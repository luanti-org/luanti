// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 grorp

#include "s_pause_menu.h"
#include "cpp_api/s_internal.h"

void ScriptApiPauseMenu::open_settings()
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "open_settings");

	PCALL_RES(lua_pcall(L, 0, 0, error_handler));

	lua_pop(L, 2); // Pop core, error handler
}
