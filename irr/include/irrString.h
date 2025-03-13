// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine" and the "irrXML" project.
// For conditions of distribution and use, see copyright notice in irrlicht.h and irrXML.h

#pragma once

#include "irrTypes.h"
#include <string>
#include <string_view>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>

/* HACK: import these string methods from MT's util/string.h */
extern std::wstring utf8_to_wide(std::string_view input);
extern std::string wide_to_utf8(std::wstring_view input);
/* */

namespace irr
{
namespace core
{

//! Very simple string class with some useful features.
/** string<c8> and string<wchar_t> both accept Unicode AND ASCII/Latin-1,
so you can assign Unicode to string<c8> and ASCII/Latin-1 to string<wchar_t>
(and the other way round) if you want to.

However, note that the conversation between both is not done using any encoding.
This means that c8 strings are treated as ASCII/Latin-1, not UTF-8, and
are simply expanded to the equivalent wchar_t, while Unicode/wchar_t
characters are truncated to 8-bit ASCII/Latin-1 characters, discarding all
other information in the wchar_t.

Helper functions for converting between UTF-8 and wchar_t are provided
outside the string class for explicit use.
*/

// forward declarations
template <typename T>
class string;

//! Typedef for character strings
typedef string<c8> stringc;

//! Typedef for wide character strings
typedef string<wchar_t> stringw;

//! Returns a character converted to lower case
static inline u32 locale_lower(u32 x)
{
	// ansi
	return x >= 'A' && x <= 'Z' ? x + 0x20 : x;
}

//! Returns a character converted to upper case
static inline u32 locale_upper(u32 x)
{
	// ansi
	return x >= 'a' && x <= 'z' ? x + ('A' - 'a') : x;
}

template <typename T>
class string
{
	using stl_type = std::basic_string<T>;
public:
	typedef T char_type;

	//! Default constructor
	string()
	{
	}

	//! Copy constructor
	string(const string<T> &other)
	{
		*this = other;
	}

	string(const stl_type &str) : str(str) {}

	string(stl_type &&str) : str(std::move(str)) {}

	//! Constructor from other string types
	template <class B>
	string(const string<B> &other)
	{
		*this = other;
	}

	//! Constructs a string from a float
	explicit string(const double number)
	{
		c8 tmpbuf[32];
		snprintf_irr(tmpbuf, sizeof(tmpbuf), "%0.6f", number);
		str = tmpbuf;
	}

	//! Constructs a string from an int
	explicit string(int number)
	{
		str = std::to_string(number);
	}

	//! Constructs a string from an unsigned int
	explicit string(unsigned int number)
	{
		str = std::to_string(number);
	}

	//! Constructs a string from a long
	explicit string(long number)
	{
		str = std::to_string(number);
	}

	//! Constructs a string from an unsigned long
	explicit string(unsigned long number)
	{
		str = std::to_string(number);
	}

	//! Constructor for copying a string from a pointer with a given length
	template <class B>
	string(const B *const c, u32 length)
	{
		if (!c)
			return;

		str.resize(length);
		for (u32 l = 0; l < length; ++l)
			str[l] = (T)c[l];
	}

	//! Constructor for Unicode and ASCII strings
	template <class B>
	string(const B *const c)
	{
		*this = c;
	}

	//! Destructor
	~string()
	{
	}

	//! Assignment operator
	string<T> &operator=(const string<T> &other)
	{
		if (this == &other)
			return *this;

		str = other.str;
		return *this;
	}

	//! Assignment operator for other string types
	template <class B>
	string<T> &operator=(const string<B> &other)
	{
		*this = other.c_str();
		return *this;
	}

	//! Assignment operator for strings, ASCII and Unicode
	template <class B>
	string<T> &operator=(const B *const c)
	{
		if (!c) {
			clear();
			return *this;
		}

		if constexpr (sizeof(T) != sizeof(B)) {
			_IRR_DEBUG_BREAK_IF(
				(uintptr_t)c >= (uintptr_t)(str.data()) &&
				(uintptr_t)c <  (uintptr_t)(str.data() + str.size()));
		}

		if ((void *)c == (void *)c_str())
			return *this;

		u32 len = calclen(c);
		// In case `c` is a pointer to our own buffer, we may not resize first
		// or it can become invalid.
		if (len > str.size())
			str.resize(len);
		for (u32 l = 0; l < len; ++l)
			str[l] = static_cast<T>(c[l]);
		if (len < str.size())
			str.resize(len);

		return *this;
	}

