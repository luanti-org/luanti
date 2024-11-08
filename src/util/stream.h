// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest Authors

#pragma once

#include <iostream>
#include <string_view>
#include <functional>

template<unsigned int BufferLength, typename Emitter = std::function<void(std::string_view)> >
class StringStreamBuffer : public std::streambuf {
public:
	StringStreamBuffer(Emitter emitter) : m_emitter(emitter) {
		buffer_index = 0;
	}

	int overflow(int c) override {
		if (c != traits_type::eof())
			push_back(c);
		return 0;
	}

	void push_back(char c) {
		// emit only complete lines, or if the buffer is full
		if (c == '\n') {
			sync();
		} else {
			buffer[buffer_index++] = c;
			if (buffer_index >= BufferLength) {
				sync();
			}
		}
	}

	std::streamsize xsputn(const char *s, std::streamsize n) override {
		for (std::streamsize i = 0; i < n; ++i)
			push_back(s[i]);
		return n;
	}

	int sync() override {
		if (buffer_index)
			m_emitter(std::string_view(buffer, buffer_index));
		buffer_index = 0;
		return 0;
	}

private:
	Emitter m_emitter;
	unsigned int buffer_index;
	char buffer[BufferLength];
};

class DummyStreamBuffer : public std::streambuf {
	int overflow(int c) override {
		return 0;
	}
	std::streamsize xsputn(const char *s, std::streamsize n) override {
		return n;
	}
};
