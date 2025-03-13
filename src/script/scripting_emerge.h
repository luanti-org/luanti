// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 sfan5 <sfan5@live.de>

#pragma once
#include "cpp_api/s_base.h"
#include "cpp_api/s_mapgen.h"
#include "cpp_api/s_security.h"

class EmergeThread;

class EmergeScripting:
		virtual public ScriptApiBase,
		public ScriptApiMapgen,
		public ScriptApiSecurity
{
public:
	EmergeScripting(EmergeThread *parent);

protected:
	bool checkPathInternal(const std::string &abs_path, bool write_required,
		bool *write_allowed) override {
		return ScriptApiSecurity::checkPathWithGamedef(getStack(),
			abs_path, write_required, write_allowed);
	};

private:
	void InitializeModApi(lua_State *L, int top);
};