	//! Append operator for other strings
	string<T> operator+(const string<T> &other) const
	{
		string<T> tmp(*this);
		tmp.append(other);

		return tmp;
	}

	//! Append operator for strings, ASCII and Unicode
	template <class B>
	string<T> operator+(const B *const c) const
	{
		string<T> tmp(*this);
		tmp.append(c);

		return tmp;
	}

	//! Direct access operator
	T &operator[](const u32 index)
	{
		return str[index];
	}

	//! Direct access operator
	const T &operator[](const u32 index) const
	{
		return str[index];
	}

	//! Equality operator
	bool operator==(const T *const other) const
	{
		if (!other)
			return false;
		return !cmp(c_str(), other);
	}

	//! Equality operator
	bool operator==(const string<T> &other) const
	{
		return str == other.str;
	}

	//! Is smaller comparator
	bool operator<(const string<T> &other) const
	{
		return str < other.str;
	}

	//! Inequality operator
	bool operator!=(const T *const other) const
	{
		return !(*this == other);
	}

	//! Inequality operator
	bool operator!=(const string<T> &other) const
	{
		return !(*this == other);
	}

	//! Returns length of the string's content
	/** \return Length of the string's content in characters, excluding
	the trailing NUL. */
	u32 size() const
	{
		return static_cast<u32>(str.size());
	}

	//! Informs if the string is empty or not.
	//! \return True if the string is empty, false if not.
	bool empty() const
	{
		return str.empty();
	}

	void clear(bool releaseMemory = true)
	{
		if (releaseMemory) {
			stl_type empty;
			std::swap(str, empty);
		} else {
			str.clear();
		}
	}

	//! Returns character string
	/** \return pointer to C-style NUL terminated string. */
	const T *c_str() const
	{
		return str.c_str();
	}

	//! Makes the string lower case.
	string<T> &make_lower()
	{
		std::transform(str.begin(), str.end(), str.begin(), [](const T &c) {
			return locale_lower(c);
		});
		return *this;
	}

	//! Makes the string upper case.
	string<T> &make_upper()
	{
		std::transform(str.begin(), str.end(), str.begin(), [](const T &c) {
			return locale_upper(c);
		});
		return *this;
	}

	//! Compares the strings ignoring case.
	/** \param other: Other string to compare.
	\return True if the strings are equal ignoring case. */
	bool equals_ignore_case(const string<T> &other) const
	{
		const T *array = c_str();
		for (u32 i = 0; array[i] && other[i]; ++i)
			if (locale_lower(array[i]) != locale_lower(other[i]))
				return false;

		return size() == other.size();
	}

	//! Compares the strings ignoring case.
	/** \param other: Other string to compare.
		\param sourcePos: where to start to compare in the string
	\return True if the strings are equal ignoring case. */
	bool equals_substring_ignore_case(const string<T> &other, const u32 sourcePos = 0) const
	{
		if (sourcePos >= size() + 1)
			return false;

		const T *array = c_str();
		u32 i;
		for (i = 0; array[sourcePos + i] && other[i]; ++i)
			if (locale_lower(array[sourcePos + i]) != locale_lower(other[i]))
				return false;

		return array[sourcePos + i] == 0 && other[i] == 0;
	}

	//! Compares the strings ignoring case.
	/** \param other: Other string to compare.
	\return True if this string is smaller ignoring case. */
	bool lower_ignore_case(const string<T> &other) const
	{
		const T *array = c_str();
		for (u32 i = 0; array[i] && other[i]; ++i) {
			s32 diff = (s32)locale_lower(array[i]) - (s32)locale_lower(other[i]);
			if (diff)
				return diff < 0;
		}

		return size() < other.size();
	}

