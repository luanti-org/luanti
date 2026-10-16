// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "util/locale.h"
#include "util/string.h"
#include <functional>
#include <string>
#include <unicode/uloc.h>

static std::string to_utf8(const std::string &str)
{
	return str;
}

static std::string to_utf8(std::u16string_view str)
{
	return ustr_to_utf8(str);
}

template <typename CharT, typename... Args>
static std::string basic_getter(const std::function<int32_t(Args..., CharT* result, int32_t maxResultSize, UErrorCode *)> &func, Args... args...)
{
	UErrorCode err = U_ZERO_ERROR;
	auto outbuf_size = func(args..., NULL, 0, &err);
	if (U_FAILURE(err) && err != U_BUFFER_OVERFLOW_ERROR) {
		return "";
	}

	std::basic_string<CharT> out(outbuf_size, 0);
	err = U_ZERO_ERROR;
	func(args..., out.data(), outbuf_size, &err);

	if (U_FAILURE(err))
		return "";
	return to_utf8(out);
}

static std::string canonicalize(const std::string &id)
{
	return basic_getter<char, const char*>(uloc_canonicalize, id.data());
}

Locale::Locale(const std::string &id)
{
	name = canonicalize(id);
}

std::string Locale::getDisplayName(const Locale &in_locale) const
{
	return basic_getter<char16_t, const char*, const char*>(uloc_getDisplayName, getName().c_str(), in_locale.getName().c_str());
}
