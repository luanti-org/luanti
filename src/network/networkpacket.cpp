// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "networkpacket.h"
#include <sstream>
#include "networkexceptions.h"
#include "util/serialize.h"
#include "networkprotocol.h"

void NetworkPacket::checkReadOffset(u32 from_offset, u32 field_size) const
{
	if (from_offset + field_size > m_datasize) {
		std::ostringstream ss;
		ss << "Reading outside packet (offset: " <<
				from_offset << ", packet size: " << getSize() << ")";
		throw PacketError(ss.str());
	}
}

void NetworkPacket::putRawPacket(const u8 *data, u32 datasize, session_t peer_id)
{
	// If a m_command is already set, we are rewriting on same packet
	// This is not permitted
	assert(m_command == 0);

	assert(datasize >= 2);
	m_datasize = datasize - 2;
	m_peer_id = peer_id;

	m_data.resize(m_datasize);

	// split command and datas
	m_command = readU16(&data[0]);
	if (m_datasize > 0)
		memcpy(m_data.data(), &data[2], m_datasize);
}

void NetworkPacket::clear()
{
	m_data.clear();
	m_datasize = 0;
	m_read_offset = 0;
	m_command = 0;
	m_peer_id = 0;
}

const char* NetworkPacket::getString(u32 from_offset) const
{
	checkReadOffset(from_offset, 0);

	return reinterpret_cast<const char*>(&m_data[from_offset]);
}

void NetworkPacket::skip(u32 count)
{
	checkReadOffset(m_read_offset, count);
	m_read_offset += count;
}

void NetworkPacket::putRawString(const char* src, u32 len)
{
	checkDataSize(len);

	if (len == 0)
		return;

	memcpy(&m_data[m_read_offset], src, len);
	m_read_offset += len;
}

void NetworkPacket::readRawString(char *dst, u32 len)
{
	checkReadOffset(m_read_offset, len);

	if (len == 0)
		return;

	memcpy(dst, &m_data[m_read_offset], len);
	m_read_offset += len;
}


