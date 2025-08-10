// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"

class ClientSimpleObject
{
protected:
public:
	bool m_to_be_removed = false;

	ClientSimpleObject() = default;
	virtual ~ClientSimpleObject() = default;

	virtual void step(float dtime) {}
};
