// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "convert_json.h"

#include <json/json.h>

#include <iostream>
#include <sstream>
#include <memory>

void fastWriteJson(const Json::Value &value, std::ostream &to)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
	writer->write(value, &to);
}

std::string fastWriteJson(const Json::Value &value)
{
	std::ostringstream oss;
	fastWriteJson(value, oss);
	return oss.str();
}
