// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "convert_json.h"
#include "util/stream.h"

#include <json/json.h>

#include <iostream>
#include <memory>

void fastWriteJson(const Json::Value &value, std::ostream &to)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	builder["emitUTF8"] = true;
	std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
	writer->write(value, &to);
}

std::string fastWriteJson(const Json::Value &value)
{
	my_ostringstream oss;
	fastWriteJson(value, oss);
	return oss.take();
}
