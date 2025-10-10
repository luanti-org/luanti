// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "config.h" // IS_CLIENT_BUILD
#if IS_CLIENT_BUILD
#include "irrString.h"
#endif
#include <cstdlib>
#include <string>
#include <string_view>
#include <cstring>
#include <vector>
#include <limits>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cwctype>
#include <unordered_map>
#include <optional>

class Translations;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Checks whether a value is an ASCII printable character
#define IS_ASCII_PRINTABLE_CHAR(x)   \
	(((unsigned int)(x) >= 0x20) &&  \
	( (unsigned int)(x) <= 0x7e))

// Checks whether a value is in a Unicode private use area
#define IS_PRIVATE_USE_CHAR16(x)    \
	((wchar_t)(x) >= 0xE000 &&   \
	  (wchar_t)(x) <= 0xF8FF)
#define IS_PRIVATE_USE_CHAR32(x)    \
	(((wchar_t)(x) >= 0xF0000 &&  \
	  (wchar_t)(x) <= 0xFFFFD) || \
	 ((wchar_t)(x) >= 0x100000 && \
	  (wchar_t)(x) <= 0x10FFFD))
#if WCHAR_MAX > 0xFFFF
#define IS_PRIVATE_USE_CHAR(x) (IS_PRIVATE_USE_CHAR16(x) || IS_PRIVATE_USE_CHAR32(x))
#else
#define IS_PRIVATE_USE_CHAR(x) IS_PRIVATE_USE_CHAR16(x)
#endif

// Checks whether a byte is an inner byte for an utf-8 multibyte sequence
#define IS_UTF8_MULTB_INNER(x)       \
	(((unsigned char)(x) >= 0x80) && \
	( (unsigned char)(x) <= 0xbf))

// Checks whether a byte is a start byte for an utf-8 multibyte sequence
#define IS_UTF8_MULTB_START(x)       \
	(((unsigned char)(x) >= 0xc2) && \
	( (unsigned char)(x) <= 0xf4))

// Given a start byte x for an utf-8 multibyte sequence
// it gives the length of the whole sequence in bytes.
#define UTF8_MULTB_START_LEN(x)            \
	(((unsigned char)(x) < 0xe0) ? 2 :     \
	(((unsigned char)(x) < 0xf0) ? 3 : 4))

typedef std::unordered_map<std::string, std::string> StringMap;

struct FlagDesc {
	const char *name;
	u32 flag;
};

// Try to avoid converting between wide and UTF-8 unless you need to
// input/output stuff via Irrlicht
[[nodiscard]] std::wstring utf8_to_wide(std::string_view input);
[[nodiscard]] std::string wide_to_utf8(std::wstring_view input);

void wide_add_codepoint(std::wstring &result, char32_t codepoint);

std::string urlencode(std::string_view str);
std::string urldecode(std::string_view str);

u32 readFlagString(std::string str, const FlagDesc *flagdesc, u32 *flagmask);
std::string writeFlagString(u32 flags, const FlagDesc *flagdesc, u32 flagmask);

size_t mystrlcpy(char *dst, const char *src, size_t size) noexcept;

/// @brief turn string into a map seed. either directly if it's a number or by hashing it.
u64 read_seed(const char *str);

bool parseColorString(const std::string &value, video::SColor &color, bool quiet,
		unsigned char default_alpha = 0xff);
std::string encodeHexColorString(video::SColor color);

/**
 * Converts a letter to lowercase, with safe handling of the char type and non-ASCII.
 * @param c input letter
 * @returns same letter but lowercase
*/
inline char my_tolower(char c)
{
	// By design this function cannot handle any Unicode (codepoints don't fit into char),
	// but make sure to pass it through unchanged.
	// tolower() can mangle it if the POSIX locale is not UTF-8.
	if (static_cast<unsigned char>(c) > 0x7f)
		return c;
	// toupper(3): "If the argument c is of type char, it must be cast to unsigned char"
	return tolower(static_cast<unsigned char>(c));
}

