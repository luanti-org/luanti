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

void AttachmentData::serialize(std::ostream &os) const
{
	// command
	writeU8(os, AO_CMD_ATTACH_TO);
	// parameters
	writeS16(os, parent_id);
	os << serializeString16(bone);
	writeV3F32(os, position);
	writeV3F32(os, rotation);
	writeU8(os, flags);
}

void AttachmentData::deSerialize(std::istream &is)
{
	parent_id = readS16(is);
	bone = deSerializeString16(is);
	position = readV3F32(is);
	rotation = readV3F32(is);

	// can read if the client version >= 5.4.0-dev
	flags = canRead(is) ? readU8(is) : false;
}
