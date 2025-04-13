// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "serialize.h"
#include "porting.h"
#include "util/string.h"
#include "util/hex.h"
#include "exceptions.h"
#include "irrlichttypes.h"

#include <iostream>
#include <cassert>

FloatType g_serialize_f32_type = FLOATTYPE_UNKNOWN;


////
//// String
////

std::string serializeString16(std::string_view plain)
{
	std::string s;
	char buf[2];

	static_assert(STRING_MAX_LEN <= U16_MAX);
	if (plain.size() > STRING_MAX_LEN)
		throw SerializationError("String too long for serializeString16");
	s.reserve(2 + plain.size());

	writeU16((u8 *)&buf[0], plain.size());
	s.append(buf, 2);

	s.append(plain);
	return s;
}

std::string deSerializeString16(std::istream &is)
{
	std::string s;
	char buf[2];

	is.read(buf, 2);
	if (is.gcount() != 2)
		throw SerializationError("deSerializeString16: size not read");

	u16 s_size = readU16((u8 *)buf);
	if (s_size == 0)
		return s;

	s.resize(s_size);
	is.read(&s[0], s_size);
	if (is.gcount() != s_size)
		throw SerializationError("deSerializeString16: truncated");

	return s;
}


////
//// Long String
////

std::string serializeString32(std::string_view plain)
{
	std::string s;
	char buf[4];

	static_assert(LONG_STRING_MAX_LEN <= U32_MAX);
	if (plain.size() > LONG_STRING_MAX_LEN)
		throw SerializationError("String too long for serializeLongString");
	s.reserve(4 + plain.size());

	writeU32((u8*)&buf[0], plain.size());
	s.append(buf, 4);
	s.append(plain);
	return s;
}

std::string deSerializeString32(std::istream &is)
{
	std::string s;
	char buf[4];

	is.read(buf, 4);
	if (is.gcount() != 4)
		throw SerializationError("deSerializeLongString: size not read");

	u32 s_size = readU32((u8 *)buf);
	if (s_size == 0)
		return s;

	// We don't really want a remote attacker to force us to allocate 4GB...
	if (s_size > LONG_STRING_MAX_LEN) {
		throw SerializationError("deSerializeLongString: "
			"string too long: " + itos(s_size) + " bytes");
	}

	s.resize(s_size);
	is.read(&s[0], s_size);
	if ((u32)is.gcount() != s_size)
		throw SerializationError("deSerializeLongString: truncated");

	return s;
}

////
//// String Array
////

std::string serializeString16Array(const std::vector<std::string> &array)
{
	std::string ret;
	const auto &at = [&] (size_t index) {
		return reinterpret_cast<u8*>(&ret[index]);
	};

	if (array.size() > U32_MAX)
		throw SerializationError("serializeString16Array: too many strings");
	ret.resize(4 + array.size() * 2);
	writeU32(at(0), array.size());

	// Serialize lengths next to each other
	size_t total = 0;
	for (u32 i = 0; i < array.size(); i++) {
		auto &s = array[i];
		if (s.size() > STRING_MAX_LEN)
			throw SerializationError("serializeString16Array: string too long");
		writeU16(at(4 + 2*i), s.size());
		total += s.size();
	}

	// Now the contents
	ret.reserve(ret.size() + total);
	for (auto &s : array)
		ret.append(s);

	return ret;
}

std::vector<std::string> deserializeString16Array(std::istream &is)
{
	std::vector<std::string> ret;

	u32 count = readU32(is);
	if (is.gcount() != 4)
		throw SerializationError("deserializeString16Array: count not read");
	ret.resize(count);

	// prepare string buffers as we read the sizes
	for (auto &sbuf : ret) {
		u16 size = readU16(is);
		if (is.gcount() != 2)
			throw SerializationError("deserializeString16Array: size not read");
		sbuf.resize(size);
	}

	// now extract the strings
	for (auto &sbuf : ret) {
		is.read(sbuf.data(), sbuf.size());
		if (is.gcount() != (std::streamsize) sbuf.size())
			throw SerializationError("deserializeString16Array: truncated");
	}

	return ret;
}