/**
 * Returns a copy of \p str with spaces inserted at the right hand side to ensure
 * that the string is \p len characters in length. If \p str is <= \p len then the
 * returned string will be identical to str.
 */
inline std::string padStringRight(std::string str, size_t len)
{
	if (len > str.size())
		str.insert(str.end(), len - str.size(), ' ');

	return str;
}

/**
 * Returns a version of \p str with the first occurrence of a string
 * contained within ends[] removed from the end of the string.
 *
 * @param str
 * @param ends A NULL- or ""- terminated array of strings to remove from s in
 *	the copy produced.  Note that once one of these strings is removed
 *	that no further postfixes contained within this array are removed.
 *
 * @return If no end could be removed then "" is returned.
 */
inline std::string_view removeStringEnd(std::string_view str,
		const char *ends[])
{
	const char **p = ends;

	for (; *p && (*p)[0] != '\0'; p++) {
		std::string_view end(*p);
		if (str.size() < end.size())
			continue;
		if (str.compare(str.size() - end.size(), end.size(), end) == 0)
			return str.substr(0, str.size() - end.size());
	}

	return std::string_view();
}


#define MAKE_VARIANT(_name, _t0, _t1) \
	template <typename T, typename... Args> \
	inline auto _name(_t0 arg1, _t1 arg2, Args&&... args) \
	{ \
		return (_name)(std::basic_string_view<T>(arg1), std::basic_string_view<T>(arg2), \
			std::forward<Args>(args)...); \
	}


/**
 * Check two strings for equivalence.  If \p case_insensitive is true
 * then the case of the strings is ignored (default is false).
 *
 * @param s1
 * @param s2
 * @param case_insensitive
 * @return true if the strings match
 */
template <typename T>
inline bool str_equal(std::basic_string_view<T> s1,
		std::basic_string_view<T> s2,
		bool case_insensitive = false)
{
	if (!case_insensitive)
		return s1 == s2;

	if (s1.size() != s2.size())
		return false;

	for (size_t i = 0; i < s1.size(); ++i)
		if (my_tolower(s1[i]) != my_tolower(s2[i]))
			return false;

	return true;
}

// For some reason an std::string will not implicitly get converted
// to an std::basic_string_view<char> in the template case above, so we need
// these three wrappers. It works if you take out the template parameters.
// see also <https://stackoverflow.com/questions/68380141/>
MAKE_VARIANT(str_equal, const std::basic_string<T> &, const std::basic_string<T> &)

MAKE_VARIANT(str_equal, std::basic_string_view<T>, const std::basic_string<T> &)

MAKE_VARIANT(str_equal, const std::basic_string<T> &, std::basic_string_view<T>)


/**
 * Check whether \p str begins with the string prefix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param prefix
 * @param case_insensitive
 * @return true if the str begins with prefix
 */
template <typename T>
inline bool str_starts_with(std::basic_string_view<T> str,
		std::basic_string_view<T> prefix,
		bool case_insensitive = false)
{
	if (str.size() < prefix.size())
		return false;

	if (!case_insensitive)
		return str.compare(0, prefix.size(), prefix) == 0;

	for (size_t i = 0; i < prefix.size(); ++i)
		if (my_tolower(str[i]) != my_tolower(prefix[i]))
			return false;
	return true;
}

// (same conversion issue here)
MAKE_VARIANT(str_starts_with, const std::basic_string<T> &, const std::basic_string<T> &)

MAKE_VARIANT(str_starts_with, std::basic_string_view<T>, const std::basic_string<T> &)

MAKE_VARIANT(str_starts_with, const std::basic_string<T> &, std::basic_string_view<T>)

// (the same but with char pointers, only for the prefix argument)
MAKE_VARIANT(str_starts_with, const std::basic_string<T> &, const T*)

MAKE_VARIANT(str_starts_with, std::basic_string_view<T>, const T*)


