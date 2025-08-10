// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "clientsimpleobject.h"

namespace scene
{
	class ISceneManager;
}

class ClientEnvironment;

ClientSimpleObject* createSmokePuff(scene::ISceneManager *smgr,
		ClientEnvironment *env, v3f pos, v2f size);