	//! compares the first n characters of the strings
	/** \param other Other string to compare.
	\param n Number of characters to compare
	\return True if the n first characters of both strings are equal. */
	bool equalsn(const string<T> &other, u32 n) const
	{
		const T *array = c_str();
		u32 i;
		for (i = 0; i < n && array[i] && other[i]; ++i)
			if (array[i] != other[i])
				return false;

		// if one (or both) of the strings was smaller then they
		// are only equal if they have the same length
		return (i == n) || (size() == other.size());
	}

	//! compares the first n characters of the strings
	/** \param str Other string to compare.
	\param n Number of characters to compare
	\return True if the n first characters of both strings are equal. */
	bool equalsn(const T *const other, u32 n) const
	{
		if (!other)
			return false;
		const T *array = c_str();
		u32 i;
		for (i = 0; i < n && array[i] && other[i]; ++i)
			if (array[i] != other[i])
				return false;

		// if one (or both) of the strings was smaller then they
		// are only equal if they have the same length
		return (i == n) || (array[i] == 0 && other[i] == 0);
	}

	//! Appends a character to this string
	/** \param character: Character to append. */
	string<T> &append(T character)
	{
		str.append(1, character);
		return *this;
	}

	//! Appends a char string to this string
	/** \param other: Char string to append. */
	/** \param length: The length of the string to append. */
	string<T> &append(const T *const other, u32 length = 0xffffffff)
	{
		if (!other)
			return *this;

		u32 len = calclen(other);
		if (len > length)
			len = length;

		str.append(other, len);
		return *this;
	}

	//! Appends a string to this string
	/** \param other: String to append. */
	string<T> &append(const string<T> &other)
	{
		str.append(other.str);
		return *this;
	}

	//! Appends a string of the length l to this string.
	/** \param other: other String to append to this string.
	\param length: How much characters of the other string to add to this one. */
	string<T> &append(const string<T> &other, u32 length)
	{
		if (other.size() < length)
			append(other);
		else
			str.append(other.c_str(), length);
		return *this;
	}

	//! Insert a certain amount of characters into the string before the given index
	//\param pos Insert the characters before this index
	//\param s String to insert. Must be at least of size n
	//\param n Number of characters from string s to use.
	string<T> &insert(u32 pos, const T *s, u32 n)
	{
		if (pos < size() + 1) {
			str.insert(pos, s, n);
		}

		return *this;
	}

	//! Reserves some memory.
	/** \param count: Amount of characters to reserve, including
	the trailing NUL. */
	void reserve(u32 count)
	{
		if (count == 0)
			return;
		str.reserve(count - 1);
	}

	//! finds first occurrence of character in string
	/** \param c: Character to search for.
	\return Position where the character has been found,
	or -1 if not found. */
	s32 findFirst(T c) const
	{
		auto r = str.find(c);
		return pos_from_stl(r);
	}

	//! finds first occurrence of a character of a list in string
	/** \param c: List of characters to find. For example if the method
	should find the first occurrence of 'a' or 'b', this parameter should be "ab".
	\param count: Amount of characters in the list. Usually,
	this should be strlen(c)
	\return Position where one of the characters has been found,
	or -1 if not found. */
	s32 findFirstChar(const T *const c, u32 count = 1) const
	{
		if (!c || !count)
			return -1;

		auto r = str.find_first_of(c, 0, count);
		return pos_from_stl(r);
	}

	//! Finds first position of a character not in a given list.
	/** \param c: List of characters not to find. For example if the method
	should find the first occurrence of a character not 'a' or 'b', this parameter should be "ab".
	\param count: Amount of characters in the list. Usually,
	this should be strlen(c)
	\return Position where the character has been found,
	or -1 if not found. */
	s32 findFirstCharNotInList(const T *const c, u32 count = 1) const
	{
		if (!c || !count)
			return -1;

		auto r = str.find_first_not_of(c, 0, count);
		return pos_from_stl(r);
	}

