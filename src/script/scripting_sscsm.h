// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "cpp_api/s_base.h"
#include "cpp_api/s_sscsm.h"
#include "cpp_api/s_security.h"

class SSCSMScripting :
		virtual public ScriptApiBase,
		public ScriptApiSSCSM,
		public ScriptApiSecurity
{
public:
	SSCSMScripting(SSCSMEnvironment *env);

protected:
	bool checkPathInternal(const std::string &abs_path, bool write_required,
		bool *write_allowed) { return false; };

private:
	void initializeModApi(lua_State *L, int top);
};
