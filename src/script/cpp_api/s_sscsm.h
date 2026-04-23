// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "cpp_api/s_base.h"

class ScriptApiSSCSM : virtual public ScriptApiBase
{
public:
	void load_mods(const std::vector<std::pair<std::string, std::string>> &mods);

	void environment_step(float dtime);

	void set_content_defs(const std::vector<std::pair<u16, std::string>> &defs);

	void on_modchannel_message(const std::string &channel,
			const std::string &sender, const std::string &message);
	void on_modchannel_signal(const std::string &channel, u8 signal);
};
