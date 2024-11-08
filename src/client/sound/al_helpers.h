// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#pragma once

#include "log.h"
#include "util/basic_macros.h"
#include "irr_v3d.h"

#if defined(_WIN32)
	#include <al.h>
	#include <alc.h>
	//#include <alext.h>
#elif defined(__APPLE__)
	#define OPENAL_DEPRECATED
	#include <OpenAL/al.h>
	#include <OpenAL/alc.h>
	//#include <OpenAL/alext.h>
#else
	#include <AL/al.h>
	#include <AL/alc.h>
	#include <AL/alext.h>
#endif

#include <utility>

namespace sound {

inline const char *getAlErrorString(ALenum err) noexcept
{
	switch (err) {
	case AL_NO_ERROR:
		return "no error";
	case AL_INVALID_NAME:
		return "invalid name";
	case AL_INVALID_ENUM:
		return "invalid enum";
	case AL_INVALID_VALUE:
		return "invalid value";
	case AL_INVALID_OPERATION:
		return "invalid operation";
	case AL_OUT_OF_MEMORY:
		return "out of memory";
	default:
		return "<unknown OpenAL error>";
	}
}

inline ALenum warn_if_al_error(const char *desc)
{
	ALenum err = alGetError();
	if (err == AL_NO_ERROR)
		return err;
	warningstream << "[OpenAL Error] " << desc << ": " << getAlErrorString(err)
			<< std::endl;
	return err;
}

/**
 * Transforms vectors from a left-handed coordinate system to a right-handed one
 * and vice-versa.
 * (Needed because Minetest uses a left-handed one and OpenAL a right-handed one.)
 */
inline v3f swap_handedness(v3f v) noexcept
{
	return v3f(-v.X, v.Y, v.Z);
}

/**
 * RAII wrapper for openal sound buffers.
 */
struct RAIIALSoundBuffer final
{
	RAIIALSoundBuffer() noexcept = default;
	explicit RAIIALSoundBuffer(ALuint buffer) noexcept : m_buffer(buffer) {};

	~RAIIALSoundBuffer() noexcept { reset(0); }

	DISABLE_CLASS_COPY(RAIIALSoundBuffer)

	RAIIALSoundBuffer(RAIIALSoundBuffer &&other) noexcept : m_buffer(other.release()) {}
	RAIIALSoundBuffer &operator=(RAIIALSoundBuffer &&other) noexcept;

	ALuint get() noexcept { return m_buffer; }

	ALuint release() noexcept { return std::exchange(m_buffer, 0); }

	void reset(ALuint buf) noexcept;

	static RAIIALSoundBuffer generate() noexcept;

private:
	// According to openal specification:
	// > Deleting buffer name 0 is a legal NOP.
	//
	// and:
	// > [...] the NULL buffer (i.e., 0) which can always be queued.
	ALuint m_buffer = 0;
};

} // namespace sound
