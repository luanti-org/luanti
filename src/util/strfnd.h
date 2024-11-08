// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <string_view>

// FIXME: convert this class to string_view
template <typename T>
class BasicStrfnd {
	typedef std::basic_string<T> String;
	String str;
	size_t pos;
public:
	BasicStrfnd(const String &s) { start(s); }
	BasicStrfnd(const T *ptr) { str = ptr; pos = 0; }
	BasicStrfnd(std::basic_string_view<T> sv) { str = sv; pos = 0; }

	void start(const String &s) { str = s; pos = 0; }
	size_t where() { return pos; }
	void to(size_t i) { pos = i; }
	bool at_end() { return pos >= str.size(); }
	String what() { return str; }

	String next(const String &sep)
	{
		if (pos >= str.size())
			return String();

		size_t n;
		if (sep.empty() || (n = str.find(sep, pos)) == String::npos) {
			n = str.size();
		}
		String ret = str.substr(pos, n - pos);
		pos = n + sep.size();
		return ret;
	}

	// Returns substr up to the next occurrence of sep that isn't escaped with esc ('\\')
	String next_esc(const String &sep, T esc=static_cast<T>('\\'))
	{
		if (pos >= str.size())
			return String();

		size_t n, old_p = pos;
		do {
			if (sep.empty() || (n = str.find(sep, pos)) == String::npos) {
				pos = n = str.size();
				break;
			}
			pos = n + sep.length();
		} while (n > 0 && str[n - 1] == esc);

		return str.substr(old_p, n - old_p);
	}

	void skip_over(const String &chars)
	{
		size_t p = str.find_first_not_of(chars, pos);
		if (p != String::npos)
			pos = p;
	}
};

typedef BasicStrfnd<char> Strfnd;
typedef BasicStrfnd<wchar_t> WStrfnd;