/**
 * Check whether \p str ends with the string suffix. If \p case_insensitive
 * is true then the check is case insensitve (default is false; i.e. case is
 * significant).
 *
 * @param str
 * @param suffix
 * @param case_insensitive
 * @return true if the str begins with suffix
 */
template <typename T>
inline bool str_ends_with(std::basic_string_view<T> str,
		std::basic_string_view<T> suffix,
		bool case_insensitive = false)
{
	if (str.size() < suffix.size())
		return false;

	size_t start = str.size() - suffix.size();
	if (!case_insensitive)
		return str.compare(start, suffix.size(), suffix) == 0;

	for (size_t i = 0; i < suffix.size(); ++i)
		if (my_tolower(str[start + i]) != my_tolower(suffix[i]))
			return false;
	return true;
}

// (same conversion issue here)
MAKE_VARIANT(str_ends_with, const std::basic_string<T> &, const std::basic_string<T> &)

MAKE_VARIANT(str_ends_with, std::basic_string_view<T>, const std::basic_string<T> &)

MAKE_VARIANT(str_ends_with, const std::basic_string<T> &, std::basic_string_view<T>)

// (the same but with char pointers, only for the suffix argument)
MAKE_VARIANT(str_ends_with, const std::basic_string<T> &, const T*)

MAKE_VARIANT(str_ends_with, std::basic_string_view<T>, const T*)


#undef MAKE_VARIANT


/**
 * Splits a string into its component parts separated by the character
 * \p delimiter.
 *
 * @return An std::vector<std::basic_string<T> > of the component parts
 */
template <typename T>
[[nodiscard]]
inline std::vector<std::basic_string<T> > str_split(
		const std::basic_string<T> &str,
		T delimiter)
{
	std::vector<std::basic_string<T> > parts;
	std::basic_stringstream<T> sstr(str);
	std::basic_string<T> part;

	while (std::getline(sstr, part, delimiter))
		parts.push_back(part);

	return parts;
}


/**
 * @param str
 * @return A copy of \p str converted to all lowercase characters.
 */
[[nodiscard]]
inline std::string lowercase(std::string_view str)
{
	std::string s2;
	s2.resize(str.size());
	for (size_t i = 0; i < str.size(); i++)
		s2[i] = my_tolower(str[i]);
	return s2;
}


inline bool my_isspace(const char c)
{
	return std::isspace(c);
}

inline bool my_isspace(const wchar_t c)
{
	return std::iswspace(c);
}

/**
 * @param str
 * @return A view of \p str with leading and trailing whitespace removed.
 */
template<typename T>
[[nodiscard]]
inline std::basic_string_view<T> trim(std::basic_string_view<T> str)
{
	size_t front = 0;
	size_t back = str.size();

	while (front < back && my_isspace(str[front]))
		++front;

	while (back > front && my_isspace(str[back - 1]))
		--back;

	return str.substr(front, back - front);
}

// If input was a temporary string keep it one to make sure patterns like
// trim(func_that_returns_str()) are predictable regarding memory allocation
// and don't lead to UAF. ↓ ↓ ↓

/**
 * @param str
 * @return A copy of \p str with leading and trailing whitespace removed.
 */
template<typename T>
[[nodiscard]]
inline std::basic_string<T> trim(std::basic_string<T> &&str)
{
	std::basic_string<T> ret(trim(std::basic_string_view<T>(str)));
	return ret;
}

template<typename T>
[[nodiscard]]
inline std::basic_string_view<T> trim(const std::basic_string<T> &str)
{
	return trim(std::basic_string_view<T>(str));
}

// The above declaration causes ambiguity with char pointers so we have to fix that:
template<typename T>
[[nodiscard]]
inline std::basic_string_view<T> trim(const T *str)
{
	return trim(std::basic_string_view<T>(str));
}


/**
 * Returns whether \p str should be regarded as (bool) true.  Case and leading
 * and trailing whitespace are ignored.  Values that will return
 * true are "y", "yes", "true" and any number that is not 0.
 * @param str
 */
