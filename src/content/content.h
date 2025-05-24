// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 rubenwardy <rw@rubenwardy.com>

#pragma once
#include "irrlichttypes.h"
#include <string>

enum class ContentType
{
	UNKNOWN,
	MOD,
	MODPACK,
	GAME,
	TXP
};


struct ContentSpec
{
	std::string type;
	std::string author;
	u32 release = 0;

	/// Technical name / Id
	std::string name;

	/// Human-readable title
	std::string title;

	/// Short description
	std::string desc;
	std::string path;
	std::string textdomain;
};


ContentType getContentType(const std::string &path);
void parseContentInfo(ContentSpec &spec);