	//! Finds last position of a character not in a given list.
	/** \param c: List of characters not to find. For example if the method
	should find the first occurrence of a character not 'a' or 'b', this parameter should be "ab".
	\param count: Amount of characters in the list. Usually,
	this should be strlen(c)
	\return Position where the character has been found,
	or -1 if not found. */
	s32 findLastCharNotInList(const T *const c, u32 count = 1) const
	{
		if (!c || !count)
			return -1;

		auto r = str.find_last_not_of(c, npos, count);
		return pos_from_stl(r);
	}

	//! finds next occurrence of character in string
	/** \param c: Character to search for.
	\param startPos: Position in string to start searching.
	\return Position where the character has been found,
	or -1 if not found. */
	s32 findNext(T c, u32 startPos) const
	{
		auto r = str.find(c, startPos);
		return pos_from_stl(r);
	}

	//! finds last occurrence of character in string
	/** \param c: Character to search for.
	\param start: start to search reverse ( default = -1, on end )
	\return Position where the character has been found,
	or -1 if not found. */
	s32 findLast(T c, s32 start = -1) const
	{
		auto r = str.rfind(c, pos_to_stl(start));
		return pos_from_stl(r);
	}

	//! finds last occurrence of a character of a list in string
	/** \param c: List of strings to find. For example if the method
	should find the last occurrence of 'a' or 'b', this parameter should be "ab".
	\param count: Amount of characters in the list. Usually,
	this should be strlen(c)
	\return Position where one of the characters has been found,
	or -1 if not found. */
	s32 findLastChar(const T *const c, u32 count = 1) const
	{
		if (!c || !count)
			return -1;

		auto r = str.find_last_of(c, npos, count);
		return pos_from_stl(r);
	}

	//! finds another string in this string
	/** \param str: Another string
	\param start: Start position of the search
	\return Positions where the string has been found,
	or -1 if not found. */
	s32 find(const T *const other, const u32 start = 0) const
	{
		if (other && *other) {
			auto r = str.find(other, start);
			return pos_from_stl(r);
		}

		return -1;
	}

	//! Returns a substring
	/** \param begin Start of substring.
	\param length Length of substring.
	\param make_lower copy only lower case */
	string<T> subString(u32 begin, s32 length, bool make_lower = false) const
	{
		// if start after string
		// or no proper substring length
		if ((length <= 0) || (begin >= size()))
			return string<T>("");

		string<T> o = str.substr(begin, length);
		if (make_lower)
			o.make_lower();
		return o;
	}

	//! Appends a character to this string
	/** \param c Character to append. */
	string<T> &operator+=(T c)
	{
		append(c);
		return *this;
	}

	//! Appends a char string to this string
	/** \param c Char string to append. */
	string<T> &operator+=(const T *const c)
	{
		append(c);
		return *this;
	}

	//! Appends a string to this string
	/** \param other String to append. */
	string<T> &operator+=(const string<T> &other)
	{
		append(other);
		return *this;
	}

	//! Appends a string representation of a number to this string
	/** \param i Number to append. */
	string<T> &operator+=(const int i)
	{
		append(string<T>(i));
		return *this;
	}

	//! Appends a string representation of a number to this string
	/** \param i Number to append. */
	string<T> &operator+=(const unsigned int i)
	{
		append(string<T>(i));
		return *this;
	}

	//! Appends a string representation of a number to this string
	/** \param i Number to append. */
	string<T> &operator+=(const long i)
	{
		append(string<T>(i));
		return *this;
	}

	//! Appends a string representation of a number to this string
	/** \param i Number to append. */
	string<T> &operator+=(const unsigned long i)
	{
		append(string<T>(i));
		return *this;
	}

	//! Appends a string representation of a number to this string
	/** \param i Number to append. */
	string<T> &operator+=(const double i)
	{
		append(string<T>(i));
		return *this;
	}

	//! Appends a string representation of a number to this string
	/** \param i Number to append. */
	string<T> &operator+=(const float i)
	{
		append(string<T>(i));
		return *this;
	}

	//! Replaces all characters of a special type with another one
	/** \param toReplace Character to replace.
	\param replaceWith Character replacing the old one. */
	string<T> &replace(T toReplace, T replaceWith)
	{
		std::replace(str.begin(), str.end(), toReplace, replaceWith);
		return *this;
	}