inline bool is_yes(std::string_view str)
{
	std::string s2 = lowercase(trim(str));

	return s2 == "y" || s2 == "yes" || s2 == "true" || atoi(s2.c_str()) != 0;
}


/**
 * Converts the string \p str to a signed 32-bit integer. The converted value
 * is constrained so that min <= value <= max.
 *
 * @see atoi(3) for limitations
 *
 * @param str
 * @param min Range minimum
 * @param max Range maximum
 * @return The value converted to a signed 32-bit integer and constrained
 *	within the range defined by min and max (inclusive)
 */
inline s32 mystoi(const std::string &str, s32 min, s32 max)
{
	s32 i = atoi(str.c_str());

	if (i < min)
		i = min;
	if (i > max)
		i = max;

	return i;
}

/**
 * Returns a 32-bit value reprensented by the string \p str (decimal).
 * @see atoi(3) for further limitations
 */
inline s32 mystoi(const std::string &str)
{
	return atoi(str.c_str());
}

/**
 * Returns a float reprensented by the string \p str (decimal).
 * @see atof(3)
 */
inline float mystof(const std::string &str)
{
	return atof(str.c_str());
}

#define stoi mystoi
#define stof mystof

/// Returns a value represented by the string \p val.
template <typename T>
inline T from_string(const std::string &str)
{
	std::istringstream tmp(str);
	T t;
	tmp >> t;
	return t;
}

/// Returns a 64-bit signed value represented by the string \p str (decimal).
inline s64 stoi64(const std::string &str) { return from_string<s64>(str); }

/// Returns a string representing the decimal value of the 32-bit value \p i.
inline std::string itos(s32 i) { return std::to_string(i); }
/// Returns a string representing the decimal value of the 64-bit value \p i.
inline std::string i64tos(s64 i) { return std::to_string(i); }

/// Returns a string representing the exact decimal value of the float value \p f.
inline std::string ftos(float f)
{
	std::ostringstream oss;
	oss << std::setprecision(std::numeric_limits<float>::max_digits10) << f;
	return oss.str();
}

/// @brief Converts double to string. Handles high precision and inf/nan.
std::string my_double_to_string(double number);
/// @brief Converts string to double. Handles high precision and inf/nan.
std::optional<double> my_string_to_double(const std::string &s);

/**
 * Replace all occurrences of \p pattern in \p str with \p replacement.
 *
 * @param str String to replace pattern with replacement within.
 * @param pattern The pattern to replace.
 * @param replacement What to replace the pattern with.
 */
inline void str_replace(std::string &str, std::string_view pattern,
		std::string_view replacement)
{
	std::string::size_type start = str.find(pattern, 0);
	while (start != str.npos) {
		str.replace(start, pattern.size(), replacement);
		start = str.find(pattern, start + replacement.size());
	}
}

/**
 * Escapes characters that cannot be used in formspecs
 */
inline void str_formspec_escape(std::string &str)
{
	str_replace(str, "\\", "\\\\");
	str_replace(str, "]", "\\]");
	str_replace(str, "[", "\\[");
	str_replace(str, ";", "\\;");
	str_replace(str, ",", "\\,");
	str_replace(str, "$", "\\$");
}

/**
 * Replace all occurrences of the character \p from in \p str with \p to.
 *
 * @param str The string to (potentially) modify.
 * @param from The character in str to replace.
 * @param to The replacement character.
 */
void str_replace(std::string &str, char from, char to);


/**
 * Check that a string only contains whitelisted characters. This is the
 * opposite of string_allowed_blacklist().
 *
 * @param str The string to be checked.
 * @param allowed_chars A string containing permitted characters.
 * @return true if the string is allowed, otherwise false.
 *
 * @see string_allowed_blacklist()
 */
inline bool string_allowed(std::string_view str, std::string_view allowed_chars)
{
	return str.find_first_not_of(allowed_chars) == str.npos;
}


