// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#pragma once

#include <string>
#include "core.h"

RenderingCore *createRenderingCore(const std::string &stereo_mode, IrrlichtDevice *device,
		Client *client, Hud *hud);
