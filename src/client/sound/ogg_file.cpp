// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "ogg_file.h"

#include <cassert>
#include <cstring> // memcpy
#include <memory>

namespace sound {

/*
 * OggVorbisBufferSource struct
 */

size_t OggVorbisBufferSource::read_func(void *ptr, size_t size, size_t nmemb,
		void *datasource) noexcept
{
	OggVorbisBufferSource *s = (OggVorbisBufferSource *)datasource;
	size_t copied_size = MYMIN(s->buf.size() - s->cur_offset, size);
	memcpy(ptr, s->buf.data() + s->cur_offset, copied_size);
	s->cur_offset += copied_size;
	return copied_size;
}

int OggVorbisBufferSource::seek_func(void *datasource, ogg_int64_t offset, int whence) noexcept
{
	OggVorbisBufferSource *s = (OggVorbisBufferSource *)datasource;
	if (whence == SEEK_SET) {
		if (offset < 0 || (size_t)offset > s->buf.size()) {
			// offset out of bounds
			return -1;
		}
		s->cur_offset = offset;
		return 0;
	} else if (whence == SEEK_CUR) {
		if ((size_t)MYMIN(-offset, 0) > s->cur_offset
				|| s->cur_offset + offset > s->buf.size()) {
			// offset out of bounds
			return -1;
		}
		s->cur_offset += offset;
		return 0;
	} else if (whence == SEEK_END) {
		if (offset > 0 || (size_t)-offset > s->buf.size()) {
			// offset out of bounds
			return -1;
		}
		s->cur_offset = s->buf.size() - offset;
		return 0;
	}
	return -1;
}

int OggVorbisBufferSource::close_func(void *datasource) noexcept
{
	auto s = reinterpret_cast<OggVorbisBufferSource *>(datasource);
	delete s;
	return 0;
}

long OggVorbisBufferSource::tell_func(void *datasource) noexcept
{
	OggVorbisBufferSource *s = (OggVorbisBufferSource *)datasource;
	return s->cur_offset;
}

const ov_callbacks OggVorbisBufferSource::s_ov_callbacks = {
	&OggVorbisBufferSource::read_func,
	&OggVorbisBufferSource::seek_func,
	&OggVorbisBufferSource::close_func,
	&OggVorbisBufferSource::tell_func
};

/*
 * RAIIOggFile struct
 */

std::optional<OggFileDecodeInfo> RAIIOggFile::getDecodeInfo(const std::string &filename_for_logging)
{
	OggFileDecodeInfo ret;
	ret.name_for_logging = filename_for_logging;

	long num_logical_bitstreams = ov_streams(&m_file);
	if (num_logical_bitstreams < 1) {
		warningstream << "Audio: Can't decode. Has not even one logical bitstream (but "
				<< num_logical_bitstreams << "): "
				<< ret.name_for_logging << std::endl;
		return std::nullopt;
	}

	// We only support sounds where the sample rate and channel count doesn't
	// change.
	// Read the info for the first logical bitstream:
	vorbis_info *info0 = ov_info(&m_file, 0);
	if (!info0) {
		warningstream << "Audio: Can't decode. ov_info failed for: "
				<< ret.name_for_logging << std::endl;
		return std::nullopt;
	}

	if (info0->channels == 1) {
		ret.is_stereo = false;
		ret.format = AL_FORMAT_MONO16;
		ret.bytes_per_sample = 2;
	} else if (info0->channels == 2) {
		ret.is_stereo = true;
		ret.format = AL_FORMAT_STEREO16;
		ret.bytes_per_sample = 4;
	} else {
		warningstream << "Audio: Can't decode. Sound is neither mono nor stereo: "
				<< ret.name_for_logging << std::endl;
		return std::nullopt;
	}

	ret.freq = info0->rate;

	if (!ov_seekable(&m_file)) {
		warningstream << "Audio: Can't decode. Sound is not seekable, can't get length: "
				<< ret.name_for_logging << std::endl;
		return std::nullopt;
	}

	auto pcm_total = ov_pcm_total(&m_file, -1);
	assert(pcm_total >= 0);
	auto time_total = ov_time_total(&m_file, -1);
	assert(time_total >= 0);
	ret.length_samples = static_cast<ALuint>(pcm_total);
	ret.length_seconds = static_cast<f32>(time_total);

	// Check that channel count and sample rate do indeed not change
	for (int strm = 1; strm < num_logical_bitstreams; ++strm) {
		vorbis_info *info = ov_info(&m_file, strm);
		if (!info) {
			warningstream << "Audio: Can't decode. ov_info failed for: "
					<< ret.name_for_logging << std::endl;
			return std::nullopt;
		}
		if (info->channels != info0->channels) {
			warningstream << "Audio: Can't decode. Channel count changes from "
					<< info0->channels << " to " << info->channels << ": "
					<< ret.name_for_logging << std::endl;
			return std::nullopt;
		}
		if (info->rate != info0->rate) {
			warningstream << "Audio: Can't decode. Sample rate changes from "
					<< info0->rate << " to " << info->rate << ": "
					<< ret.name_for_logging << std::endl;
			return std::nullopt;
		}
	}

	return ret;
}

RAIIALSoundBuffer RAIIOggFile::loadBuffer(const OggFileDecodeInfo &decode_info,
		ALuint pcm_start, ALuint pcm_end)
{
	constexpr int endian = 0; // 0 for Little-Endian, 1 for Big-Endian
	constexpr int word_size = 2; // we use s16 samples
	constexpr int word_signed = 1; // ^

	assert(pcm_end <= decode_info.length_samples);
	assert(pcm_start <= decode_info.length_samples);

	// seek
	s64 current_pcm = ov_pcm_tell(&m_file);
	if (current_pcm != pcm_start) {
		if (ov_pcm_seek(&m_file, pcm_start) != 0) {
			warningstream << "Audio: Error decoding (could not seek): "
					<< decode_info.name_for_logging << std::endl;
			return RAIIALSoundBuffer();
		}
		assert(ov_pcm_tell(&m_file) == pcm_start);
		current_pcm = pcm_start;
	}

	const size_t size = static_cast<size_t>(pcm_end - pcm_start)
			* decode_info.bytes_per_sample;

	std::unique_ptr<char[]> snd_buffer(new char[size]);

	// read size bytes
	s64 last_byte_offset = current_pcm * decode_info.bytes_per_sample;
	size_t read_count = 0;
	int bitstream;
	while (read_count < size) {
		// Read up to a buffer's worth of decoded sound data
		long num_bytes = ov_read(&m_file, &snd_buffer[read_count], size - read_count,
				endian, word_size, word_signed, &bitstream);

		if (num_bytes <= 0) {
			std::string_view errstr = [&]{
				switch (num_bytes) {
				case 0: return "EOF";
				case OV_HOLE: return "OV_HOLE";
				case OV_EBADLINK: return "OV_EBADLINK";
				case OV_EINVAL: return "OV_EINVAL";
				default: return "unknown error";
				}
			}();
			warningstream << "Audio: Error decoding (" << errstr << "): "
					<< decode_info.name_for_logging << std::endl;
			return RAIIALSoundBuffer();
		}

		// This usually doesn't happen, but for some sounds ov_read seems to skip
		// some samples, see #14453.
		s64 current_byte_offset = ov_pcm_tell(&m_file) * decode_info.bytes_per_sample;
		if (current_byte_offset != last_byte_offset + num_bytes) {
			infostream << "Audio: ov_read skipped "
					<< current_byte_offset - (last_byte_offset + num_bytes)
					<< " bytes, re-seeking: "
					<< decode_info.name_for_logging << std::endl;
			s64 expected_offset = (last_byte_offset + num_bytes) / decode_info.bytes_per_sample;
			if (ov_pcm_seek(&m_file, expected_offset) != 0) {
				warningstream << "Audio: Error decoding (could not seek): "
						<< decode_info.name_for_logging << std::endl;
				return RAIIALSoundBuffer();
			}
			assert(ov_pcm_tell(&m_file) == expected_offset);
			current_byte_offset = last_byte_offset + num_bytes;
		}
		last_byte_offset = current_byte_offset;

		read_count += num_bytes;
	}

	// load buffer to openal
	RAIIALSoundBuffer snd_buffer_id = RAIIALSoundBuffer::generate();
	alBufferData(snd_buffer_id.get(), decode_info.format, &(snd_buffer[0]), size,
			decode_info.freq);

	ALenum error = alGetError();
	if (error != AL_NO_ERROR) {
		warningstream << "Audio: OpenAL error: " << getAlErrorString(error)
				<< "preparing sound buffer for sound \""
				<< decode_info.name_for_logging << "\"" << std::endl;
	}

	return snd_buffer_id;
}

} // namespace sound