	//! Replaces all instances of a string with another one.
	/** \param toReplace The string to replace.
	\param replaceWith The string replacing the old one. */
	string<T> &replace(const string<T> &toReplace, const string<T> &replaceWith)
	{
		size_type pos = 0;
		while ((pos = str.find(toReplace.str, pos)) != npos) {
			str.replace(pos, toReplace.size(), replaceWith.str);
			pos += replaceWith.size();
		}

		return *this;
	}

	//! Removes a character from a string.
	/** \param c: Character to remove. */
	string<T> &remove(T c)
	{
		str.erase(std::remove(str.begin(), str.end(), c), str.end());
		return *this;
	}

	//! Removes a string from the string.
	/** \param toRemove: String to remove. */
	string<T> &remove(const string<T> &toRemove)
	{
		u32 size = toRemove.size();
		if (size == 0)
			return *this;
		u32 pos = 0;
		u32 found = 0;
		for (u32 i = 0; i < str.size(); ++i) {
			u32 j = 0;
			while (j < size) {
				if (str[i + j] != toRemove[j])
					break;
				++j;
			}
			if (j == size) {
				found += size;
				i += size - 1;
				continue;
			}

			str[pos++] = str[i];
		}
		str.resize(str.size() - found);
		return *this;
	}

	//! Removes characters from a string.
	/** \param characters: Characters to remove. */
	string<T> &removeChars(const string<T> &characters)
	{
		if (characters.size() == 0)
			return *this;

		for (u32 i = 0; i < characters.size(); i++)
			remove(characters[i]);
		return *this;
	}

	//! Trims the string.
	/** Removes the specified characters (by default, Latin-1 whitespace)
	from the beginning and the end of the string. */
	string<T> &trim(const string<T> &whitespace = " \t\n\r")
	{
		// find start and end of the substring without the specified characters
		const s32 begin = findFirstCharNotInList(whitespace.c_str(), whitespace.size());
		if (begin == -1)
			return (*this = "");

		const s32 end = findLastCharNotInList(whitespace.c_str(), whitespace.size());

		return (*this = subString(begin, (end + 1) - begin));
	}

	//! Erases a character from the string.
	/** May be slow, because all elements
	following after the erased element have to be copied.
	\param index: Index of element to be erased. */
	string<T> &erase(u32 index)
	{
		str.erase(str.begin() + index);
		return *this;
	}

	//! verify the existing string.
	string<T> &validate()
	{
		// truncate to existing null
		u32 len = calclen(c_str());
		if (len != size())
			str.resize(len);

		return *this;
	}

	//! gets the last char of a string or null
	T lastChar() const
	{
		return !str.empty() ? str.back() : 0;
	}

	//! Split string into parts (tokens).
	/** This method will split a string at certain delimiter characters
	into the container passed in as reference. The type of the container
	has to be given as template parameter. It must provide a push_back and
	a size method.
	\param ret The result container. Tokens are added, the container is not cleared.
	\param delimiter C-style string of delimiter characters
	\param countDelimiters Number of delimiter characters
	\param ignoreEmptyTokens Flag to avoid empty substrings in the result
	container. If two delimiters occur without a character in between or an
	empty substring would be placed in the result. Or if a delimiter is the last
	character an empty substring would be added at the end.	If this flag is set,
	only non-empty strings are stored.
	\param keepSeparators Flag which allows to add the separator to the
	result string. If this flag is true, the concatenation of the
	substrings results in the original string. Otherwise, only the
	characters between the delimiters are returned.
	\return The number of resulting substrings
	*/
	template <class container>
	u32 split(container &ret, const T *const delimiter, u32 countDelimiters = 1, bool ignoreEmptyTokens = true, bool keepSeparators = false) const
	{
		if (!delimiter)
			return 0;

		const u32 oldSize = static_cast<u32>(ret.size());

		u32 tokenStartIdx = 0;
		for (u32 i = 0; i < size() + 1; ++i) {
			for (u32 j = 0; j < countDelimiters; ++j) {
				if (str[i] == delimiter[j]) {
					if (i - tokenStartIdx > 0)
						ret.push_back(string<T>(&str[tokenStartIdx], i - tokenStartIdx));
					else if (!ignoreEmptyTokens)
						ret.push_back(string<T>());
					if (keepSeparators) {
						ret.push_back(string<T>(&str[i], 1));
					}

					tokenStartIdx = i + 1;
					break;
				}
			}
		}
		if (size() > tokenStartIdx)
			ret.push_back(string<T>(&str[tokenStartIdx], size() - tokenStartIdx));
		else if (!ignoreEmptyTokens)
			ret.push_back(string<T>());

		return static_cast<u32>(ret.size() - oldSize);
	}

