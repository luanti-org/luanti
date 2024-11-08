// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "json-forwards.h"
#include <ostream>

void fastWriteJson(const Json::Value &value, std::ostream &to);

std::string fastWriteJson(const Json::Value &value);
