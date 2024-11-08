// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "al_helpers.h"

namespace sound {

/*
 * RAIIALSoundBuffer
 */

RAIIALSoundBuffer &RAIIALSoundBuffer::operator=(RAIIALSoundBuffer &&other) noexcept
{
	if (&other != this)
		reset(other.release());
	return *this;
}

void RAIIALSoundBuffer::reset(ALuint buf) noexcept
{
	if (m_buffer != 0) {
		alDeleteBuffers(1, &m_buffer);
		warn_if_al_error("Failed to free sound buffer");
	}

	m_buffer = buf;
}

RAIIALSoundBuffer RAIIALSoundBuffer::generate() noexcept
{
	ALuint buf;
	alGenBuffers(1, &buf);
	return RAIIALSoundBuffer(buf);
}

} // namespace sound