/**
 * Check that a string contains no blacklisted characters. This is the
 * opposite of string_allowed().
 *
 * @param str The string to be checked.
 * @param blacklisted_chars A string containing prohibited characters.
 * @return true if the string is allowed, otherwise false.

 * @see string_allowed()
 */
inline bool string_allowed_blacklist(std::string_view str,
		std::string_view blacklisted_chars)
{
	return str.find_first_of(blacklisted_chars) == str.npos;
}


/**
 * Create a string based on \p from where a newline is forcefully inserted
 * every \p row_len characters.
 *
 * @note This function does not honour word wraps and blindly inserts a newline
 *	every \p row_len characters whether it breaks a word or not.  It is
 *	intended to be used for, for example, showing paths in the GUI.
 *
 * @note This function doesn't wrap inside utf-8 multibyte sequences and also
 * 	counts multibyte sequences correcly as single characters.
 *
 * @param from The (utf-8) string to be wrapped into rows.
 * @param row_len The row length (in characters).
 * @param has_color_codes Whether the source string has colorize codes.
 * @return A new string with the wrapping applied.
 */
std::string wrap_rows(std::string_view from, unsigned row_len, bool has_color_codes = false);


/**
 * Removes backslashes from an escaped string (FormSpec strings)
 */
template <typename T>
[[nodiscard]]
inline std::basic_string<T> unescape_string(const std::basic_string<T> &s)
{
	std::basic_string<T> res;
	res.reserve(s.size());

	for (size_t i = 0; i < s.length(); i++) {
		if (s[i] == '\\') {
			i++;
			if (i >= s.length())
				break;
		}
		res += s[i];
	}

	return res;
}

/**
 * Remove all escape sequences in \p s.
 *
 * @param s The string in which to remove escape sequences.
 * @return \p s, with escape sequences removed.
 */
template <typename T>
[[nodiscard]]
std::basic_string<T> unescape_enriched(std::basic_string_view<T> s)
{
	std::basic_string<T> output;
	output.reserve(s.size());
	size_t i = 0;
	while (i < s.length()) {
		if (s[i] == static_cast<T>('\x1b')) {
			++i;
			if (i == s.length())
				continue;
			if (s[i] == static_cast<T>('(')) {
				++i;
				while (i < s.length() && s[i] != static_cast<T>(')')) {
					if (s[i] == static_cast<T>('\\'))
						++i;
					++i;
				}
			}
			++i;
			continue;
		}
		output += s[i];
		++i;
	}
	return output;
}

// (same templating issue here)
[[nodiscard]]
inline std::string unescape_enriched(std::string_view s)
{
	return unescape_enriched<char>(s);
}
[[nodiscard]]
inline std::wstring unescape_enriched(std::wstring_view s)
{
	return unescape_enriched<wchar_t>(s);
}

template <typename T>
[[nodiscard]]
std::vector<std::basic_string<T> > split(const std::basic_string<T> &s, T delim)
{
	std::vector<std::basic_string<T> > tokens;

	std::basic_string<T> current;
	bool last_was_escape = false;
	for (size_t i = 0; i < s.length(); i++) {
		T si = s[i];
		if (last_was_escape) {
			current += '\\';
			current += si;
			last_was_escape = false;
		} else {
			if (si == delim) {
				tokens.push_back(current);
				current.clear();
				last_was_escape = false;
			} else if (si == '\\') {
				last_was_escape = true;
			} else {
				current += si;
				last_was_escape = false;
			}
		}
	}
	//push last element
	tokens.push_back(current);

	return tokens;
}

[[nodiscard]]
std::wstring translate_string(std::wstring_view s, Translations *translations);

[[nodiscard]]
std::wstring translate_string(std::wstring_view s);

[[nodiscard]]
inline std::wstring unescape_translate(std::wstring_view s)
{
	return unescape_enriched(translate_string(s));
}

