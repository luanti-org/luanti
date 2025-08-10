// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "util/pointer.h" // Buffer<T>
#include "irrlichttypes_bloated.h"
#include "networkprotocol.h"
#include <SColor.h>
#include <string>
#include <string_view>
#include <vector>

class NetworkPacket
{
public:
	NetworkPacket(u16 command, u32 preallocate, session_t peer_id) :
		m_command(command), m_peer_id(peer_id)
	{
		m_data.reserve(preallocate);
	}
	NetworkPacket(u16 command, u32 preallocate) :
		m_command(command)
	{
		m_data.reserve(preallocate);
	}
	NetworkPacket() = default;

	~NetworkPacket() = default;

	void putRawPacket(const u8 *data, u32 datasize, session_t peer_id);
	void clear();

	// Getters
	u32 getSize() const { return m_datasize; }
	session_t getPeerId() const { return m_peer_id; }
	u16 getCommand() const { return m_command; }
	u32 getRemainingBytes() const { return m_datasize - m_read_offset; }

	// Returns a pointer to buffer data.
	// A better name for this would be getRawString()
	const char *getString(u32 from_offset) const;
	const char *getRemainingString() const { return getString(m_read_offset); }

	// Perform length check and skip ahead by `count` bytes.
	void skip(u32 count);

	// Appends bytes from string buffer to packet
	void putRawString(const char *src, u32 len);
	void putRawString(std::string_view src)
	{
		putRawString(src.data(), src.size());
	}

	// Reads bytes from packet into string buffer
	void readRawString(char *dst, u32 len);
	std::string readRawString(u32 len)
	{
		std::string s;
		s.resize(len);
		readRawString(&s[0], len);
		return s;
	}

	NetworkPacket &operator>>(std::string &dst);
	NetworkPacket &operator<<(std::string_view src);

	void putLongString(std::string_view src);

	NetworkPacket &operator>>(std::wstring &dst);
	NetworkPacket &operator<<(std::wstring_view src);

	std::string readLongString();

	NetworkPacket &operator>>(char &dst);
	NetworkPacket &operator<<(char src);

	NetworkPacket &operator>>(bool &dst);
	NetworkPacket &operator<<(bool src);

	NetworkPacket &operator>>(u8 &dst);
	NetworkPacket &operator<<(u8 src);

	NetworkPacket &operator>>(u16 &dst);
	NetworkPacket &operator<<(u16 src);

	NetworkPacket &operator>>(u32 &dst);
	NetworkPacket &operator<<(u32 src);

	NetworkPacket &operator>>(u64 &dst);
	NetworkPacket &operator<<(u64 src);

	NetworkPacket &operator>>(float &dst);
	NetworkPacket &operator<<(float src);

	NetworkPacket &operator>>(v2f &dst);
	NetworkPacket &operator<<(v2f src);

	NetworkPacket &operator>>(v3f &dst);
	NetworkPacket &operator<<(v3f src);

	NetworkPacket &operator>>(s16 &dst);
	NetworkPacket &operator<<(s16 src);

	NetworkPacket &operator>>(s32 &dst);
	NetworkPacket &operator<<(s32 src);

	NetworkPacket &operator>>(v2s32 &dst);
	NetworkPacket &operator<<(v2s32 src);

	NetworkPacket &operator>>(v3s16 &dst);
	NetworkPacket &operator<<(v3s16 src);

	NetworkPacket &operator>>(v3s32 &dst);
	NetworkPacket &operator<<(v3s32 src);

	NetworkPacket &operator>>(video::SColor &dst);
	NetworkPacket &operator<<(video::SColor src);

	// Temp, we remove SharedBuffer when migration finished
	// ^ this comment has been here for 7 years
	Buffer<u8> oldForgePacket();

private:
	void checkReadOffset(u32 from_offset, u32 field_size) const;

	// resize data buffer for writing
	inline void checkDataSize(u32 field_size)
	{
		if (m_read_offset + field_size > m_datasize) {
			m_datasize = m_read_offset + field_size;
			m_data.resize(m_datasize);
		}
	}

	std::vector<u8> m_data;
	u32 m_datasize = 0;
	u32 m_read_offset = 0; // read and write offset
	u16 m_command = 0;
	session_t m_peer_id = 0;
};
