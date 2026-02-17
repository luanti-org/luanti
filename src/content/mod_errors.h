// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti developers

#pragma once

#include <string>

class ModErrors {
public:
	static void logErrors();
	static void setModError(const std::string &path, std::string error_message);
};