/**
 * Checks that all characters in \p to_check are a decimal digits.
 *
 * @param to_check
 * @return true if to_check is not empty and all characters in to_check are
 *	decimal digits, otherwise false
 */
inline bool is_number(std::string_view to_check)
{
	for (char i : to_check)
		if (!std::isdigit(i))
			return false;

	return !to_check.empty();
}


/**
 * Returns a C-string, either "true" or "false", corresponding to \p val.
 *
 * @return If \p val is true, then "true" is returned, otherwise "false".
 */
inline const char *bool_to_cstr(bool val)
{
	return val ? "true" : "false";
}

/**
 * Converts a duration in seconds to a pretty-printed duration in
 * days, hours, minutes and seconds.
 *
 * @param sec duration in seconds
 * @return pretty-printed duration
 */
inline const std::string duration_to_string(int sec)
{
	std::ostringstream ss;
	const char *neg = "";
	if (sec < 0) {
		sec = -sec;
		neg = "-";
	}
	int total_sec = sec;
	int min = sec / 60;
	sec %= 60;
	int hour = min / 60;
	min %= 60;
	int day = hour / 24;
	hour %= 24;

	if (day > 0) {
		ss << neg << day << "d";
		if (hour > 0 || min > 0 || sec > 0)
			ss << " ";
	}

	if (hour > 0) {
		ss << neg << hour << "h";
		if (min > 0 || sec > 0)
			ss << " ";
	}

	if (min > 0) {
		ss << neg << min << "min";
		if (sec > 0)
			ss << " ";
	}

	if (sec > 0 || total_sec == 0) {
		ss << neg << sec << "s";
	}

	return ss.str();
}

/**
 * Joins a vector of strings by the string \p delimiter.
 *
 * @return A std::string
 */
[[nodiscard]]
inline std::string str_join(const std::vector<std::string> &list,
		std::string_view delimiter)
{
	std::ostringstream oss;
	bool first = true;
	for (const auto &part : list) {
		if (!first)
			oss << delimiter;
		oss << part;
		first = false;
	}
	return oss.str();
}

#if IS_CLIENT_BUILD
/**
 * Create a UTF8 std::string from an core::stringw.
 */
[[nodiscard]]
inline std::string stringw_to_utf8(const core::stringw &input)
{
	std::wstring_view sv(input.c_str(), input.size());
	return wide_to_utf8(sv);
}

/**
 * Create an core:stringw from a UTF8 std::string.
 */
[[nodiscard]]
inline core::stringw utf8_to_stringw(std::string_view input)
{
	std::wstring str = utf8_to_wide(input);
	return core::stringw(str.c_str(), str.size());
}
#endif

/**
 * Sanitize the name of a new directory. This consists of two stages:
 * 1. Check for 'reserved filenames' that can't be used on some filesystems
 *    and add a prefix to them
 * 2. Remove 'unsafe' characters from the name by replacing them with '_'
 */
[[nodiscard]]
std::string sanitizeDirName(std::string_view str, std::string_view optional_prefix);

/**
 * Sanitize an untrusted string (e.g. from the network). This will get strip
 * control characters and (optionally) any MT-style escape sequences too.
 * Note that they won't be removed cleanly but rather just broken, unlike with
 * unescape_enriched.
 * Line breaks and UTF-8 is permitted.
 */
[[nodiscard]]
std::string sanitize_untrusted(std::string_view str, bool keep_escapes = true);

/**
 * Prints a sanitized version of a string without control characters.
 * '\t' and '\n' are allowed, as are UTF-8 control characters (e.g. RTL).
 * ASCII control characters are replaced with their hex encoding in angle
 * brackets (e.g. "a\x1eb" -> "a<1e>b").
 */
void safe_print_string(std::ostream &os, std::string_view str);

/**
 * Parses a string of form `(1, 2, 3)` or `1, 2, 4` to a v3f
 *
 * @param str string
 * @return float vector
 */
std::optional<v3f> str_to_v3f(std::string_view str);
