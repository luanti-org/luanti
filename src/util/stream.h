// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Luanti Authors

#pragma once

#include <cstring>
#include <iostream>
#include <string_view>
#include <functional>

// this is declared in util/string.h, which we don't want to pull in entirely.
extern size_t utf8_truncate_count(std::string_view input);

template<unsigned int BufferLength, typename Emitter = std::function<void(std::string_view)> >
class StringStreamBuffer : public std::streambuf {
public:
	StringStreamBuffer(Emitter emitter) : m_emitter(emitter) {
		buffer_index = 0;
	}

protected:
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
		unsigned int tail = 0;
		if (buffer_index) {
			tail = utf8_truncate_count(std::string_view(buffer, buffer_index));
			if (tail != buffer_index)
				m_emitter(std::string_view(buffer, buffer_index - tail));
		}
		if (tail != buffer_index)
			memmove(buffer, buffer + buffer_index - tail, tail);
		// Note: because utf8_truncate_count will never return a tail larger
		// than 4, we will always be able to flush enough bytes so that push_back
		// doesn't need to worry about overflow (unless our buffer is tiny).
		static_assert(BufferLength >= 10);
		buffer_index = tail;
		return 0;
	}

private:
	Emitter m_emitter;
	unsigned int buffer_index;
	char buffer[BufferLength];
};

class DummyStreamBuffer : public std::streambuf {
protected:
	int overflow(int c) override {
		return 0;
	}
	std::streamsize xsputn(const char *s, std::streamsize n) override {
		return n;
	}
};

/******/

/// Can be used in place of std::ostringstream and allows operating on the result zero-copy.
/// @note will be obsolete with C++20
class my_ostringstream : public std::ostream {
public:
	my_ostringstream() : std::ostream(&buf) {}

	const std::string &str() const { return buf.buffer; }

	std::string take() {
		std::string ret;
		buf.buffer.swap(ret);
		return ret;
	}

	/// @warning The view will be invalidated when writing to the stream!
	std::string_view view() const { return std::string_view(buf.buffer); }

private:
	class my_ostringbuf : public std::streambuf {
	public:
		my_ostringbuf() {}

		std::string buffer;

	protected:
		int_type overflow(int_type ch) override {
			if (ch != traits_type::eof())
				buffer.push_back(ch);
			return 0;
		}

		std::streamsize xsputn(const char* s, std::streamsize n) override {
			buffer.append(s, static_cast<size_t>(n));
			return n;
		}
	};

	my_ostringbuf buf;
};

/// Can be used in place of std::istringstream and allows operating on a buffer without copying.
/// @note C++26 adds string_view support to istringstream, however it will still copy...
class my_istringstream : public std::istream {
public:
	/// @warning Caller must ensure string view remains valid!
	my_istringstream(std::string_view s) : std::istream(&buf), buf(s) {}

private:
	class myistring_buf : public std::streambuf {
	public:
		myistring_buf(std::string_view sv) {
			if (sv.empty()) {
				setg(nullptr, nullptr, nullptr);
			} else {
				// only takes a non-const pointer
				auto *p = const_cast<char*>(sv.data());
				setg(p, p, p + sv.size());
			}
		}
	};

	myistring_buf buf;
};
