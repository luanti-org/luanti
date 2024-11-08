// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#pragma once

#include "ogg_file.h"
#include <memory>
#include <tuple>
#include <vector>

namespace sound {

/**
 * Stores sound pcm data buffers.
 */
struct ISoundDataOpen
{
	OggFileDecodeInfo m_decode_info;

	explicit ISoundDataOpen(const OggFileDecodeInfo &decode_info) :
			m_decode_info(decode_info) {}

	virtual ~ISoundDataOpen() = default;

	/**
	 * Iff the data is streaming, there is more than one buffer.
	 * @return Whether it's streaming data.
	 */
	virtual bool isStreaming() const noexcept = 0;

	/**
	 * Load a buffer containing data starting at the given offset. Or just get it
	 * if it was already loaded.
	 *
	 * This function returns multiple values:
	 * * `buffer`: The OpenAL buffer.
	 * * `buffer_end`: The offset (in the file) where `buffer` ends (exclusive).
	 * * `offset_in_buffer`: Offset relative to `buffer`'s start where the requested
	 *       `offset` is.
	 *       `offset_in_buffer == 0` is guaranteed if some loaded buffer ends at
	 *       `offset`.
	 *
	 * @param offset The start of the buffer.
	 * @return `{buffer, buffer_end, offset_in_buffer}` or `{0, sound_data_end, 0}`
	 *         if `offset` is invalid.
	 */
	virtual std::tuple<ALuint, ALuint, ALuint> getOrLoadBufferAt(ALuint offset) = 0;

	static std::shared_ptr<ISoundDataOpen> fromOggFile(std::unique_ptr<RAIIOggFile> oggfile,
		const std::string &filename_for_logging);
};

/**
 * Will be opened lazily when first used.
 */
struct ISoundDataUnopen
{
	virtual ~ISoundDataUnopen() = default;

	// Note: The ISoundDataUnopen is moved (see &&). It is not meant to be kept
	// after opening.
	virtual std::shared_ptr<ISoundDataOpen> open(const std::string &sound_name) && = 0;
};

/**
 * Sound file is in a memory buffer.
 */
struct SoundDataUnopenBuffer final : ISoundDataUnopen
{
	std::string m_buffer;

	explicit SoundDataUnopenBuffer(std::string &&buffer) : m_buffer(std::move(buffer)) {}

	std::shared_ptr<ISoundDataOpen> open(const std::string &sound_name) && override;
};

/**
 * Sound file is in file system.
 */
struct SoundDataUnopenFile final : ISoundDataUnopen
{
	std::string m_path;

	explicit SoundDataUnopenFile(const std::string &path) : m_path(path) {}

	std::shared_ptr<ISoundDataOpen> open(const std::string &sound_name) && override;
};

/**
 * Non-streaming opened sound data.
 * All data is completely loaded in one buffer.
 */
struct SoundDataOpenBuffer final : ISoundDataOpen
{
	RAIIALSoundBuffer m_buffer;

	SoundDataOpenBuffer(std::unique_ptr<RAIIOggFile> oggfile,
			const OggFileDecodeInfo &decode_info);

	bool isStreaming() const noexcept override { return false; }

	std::tuple<ALuint, ALuint, ALuint> getOrLoadBufferAt(ALuint offset) override
	{
		if (offset >= m_decode_info.length_samples)
			return {0, m_decode_info.length_samples, 0};
		return {m_buffer.get(), m_decode_info.length_samples, offset};
	}
};

/**
 * Streaming opened sound data.
 *
 * Uses a sorted list of contiguous sound data regions (`ContiguousBuffers`s) for
 * efficient seeking.
 */
struct SoundDataOpenStream final : ISoundDataOpen
{
	/**
	 * An OpenAL buffer that goes until `m_end` (exclusive).
	 */
	struct SoundBufferUntil final
	{
		ALuint m_end;
		RAIIALSoundBuffer m_buffer;
	};

	/**
	 * A sorted non-empty vector of contiguous buffers.
	 * The start (inclusive) of each buffer is the end of its predecessor, or
	 * `m_start` for the first buffer.
	 */
	struct ContiguousBuffers final
	{
		ALuint m_start;
		std::vector<SoundBufferUntil> m_buffers;
	};

	std::unique_ptr<RAIIOggFile> m_oggfile;
	// A sorted vector of non-overlapping, non-contiguous `ContiguousBuffers`s.
	std::vector<ContiguousBuffers> m_bufferss;

	SoundDataOpenStream(std::unique_ptr<RAIIOggFile> oggfile,
			const OggFileDecodeInfo &decode_info);

	bool isStreaming() const noexcept override { return true; }

	std::tuple<ALuint, ALuint, ALuint> getOrLoadBufferAt(ALuint offset) override;

private:
	// offset must be before after_it's m_start and after (after_it-1)'s last m_end
	// new buffer will be inserted into m_bufferss before after_it
	// returns same as getOrLoadBufferAt
	std::tuple<ALuint, ALuint, ALuint> loadBufferAt(ALuint offset,
			std::vector<ContiguousBuffers>::iterator after_it);
};

} // namespace sound
