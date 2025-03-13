// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "cpp_api/s_base.h"

enum ModChannelSignal : u8;

class ScriptApiModChannels : virtual public ScriptApiBase
{
public:
	void on_modchannel_message(const std::string &channel, const std::string &sender,
			const std::string &message);
	void on_modchannel_signal(const std::string &channel, ModChannelSignal signal);
};
