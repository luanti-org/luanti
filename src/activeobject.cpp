// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "activeobject.h"
#include "util/serialize.h"

void ActiveObjectMessage::appendTo(std::string &data) const
{
	char idbuf[2];
	writeU16((u8*) idbuf, id);
	data.append(idbuf, sizeof(idbuf));
	data.append(serializeString16(datastring));
}
