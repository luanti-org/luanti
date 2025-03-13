// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "sound_data.h"

#include "sound_constants.h"
#include <algorithm>

namespace sound {

/*
 * ISoundDataOpen struct
 */

std::shared_ptr<ISoundDataOpen> ISoundDataOpen::fromOggFile(std::unique_ptr<RAIIOggFile> oggfile,
		const std::string &filename_for_logging)
{
	// Get some information about the OGG file
	std::optional<OggFileDecodeInfo> decode_info = oggfile->getDecodeInfo(filename_for_logging);
	if (!decode_info.has_value()) {
		warningstream << "Audio: Error decoding "
				<< filename_for_logging << std::endl;
		return nullptr;
	}

	// use duration (in seconds) to decide whether to load all at once or to stream
	if (decode_info->length_seconds <= SOUND_DURATION_MAX_SINGLE) {
		return std::make_shared<SoundDataOpenBuffer>(std::move(oggfile), *decode_info);
	} else {
		return std::make_shared<SoundDataOpenStream>(std::move(oggfile), *decode_info);
	}
}

/*
 * SoundDataUnopenBuffer struct
 */

std::shared_ptr<ISoundDataOpen> SoundDataUnopenBuffer::open(const std::string &sound_name) &&
{
	// load from m_buffer

	auto oggfile = std::make_unique<RAIIOggFile>();

	auto buffer_source = std::make_unique<OggVorbisBufferSource>();
	buffer_source->buf = std::move(m_buffer);

	oggfile->m_needs_clear = true;
	if (ov_open_callbacks(buffer_source.release(), oggfile->get(), nullptr, 0,
			OggVorbisBufferSource::s_ov_callbacks) != 0) {
		warningstream << "Audio: Error opening " << sound_name << " for decoding"
				<< std::endl;
		return nullptr;
	}

	return ISoundDataOpen::fromOggFile(std::move(oggfile), sound_name);
}

/*
 * SoundDataUnopenFile struct
 */

std::shared_ptr<ISoundDataOpen> SoundDataUnopenFile::open(const std::string &sound_name) &&
{
	// load from file at m_path

	auto oggfile = std::make_unique<RAIIOggFile>();

	if (ov_fopen(m_path.c_str(), oggfile->get()) != 0) {
		warningstream << "Audio: Error opening " << m_path << " for decoding"
				<< std::endl;
		return nullptr;
	}
	oggfile->m_needs_clear = true;

	return ISoundDataOpen::fromOggFile(std::move(oggfile), sound_name);
}

/*
 * SoundDataOpenBuffer struct
 */

SoundDataOpenBuffer::SoundDataOpenBuffer(std::unique_ptr<RAIIOggFile> oggfile,
		const OggFileDecodeInfo &decode_info) : ISoundDataOpen(decode_info)
{
	m_buffer = oggfile->loadBuffer(m_decode_info, 0, m_decode_info.length_samples);
	if (m_buffer.get() == 0) {
		warningstream << "SoundDataOpenBuffer: Failed to load sound \""
				<< m_decode_info.name_for_logging << "\"" << std::endl;
		return;
	}
}

/*
 * SoundDataOpenStream struct
 */

SoundDataOpenStream::SoundDataOpenStream(std::unique_ptr<RAIIOggFile> oggfile,
		const OggFileDecodeInfo &decode_info) :
	ISoundDataOpen(decode_info), m_oggfile(std::move(oggfile))
{
	// do nothing here. buffers are loaded at getOrLoadBufferAt
}

std::tuple<ALuint, ALuint, ALuint> SoundDataOpenStream::getOrLoadBufferAt(ALuint offset)
{
	if (offset >= m_decode_info.length_samples)
		return {0, m_decode_info.length_samples, 0};

	// find the right-most ContiguousBuffers, such that `m_start <= offset`
	// equivalent: the first element from the right such that `!(m_start > offset)`
	// (from the right, `offset` is a lower bound to the `m_start`s)
	auto lower_rit = std::lower_bound(m_bufferss.rbegin(), m_bufferss.rend(), offset,
			[](const ContiguousBuffers &bufs, ALuint offset) {
				return bufs.m_start > offset;
			});

	if (lower_rit != m_bufferss.rend()) {
		std::vector<SoundBufferUntil> &bufs = lower_rit->m_buffers;
		// find the left-most SoundBufferUntil, such that `m_end > offset`
		// equivalent: the first element from the left such that `m_end > offset`
		// (returns first element where comp gives true)
		auto upper_it = std::upper_bound(bufs.begin(), bufs.end(), offset,
				[](ALuint offset, const SoundBufferUntil &buf) {
					return offset < buf.m_end;
				});

		if (upper_it != bufs.end()) {
			ALuint start = upper_it == bufs.begin() ? lower_rit->m_start
					: (upper_it - 1)->m_end;
			return {upper_it->m_buffer.get(), upper_it->m_end, offset - start};
		}
	}

	// no loaded buffer starts before or at `offset`
	// or no loaded buffer (that starts before or at `offset`) ends after `offset`

	// lower_rit, but not reverse and 1 farther
	auto after_it = m_bufferss.begin() + (m_bufferss.rend() - lower_rit);

	return loadBufferAt(offset, after_it);
}

std::tuple<ALuint, ALuint, ALuint> SoundDataOpenStream::loadBufferAt(ALuint offset,
		std::vector<ContiguousBuffers>::iterator after_it)
{
	bool has_before = after_it != m_bufferss.begin();
	bool has_after = after_it != m_bufferss.end();

	ALuint end_before = has_before ? (after_it - 1)->m_buffers.back().m_end : 0;
	ALuint start_after = has_after ? after_it->m_start : m_decode_info.length_samples;

	const ALuint min_buf_len_samples = m_decode_info.freq * MIN_STREAM_BUFFER_LENGTH;

	//
	// 1) Find the actual start and end of the new buffer
	//

	ALuint new_buf_start = offset;
	ALuint new_buf_end = offset + min_buf_len_samples;

	// Don't load into next buffer, or past the end
	if (new_buf_end > start_after) {
		new_buf_end = start_after;
		// Also move start (for min buf size) (but not *into* previous buffer)
		if (new_buf_end - new_buf_start < min_buf_len_samples) {
			new_buf_start = std::max(
					end_before,
					new_buf_end < min_buf_len_samples ? 0
							: new_buf_end - min_buf_len_samples
				);
		}
	}

	// Widen if space to right or left is smaller than min buf size
	if (new_buf_start - end_before < min_buf_len_samples)
		new_buf_start = end_before;
	if (start_after - new_buf_end < min_buf_len_samples)
		new_buf_end = start_after;

	//
	// 2) Load [new_buf_start, new_buf_end)
	//

	// If it fails, we get a 0-buffer. we store it and won't try loading again
	RAIIALSoundBuffer new_buf = m_oggfile->loadBuffer(m_decode_info, new_buf_start,
			new_buf_end);

	//
	// 3) Insert before after_it
	//

	// Choose ContiguousBuffers to add the new SoundBufferUntil into:
	// * `after_it - 1` (=before) if existent and if there's no space between its
	//   last buffer and the new buffer
	// * A new ContiguousBuffers otherwise
	auto it = has_before && new_buf_start == end_before ? after_it - 1
			: m_bufferss.insert(after_it, ContiguousBuffers{new_buf_start, {}});

	// Add the new SoundBufferUntil
	size_t new_buf_i = it->m_buffers.size();
	it->m_buffers.push_back(SoundBufferUntil{new_buf_end, std::move(new_buf)});

	if (has_after && new_buf_end == start_after) {
		// Merge after into my ContiguousBuffers
		auto &bufs = it->m_buffers;
		auto &bufs_after = (it + 1)->m_buffers;
		bufs.insert(bufs.end(), std::make_move_iterator(bufs_after.begin()),
				std::make_move_iterator(bufs_after.end()));
		it = m_bufferss.erase(it + 1) - 1;
	}

	return {it->m_buffers[new_buf_i].m_buffer.get(), new_buf_end, offset - new_buf_start};
}

} // namespace sound
