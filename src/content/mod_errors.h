// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>

class ModErrors {
public:
	static void logErrors();
	static void setModError(const std::string &path, std::string error_message);
};
