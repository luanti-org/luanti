// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "config.h" // for USE_GETTEXT
#include "porting.h"
#include "util/string.h"

#if USE_GETTEXT
	#include <libintl.h>
#else
	// In certain environments, some standard headers like <iomanip>
	// and <locale> include libintl.h. If libintl.h is included after
	// we define our gettext macro below, this causes a syntax error
	// at the declaration of the gettext function in libintl.h.
	// Fix this by including such a header before defining the macro.
	// See issue #4446.
	// Note that we can't include libintl.h directly since we're in
	// the USE_GETTEXT=0 case and can't assume that gettext is installed.
	#include <locale>

	#define gettext(String) (String)
	#define ngettext(String1, String2, n) ((n) == 1 ? (String1) : (String2))
#endif

#define _(String) gettext(String)
#define gettext_noop(String) (String)
#define N_(String) gettext_noop((String))

void init_gettext(const char *path, const std::string &configured_language,
	int argc, char *argv[]);

inline std::string strgettext(const char *str)
{
	// We must check here that is not an empty string to avoid trying to translate it
	return str[0] ? gettext(str) : "";
}

inline std::string strgettext(const std::string &str)
{
	return strgettext(str.c_str());
}

inline std::wstring wstrgettext(const char *str)
{
	return utf8_to_wide(strgettext(str));
}

inline std::wstring wstrgettext(const std::string &str)
{
	return wstrgettext(str.c_str());
}

/**
 * Returns translated string with format args applied
 *
 * @tparam Args Template parameter for format args
 * @param src Translation source string
 * @param args Variable format args
 * @warning No dynamic sizing! string will be cut off if longer than 255 chars.
 * @return translated string
 */
template <typename ...Args>
inline std::wstring fwgettext(const char *src, Args&&... args)
{
	wchar_t buf[255];
	swprintf(buf, sizeof(buf) / sizeof(wchar_t), wstrgettext(src).c_str(),
			std::forward<Args>(args)...);
	return std::wstring(buf);
}

/**
 * Returns translated string with format args applied
 *
 * @tparam Args Template parameter for format args
 * @param format Translation source string
 * @param args Variable format args
 * @return translated string.
 */
template <typename ...Args>
inline std::string fmtgettext(const char *format, Args&&... args)
{
	format = gettext(format);

	std::string buf;
	{
		size_t default_size = strlen(format);
		if (default_size < 256)
			default_size = 256;
		buf.resize(default_size);
	}

	int len = porting::mt_snprintf(&buf[0], buf.size(), format, std::forward<Args>(args)...);
	if (len <= 0)
		throw std::runtime_error("gettext format error: " + std::string(format));
	if ((size_t)len >= buf.size()) {
		buf.resize(len+1); // extra null byte
		porting::mt_snprintf(&buf[0], buf.size(), format, std::forward<Args>(args)...);
	}
	buf.resize(len); // remove null bytes

	return buf;
}
