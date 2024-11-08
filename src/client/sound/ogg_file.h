// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#pragma once

#include "al_helpers.h"
#include <vorbis/vorbisfile.h>
#include <optional>
#include <string>

namespace sound {

/**
 * For vorbisfile to read from our buffer instead of from a file.
 */
struct OggVorbisBufferSource {
	std::string buf;
	size_t cur_offset = 0;

	static size_t read_func(void *ptr, size_t size, size_t nmemb, void *datasource) noexcept;
	static int seek_func(void *datasource, ogg_int64_t offset, int whence) noexcept;
	static int close_func(void *datasource) noexcept;
	static long tell_func(void *datasource) noexcept;

	static const ov_callbacks s_ov_callbacks;
};

/**
 * Metadata of an Ogg-Vorbis file, used for decoding.
 * We query this information once and store it in this struct.
 */
struct OggFileDecodeInfo {
	std::string name_for_logging;
	bool is_stereo;
	ALenum format; // AL_FORMAT_MONO16 or AL_FORMAT_STEREO16
	size_t bytes_per_sample;
	ALsizei freq;
	ALuint length_samples = 0;
	f32 length_seconds = 0.0f;
};

/**
 * RAII wrapper for OggVorbis_File.
 */
struct RAIIOggFile {
	bool m_needs_clear = false;
	OggVorbis_File m_file;

	RAIIOggFile() = default;

	DISABLE_CLASS_COPY(RAIIOggFile)

	~RAIIOggFile() noexcept
	{
		if (m_needs_clear)
			ov_clear(&m_file);
	}

	OggVorbis_File *get() { return &m_file; }

	std::optional<OggFileDecodeInfo> getDecodeInfo(const std::string &filename_for_logging);

	/**
	 * Main function for loading ogg vorbis sounds.
	 * Loads exactly the specified interval of PCM-data, and creates an OpenAL
	 * buffer with it.
	 *
	 * @param decode_info Cached meta information of the file.
	 * @param pcm_start First sample in the interval.
	 * @param pcm_end One after last sample of the interval (=> exclusive).
	 * @return An AL sound buffer, or a 0-buffer on failure.
	 */
	RAIIALSoundBuffer loadBuffer(const OggFileDecodeInfo &decode_info, ALuint pcm_start,
			ALuint pcm_end);
};

} // namespace sound
