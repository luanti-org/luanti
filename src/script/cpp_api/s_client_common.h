// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
// Copyright (C) 2025 grorp

#pragma once

#include "cpp_api/s_base.h"
#include "util/string.h"

class ScriptApiClientCommon : virtual public ScriptApiBase
{
public:
	bool on_formspec_input(const std::string &formname, const StringMap &fields);
};
