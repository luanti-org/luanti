// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <set>
#include <string>
#include "util/serialize.h"
#include "irrlichttypes_bloated.h"

/**
 * Describes the sound information for playback.
 * Positional handling is done separately.
 *
 * `SimpleSoundSpec`, as used by modding, is a `SoundSpec` with only name, fain,
 * pitch and fade.
*/
struct SoundSpec
{
	SoundSpec(std::string_view name = "", float gain = 1.0f,
			bool loop = false, float fade = 0.0f, float pitch = 1.0f,
			float start_time = 0.0f) :
			name(name), gain(gain), fade(fade), pitch(pitch), start_time(start_time),
			loop(loop)
	{
	}

	bool exists() const { return !name.empty(); }

	/**
	 * Serialize a `SimpleSoundSpec`.
	 */
	void serializeSimple(std::ostream &os, u16 protocol_version) const
	{
		os << serializeString16(name);
		writeF32(os, gain);
		writeF32(os, pitch);
		writeF32(os, fade);
	}

	/**
	 * Deserialize a `SimpleSoundSpec`.
	 */
	void deSerializeSimple(std::istream &is, u16 protocol_version)
	{
		name = deSerializeString16(is);
		gain = readF32(is);
		pitch = readF32(is);
		fade = readF32(is);
	}

	// Name of the sound-group
	std::string name;
	float gain = 1.0f;
	float fade = 0.0f;
	float pitch = 1.0f;
	float start_time = 0.0f;
	bool loop = false;
	// If true, a local fallback (ie. from the user's sound pack) is used if the
	// sound-group does not exist.
	bool use_local_fallback = true;
};


// The order must not be changed. This is sent over the network.
enum class SoundLocation : u8 {
	Local,
	Position,
	Object
};
