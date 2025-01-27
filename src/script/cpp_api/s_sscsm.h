// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "cpp_api/s_base.h"

class ScriptApiSSCSM : virtual public ScriptApiBase
{
public:
	void load_mods(const std::vector<std::string> &init_paths);

	void environment_step(float dtime);
};
