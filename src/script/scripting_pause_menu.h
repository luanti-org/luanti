// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 grorp

#pragma once

#include "cpp_api/s_base.h"
#include "cpp_api/s_client_common.h"
#include "cpp_api/s_pause_menu.h"
#include "cpp_api/s_security.h"

class PauseMenuScripting:
		virtual public ScriptApiBase,
		public ScriptApiPauseMenu,
		public ScriptApiClientCommon,
		public ScriptApiSecurity
{
public:
	PauseMenuScripting(Client *client);
	void loadBuiltin();

protected:
	bool checkPathInternal(const std::string &abs_path, bool write_required,
			bool *write_allowed) override;

private:
	void initializeModApi(lua_State *L, int top);
};
