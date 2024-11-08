// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include <string>
#include <ctime>

enum ChatMessageType
{
	CHATMESSAGE_TYPE_RAW = 0,
	CHATMESSAGE_TYPE_NORMAL = 1,
	CHATMESSAGE_TYPE_ANNOUNCE = 2,
	CHATMESSAGE_TYPE_SYSTEM = 3,
	CHATMESSAGE_TYPE_MAX = 4,
};

struct ChatMessage
{
	ChatMessage(const std::wstring &m = L"") : message(m) {}

	ChatMessage(ChatMessageType t, const std::wstring &m, const std::wstring &s = L"",
			std::time_t ts = std::time(0)) :
			type(t),
			message(m), sender(s), timestamp(ts)
	{
	}

	ChatMessageType type = CHATMESSAGE_TYPE_RAW;
	std::wstring message = L"";
	std::wstring sender = L"";
	std::time_t timestamp = std::time(0);
};