NetworkPacket& NetworkPacket::operator>>(std::string& dst)
{
	checkReadOffset(m_read_offset, 2);
	u16 strLen = readU16(&m_data[m_read_offset]);
	m_read_offset += 2;

	dst.clear();

	if (strLen == 0) {
		return *this;
	}

	checkReadOffset(m_read_offset, strLen);

	dst.reserve(strLen);
	dst.append((char*)&m_data[m_read_offset], strLen);

	m_read_offset += strLen;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(std::string_view src)
{
	if (src.size() > STRING_MAX_LEN) {
		throw PacketError("String too long");
	}

	u16 msgsize = src.size();

	*this << msgsize;

	putRawString(src.data(), (u32)msgsize);

	return *this;
}

void NetworkPacket::putLongString(std::string_view src)
{
	if (src.size() > LONG_STRING_MAX_LEN) {
		throw PacketError("String too long");
	}

	u32 msgsize = src.size();

	*this << msgsize;

	putRawString(src.data(), msgsize);
}

static constexpr bool NEED_SURROGATE_CODING = sizeof(wchar_t) > 2;

NetworkPacket& NetworkPacket::operator>>(std::wstring& dst)
{
	checkReadOffset(m_read_offset, 2);
	u16 strLen = readU16(&m_data[m_read_offset]);
	m_read_offset += 2;

	dst.clear();

	if (strLen == 0) {
		return *this;
	}

	checkReadOffset(m_read_offset, strLen * 2);

	dst.reserve(strLen);
	for (u16 i = 0; i < strLen; i++) {
		wchar_t c = readU16(&m_data[m_read_offset]);
		if (NEED_SURROGATE_CODING && c >= 0xD800 && c < 0xDC00 && i+1 < strLen) {
			i++;
			m_read_offset += sizeof(u16);

			wchar_t c2 = readU16(&m_data[m_read_offset]);
			c = 0x10000 + ( ((c & 0x3ff) << 10) | (c2 & 0x3ff) );
		}
		dst.push_back(c);
		m_read_offset += sizeof(u16);
	}

	return *this;
}

NetworkPacket& NetworkPacket::operator<<(std::wstring_view src)
{
	if (src.size() > WIDE_STRING_MAX_LEN) {
		throw PacketError("String too long");
	}

	if (!NEED_SURROGATE_CODING || src.size() == 0) {
		*this << static_cast<u16>(src.size());
		for (u16 i = 0; i < src.size(); i++)
			*this << static_cast<u16>(src[i]);

		return *this;
	}

	// write dummy value, to be overwritten later
	const u32 len_offset = m_read_offset;
	u32 written = 0;
	*this << static_cast<u16>(0xfff0);

	for (u16 i = 0; i < src.size(); i++) {
		wchar_t c = src[i];
		if (c > 0xffff) {
			// Encode high code-points as surrogate pairs
			u32 n = c - 0x10000;
			*this << static_cast<u16>(0xD800 | (n >> 10))
				<< static_cast<u16>(0xDC00 | (n & 0x3ff));
			written += 2;
		} else {
			*this << static_cast<u16>(c);
			written++;
		}
	}

	if (written > WIDE_STRING_MAX_LEN)
		throw PacketError("String too long");
	writeU16(&m_data[len_offset], written);

	return *this;
}

std::string NetworkPacket::readLongString()
{
	checkReadOffset(m_read_offset, 4);
	u32 strLen = readU32(&m_data[m_read_offset]);
	m_read_offset += 4;

	if (strLen == 0) {
		return "";
	}

	if (strLen > LONG_STRING_MAX_LEN) {
		throw PacketError("String too long");
	}

	checkReadOffset(m_read_offset, strLen);

	std::string dst;

	dst.reserve(strLen);
	dst.append((char*)&m_data[m_read_offset], strLen);

	m_read_offset += strLen;

	return dst;
}

NetworkPacket& NetworkPacket::operator>>(char& dst)
{
	checkReadOffset(m_read_offset, 1);

	dst = readU8(&m_data[m_read_offset]);

	m_read_offset += 1;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(char src)
{
	checkDataSize(1);

	writeU8(&m_data[m_read_offset], src);

	m_read_offset += 1;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(u8 src)
{
	checkDataSize(1);

	writeU8(&m_data[m_read_offset], src);

	m_read_offset += 1;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(bool src)
{
	checkDataSize(1);

	writeU8(&m_data[m_read_offset], src ? 1 : 0);

	m_read_offset += 1;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(u16 src)
{
	checkDataSize(2);

	writeU16(&m_data[m_read_offset], src);

	m_read_offset += 2;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(u32 src)
{
	checkDataSize(4);

	writeU32(&m_data[m_read_offset], src);

	m_read_offset += 4;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(u64 src)
{
	checkDataSize(8);

	writeU64(&m_data[m_read_offset], src);

	m_read_offset += 8;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(float src)
{
	checkDataSize(4);

	writeF32(&m_data[m_read_offset], src);

	m_read_offset += 4;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(bool& dst)
{
	checkReadOffset(m_read_offset, 1);

	dst = readU8(&m_data[m_read_offset]) != 0;

	m_read_offset += 1;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(u8& dst)
{
	checkReadOffset(m_read_offset, 1);

	dst = readU8(&m_data[m_read_offset]);

	m_read_offset += 1;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(u16& dst)
{
	checkReadOffset(m_read_offset, 2);

	dst = readU16(&m_data[m_read_offset]);

	m_read_offset += 2;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(u32& dst)
{
	checkReadOffset(m_read_offset, 4);

	dst = readU32(&m_data[m_read_offset]);

	m_read_offset += 4;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(u64& dst)
{
	checkReadOffset(m_read_offset, 8);

	dst = readU64(&m_data[m_read_offset]);

	m_read_offset += 8;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(float& dst)
{
	checkReadOffset(m_read_offset, 4);

	dst = readF32(&m_data[m_read_offset]);

	m_read_offset += 4;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(v2f& dst)
{
	checkReadOffset(m_read_offset, 8);

	dst = readV2F32(&m_data[m_read_offset]);

	m_read_offset += 8;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(v3f& dst)
{
	checkReadOffset(m_read_offset, 12);

	dst = readV3F32(&m_data[m_read_offset]);

	m_read_offset += 12;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(s16& dst)
{
	checkReadOffset(m_read_offset, 2);

	dst = readS16(&m_data[m_read_offset]);

	m_read_offset += 2;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(s16 src)
{
	*this << (u16) src;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(s32& dst)
{
	checkReadOffset(m_read_offset, 4);

	dst = readS32(&m_data[m_read_offset]);

	m_read_offset += 4;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(s32 src)
{
	*this << (u32) src;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(v3s16& dst)
{
	checkReadOffset(m_read_offset, 6);

	dst = readV3S16(&m_data[m_read_offset]);

	m_read_offset += 6;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(v2s32& dst)
{
	checkReadOffset(m_read_offset, 8);

	dst = readV2S32(&m_data[m_read_offset]);

	m_read_offset += 8;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(v3s32& dst)
{
	checkReadOffset(m_read_offset, 12);

	dst = readV3S32(&m_data[m_read_offset]);

	m_read_offset += 12;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(v2f src)
{
	*this << (float) src.X;
	*this << (float) src.Y;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(v3f src)
{
	*this << (float) src.X;
	*this << (float) src.Y;
	*this << (float) src.Z;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(v3s16 src)
{
	*this << (s16) src.X;
	*this << (s16) src.Y;
	*this << (s16) src.Z;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(v2s32 src)
{
	*this << (s32) src.X;
	*this << (s32) src.Y;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(v3s32 src)
{
	*this << (s32) src.X;
	*this << (s32) src.Y;
	*this << (s32) src.Z;
	return *this;
}

NetworkPacket& NetworkPacket::operator>>(video::SColor& dst)
{
	checkReadOffset(m_read_offset, 4);

	dst = readARGB8(&m_data[m_read_offset]);

	m_read_offset += 4;
	return *this;
}

NetworkPacket& NetworkPacket::operator<<(video::SColor src)
{
	checkDataSize(4);

	writeU32(&m_data[m_read_offset], src.color);

	m_read_offset += 4;
	return *this;
}

Buffer<u8> NetworkPacket::oldForgePacket()
{
	// this is the dummy packet used to first contact the server
	if (m_command == 0) {
		assert(m_datasize == 0);
		return Buffer<u8>();
	}

	Buffer<u8> sb(m_datasize + 2);
	writeU16(&sb[0], m_command);
	if (m_datasize > 0)
		memcpy(&sb[2], m_data.data(), m_datasize);

	return sb;
}