////
//// JSON-like strings
////

std::string serializeJsonString(std::string_view plain)
{
	std::string tmp;

	tmp.reserve(plain.size() + 2);
	tmp.push_back('"');

	for (char c : plain) {
		switch (c) {
			case '"':
				tmp.append("\\\"");
				break;
			case '\\':
				tmp.append("\\\\");
				break;
			case '\b':
				tmp.append("\\b");
				break;
			case '\f':
				tmp.append("\\f");
				break;
			case '\n':
				tmp.append("\\n");
				break;
			case '\r':
				tmp.append("\\r");
				break;
			case '\t':
				tmp.append("\\t");
				break;
			default: {
				if (c >= 32 && c <= 126) {
					tmp.push_back(c);
				} else {
					// We pretend that Unicode codepoints map to bytes (they don't)
					u8 cnum = static_cast<u8>(c);
					tmp.append("\\u00");
					tmp.push_back(hex_chars[cnum >> 4]);
					tmp.push_back(hex_chars[cnum & 0xf]);
				}
				break;
			}
		}
	}

	tmp.push_back('"');
	return tmp;
}

static void deSerializeJsonString(std::string &s)
{
	assert(s.size() >= 2);
	assert(s.front() == '"' && s.back() == '"');

	size_t w = 0; // write index
	size_t i = 1; // read index
	const size_t len = s.size() - 1; // string length with trailing quote removed

	while (i < len) {
		char c = s[i++];
		assert(c != '"');

		if (c != '\\') {
			s[w++] = c;
			continue;
		}

		if (i >= len)
			throw SerializationError("JSON string ended prematurely");
		char c2 = s[i++];
		switch (c2) {
			case 'b':
				s[w++] = '\b';
				break;
			case 'f':
				s[w++] = '\f';
				break;
			case 'n':
				s[w++] = '\n';
				break;
			case 'r':
				s[w++] = '\r';
				break;
			case 't':
				s[w++] = '\t';
				break;
			case 'u': {
				if (i + 3 >= len)
					throw SerializationError("JSON string ended prematurely");
				unsigned char v[4] = {};
				for (int j = 0; j < 4; j++)
					hex_digit_decode(s[i+j], v[j]);
				i += 4;
				u32 hexnumber = (v[0] << 12) | (v[1] << 8) | (v[2] << 4) | v[3];
				// Note that this does not work for anything other than ASCII
				// but these functions do not actually interact with real JSON input.
				s[w++] = (int) hexnumber;
				break;
			}
			default:
				s[w++] = c2;
				break;
		}
	}

	assert(w <= i && i <= len);
	// Truncate string to current write index
	s.resize(w);
}

std::string deSerializeJsonString(std::istream &is)
{
	std::string tmp;
	char c;
	bool was_backslash = false;

	// Parse initial doublequote
	c = is.get();
	if (c != '"')
		throw SerializationError("JSON string must start with doublequote");
	tmp.push_back(c);

	// Grab the entire json string
	for (;;) {
		c = is.get();
		if (is.eof())
			throw SerializationError("JSON string ended prematurely");

		tmp.push_back(c);
		if (was_backslash)
			was_backslash = false;
		else if (c == '\\')
			was_backslash = true;
		else if (c == '"')
			break; // found end of string
	}

	deSerializeJsonString(tmp);
	return tmp;
}

std::string serializeJsonStringIfNeeded(std::string_view s)
{
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] <= 0x1f || s[i] >= 0x7f || s[i] == ' ' || s[i] == '\"')
			return serializeJsonString(s);
	}
	return std::string(s);
}

std::string deSerializeJsonStringIfNeeded(std::istream &is)
{
	// Check for initial quote
	char c = is.peek();
	if (is.eof())
		return "";

	if (c == '"') {
		// json string: defer to the right implementation
		return deSerializeJsonString(is);
	}

	// not a json string:
	std::string tmp;
	std::getline(is, tmp, ' ');
	if (!is.eof())
		is.unget(); // we hit a space, put it back
	return tmp;
}