	// This function should not be used and is only kept for "CGUIFileOpenDialog::pathToStringW".
	friend size_t multibyteToWString(stringw &destination, const stringc &source);

	friend size_t utf8ToWString(stringw &destination, const char *source);
	friend size_t wStringToUTF8(stringc &destination, const wchar_t *source);

private:

	//! strlen wrapper
	template <typename U>
	static inline u32 calclen(const U *p)
	{
		u32 len = 0;
		while (*p++)
			len++;
		return len;
	}
	static inline u32 calclen(const char *p)
	{
		return static_cast<u32>(strlen(p));
	}
	static inline u32 calclen(const wchar_t *p)
	{
		return static_cast<u32>(wcslen(p));
	}

	//! strcmp wrapper
	template <typename U>
	static inline int cmp(const U *p, const U *p2)
	{
		while (*p && *p == *p2)
			p++, p2++;
		return (int)*p - (int)*p2;
	}
	static inline int cmp(const char *p, const char *p2)
	{
		return strcmp(p, p2);
	}
	static inline int cmp(const wchar_t *p, const wchar_t *p2)
	{
		return wcscmp(p, p2);
	}

	typedef typename stl_type::size_type size_type;
	static const size_type npos = stl_type::npos;

	static inline s32 pos_from_stl(size_type pos)
	{
		return pos == npos ? -1 : (s32)pos;
	}
	static inline size_type pos_to_stl(s32 pos)
	{
		return pos == -1 ? npos : (size_type)pos;
	}

	stl_type str;
};

//! Convert multibyte string to wide-character string
/** Wrapper around mbstowcs from standard library, but directly using Irrlicht string class.
What the function does exactly depends on the LC_CTYPE of the current c locale.
\param destination Wide-character string receiving the converted source
\param source multibyte string
\return The number of wide characters written to destination, not including the eventual terminating null character or -1 when conversion failed

This function should not be used and is only kept for "CGUIFileOpenDialog::pathToStringW". */
inline size_t multibyteToWString(stringw &destination, const core::stringc &source)
{
	u32 sourceSize = source.size();

	if (sourceSize) {
		destination.str.resize(sourceSize + 1);
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996) // 'mbstowcs': This function or variable may be unsafe. Consider using mbstowcs_s instead.
#endif
		const size_t written = mbstowcs(&destination[0], source.c_str(), (size_t)sourceSize);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
		if (written != (size_t)-1) {
			destination.str.resize(written);
		} else {
			// Likely character which got converted until the invalid character was encountered are in destination now.
			// And it seems even 0-terminated, but I found no documentation anywhere that this (the 0-termination) is guaranteed :-(
			destination.clear();
		}
		return written;
	} else {
		destination.clear();
		return 0;
	}
}

inline size_t utf8ToWString(stringw &destination, const char *source)
{
	destination = utf8_to_wide(source);
	return destination.size();
}

inline size_t utf8ToWString(stringw &destination, const stringc &source)
{
	return utf8ToWString(destination, source.c_str());
}

inline size_t wStringToUTF8(stringc &destination, const wchar_t *source)
{
	destination = wide_to_utf8(source);
	return destination.size();
}

inline size_t wStringToUTF8(stringc &destination, const stringw &source)
{
	return wStringToUTF8(destination, source.c_str());
}

} // end namespace core
} // end namespace irr
