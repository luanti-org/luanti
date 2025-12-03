// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti developers

#include "sound_spec.h"

#include "util/serialize.h"

void SoundSpec::serializeSimple(std::ostream &os, u16 protocol_version) const
{
	os << serializeString16(name);
	writeF32(os, gain);
	writeF32(os, pitch);
	writeF32(os, fade);
}

void SoundSpec::deSerializeSimple(std::istream &is, u16 protocol_version)
{
	name = deSerializeString16(is);
	gain = readF32(is);
	pitch = readF32(is);
	fade = readF32(is);
}
