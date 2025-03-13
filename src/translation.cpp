// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 Nore, Nathanaëlle Courant <nore@mesecons.net>

#include "translation.h"
#include "log.h"
#include "util/hex.h"
#include "util/string.h"
#include "gettext.h"
#include <unordered_map>


#if CHECK_CLIENT_BUILD()
// Client translations
static Translations client_translations;
Translations *g_client_translations = &client_translations;
#else
Translations *g_client_translations = nullptr;
#endif

const std::string_view Translations::getFileLanguage(const std::string &filename)
{
	const char *translate_ext[] = {
		".tr", ".po", ".mo", NULL
	};
	auto basename = removeStringEnd(filename, translate_ext);
	auto pos = basename.rfind('.');
	if (pos == basename.npos)
		return "";
	return basename.substr(pos+1);
}

void Translations::clear()
{
	m_translations.clear();
	m_plural_translations.clear();
}

const std::wstring &Translations::getTranslation(
		const std::wstring &textdomain, const std::wstring &s) const
{
	std::wstring key = textdomain + L"|" + s;
	auto it = m_translations.find(key);
	if (it != m_translations.end())
		return it->second;
	return s;
}

const std::wstring &Translations::getPluralTranslation(
		const std::wstring &textdomain, const std::wstring &s, unsigned long int number) const
{
	std::wstring key = textdomain + L"|" + s;
	auto it = m_plural_translations.find(key);
	if (it != m_plural_translations.end()) {
		auto n = (*(it->second.first))(number);
		const std::vector<std::wstring> &v = it->second.second;
		if (n < v.size()) {
			if (v[n].empty())
				return s;
			return v[n];
		}
	}
	return s;
}


void Translations::addTranslation(
		const std::wstring &textdomain, const std::wstring &original, const std::wstring &translated)
{
	std::wstring key = textdomain + L"|" + original;
	if (!translated.empty()) {
		m_translations.emplace(std::move(key), std::move(translated));
	}
}

void Translations::addPluralTranslation(
		const std::wstring &textdomain, const GettextPluralForm::Ptr &plural, const std::wstring &original, std::vector<std::wstring> &translated)
{
	static bool warned = false;
	if (!plural) {
		warned = true;
		if (!warned)
			errorstream << "Translations: plural translation entry defined without Plural-Forms" << std::endl;
		return;
	} else if (translated.size() != plural->size()) {
		errorstream << "Translations: incorrect number of plural translations (expected " << plural->size() << ", got " << translated.size() << ")" << std::endl;
		return;
	}
	std::wstring key = textdomain + L"|" + original;
	m_plural_translations.emplace(std::move(key), std::pair(plural, translated));
}


void Translations::loadTrTranslation(const std::string &data)
{
	std::istringstream is(data);
	std::string textdomain_narrow;
	std::wstring textdomain;
	std::string line;

	while (is.good()) {
		std::getline(is, line);
		// Trim last character if file was using a \r\n line ending
		if (line.length () > 0 && line[line.length() - 1] == '\r')
			line.resize(line.length() - 1);

		if (str_starts_with(line, "# textdomain:")) {
			auto parts = str_split(line, ':');
			if (parts.size() < 2) {
				errorstream << "Invalid textdomain translation line \"" << line
						<< "\"" << std::endl;
				continue;
			}
			textdomain_narrow = trim(parts[1]);
			textdomain = utf8_to_wide(textdomain_narrow);
		}
		if (line.empty() || line[0] == '#')
			continue;

		std::wstring wline = utf8_to_wide(line);
		if (wline.empty())
			continue;

		// Read line
		// '=' marks the key-value pair, but may be escaped by an '@'.
		// '\n' may also be escaped by '@'.
		// All other escapes are preserved.

		size_t i = 0;
		std::wostringstream word1, word2;
		while (i < wline.length() && wline[i] != L'=') {
			if (wline[i] == L'@') {
				if (i + 1 < wline.length()) {
					if (wline[i + 1] == L'=') {
						word1.put(L'=');
					} else if (wline[i + 1] == L'n') {
						word1.put(L'\n');
					} else {
						word1.put(L'@');
						word1.put(wline[i + 1]);
					}
					i += 2;
				} else {
					// End of line, go to the next one.
					word1.put(L'\n');
					if (!is.good()) {
						break;
					}
					i = 0;
					std::getline(is, line);
					wline = utf8_to_wide(line);
				}
			} else {
				word1.put(wline[i]);
				i++;
			}
		}

		if (i == wline.length()) {
			errorstream << "Malformed translation line \"" << line << "\""
			            << " in text domain " << textdomain_narrow << std::endl;
			continue;
		}
		i++;

		while (i < wline.length()) {
			if (wline[i] == L'@') {
				if (i + 1 < wline.length()) {
					if (wline[i + 1] == L'=') {
						word2.put(L'=');
					} else if (wline[i + 1] == L'n') {
						word2.put(L'\n');
					} else {
						word2.put(L'@');
						word2.put(wline[i + 1]);
					}
					i += 2;
				} else {
					// End of line, go to the next one.
					word2.put(L'\n');
					if (!is.good()) {
						break;
					}
					i = 0;
					std::getline(is, line);
					wline = utf8_to_wide(line);
				}
			} else {
				word2.put(wline[i]);
				i++;
			}
		}

		addTranslation(textdomain, word1.str(), word2.str());
	}
}


std::wstring Translations::unescapeC(const std::wstring &str)
{
	// Process escape sequences in str as if it were a C string
	std::wstring result;
	size_t i = 0;
	while (i < str.length()) {
		if (str[i] != L'\\') {
			result.push_back(str[i]);
			i++;
			continue;
		}
		i++;
		if (i == str.length()) {
			errorstream << "Unfinished escape sequence at the end of \"" << wide_to_utf8(str) << "\"" << std::endl;
			break;
		}
		switch (str[i]) {
			// From https://en.wikipedia.org/wiki/Escape_sequences_in_C#Table_of_escape_sequences
			case L'a': result.push_back(L'\a'); break;
			case L'b': result.push_back(L'\b'); break;
			case L'e': result.push_back(L'\x1b'); break;
			case L'f': result.push_back(L'\f'); break;
			case L'n': result.push_back(L'\n'); break;
			case L'r': result.push_back(L'\r'); break;
			case L't': result.push_back(L'\t'); break;
			case L'v': result.push_back(L'\v'); break;
			case L'\\': result.push_back(L'\\'); break;
			case L'\'': result.push_back(L'\''); break;
			case L'"': result.push_back(L'"'); break;
			case L'?': result.push_back(L'?'); break;
			case L'0': case L'1': case L'2': case L'3': case L'4': case L'5': case L'6': case L'7': {
				size_t j = 0;
				wchar_t c = 0;
				for (; j < 3 && i+j < str.length() && L'0' <= str[i+j] && str[i+j] <= L'7'; j++) {
					c = c * 8 + (str[i+j] - L'0');
				}
				if (c <= 0xff) {
					result.push_back(c);
				}
				i += j;
				continue;
			}
			case L'x': {
				i++;
				if (i >= str.length()) {
					errorstream << "Unfinished escape sequence at the end of \"" << wide_to_utf8(str) << "\"" << std::endl;
				}
				char32_t c = 0;
				size_t j = 0;
				unsigned char v;
				for (; i+j < str.length() && hex_digit_decode((char)str[i+j], v); j++) {
					c = c * 16 + v;
				}
				if (j == 0) {
					errorstream << "Invalid escape sequence \\x, ignoring" << std::endl;
					continue;
				}
				// If character fits in 16 bits and is not part of surrogate pair, insert it.
				// Otherwise, silently drop it: this is valid since \x escape sequences with
				// values above 0xff are implementation-defined
				if ((c < 0xd800) || (0xe000 <= c && c <= 0xffff)) {
					result.push_back(c);
				}
				i += j;
				continue;
			}
			case L'u': {
				i++;
				if (i + 4 > str.length()) {
					errorstream << "Unfinished escape sequence at the end of \"" << wide_to_utf8(str) << "\"" << std::endl;
				}
				char16_t c = 0;
				bool ok = true;
				for (size_t j = 0; j < 4; j++) {
					unsigned char v;
					if (str[i+j] <= 0xff && hex_digit_decode((char)str[i+j], v)) {
						c = c * 16 + v;
					} else {
						errorstream << "Invalid unicode escape sequence \"\\u" << wide_to_utf8(str.substr(i, 4)) << "\", ignoring" << std::endl;
						ok = false;
						break;
					}
				}
				if (ok) {
					wide_add_codepoint(result, c);
				}
				i += 4;
				continue;
			}
			case L'U': {
				i++;
				if (i + 8 > str.length()) {
					errorstream << "Unfinished escape sequence at the end of \"" << wide_to_utf8(str) << "\"" << std::endl;
				}
				char32_t c = 0;
				bool ok = true;
				for (size_t j = 0; j < 8; j++) {
					unsigned char v;
					if (str[i+j] <= 0xff && hex_digit_decode((char)str[i+j], v)) {
						c = c * 16 + v;
					} else {
						errorstream << "Invalid unicode escape sequence \"\\U" << wide_to_utf8(str.substr(i, 8)) << "\", ignoring" << std::endl;
						ok = false;
						break;
					}
				}
				if (ok) {
					wide_add_codepoint(result, c);
				}
				i += 8;
				continue;
			}
			default: {
				errorstream << "Unknown escape sequence \"\\" << str[i] << "\", ignoring" << std::endl;
				break;
			}
		}
		i++;
	}
	return result;
}

void Translations::loadPoEntry(const std::wstring &basefilename, const GettextPluralForm::Ptr &plural_form, const std::map<std::wstring, std::wstring> &entry)
{
	// Process an entry from a PO file and add it to the translation table
	// Assumes that entry[L"msgid"] is always defined
	std::wstring textdomain;
	auto ctx = entry.find(L"msgctxt");
	if (ctx != entry.end()) {
		textdomain = ctx->second;
	} else {
		textdomain = basefilename;
	}
	std::wstring original = entry.at(L"msgid");

	auto plural = entry.find(L"msgid_plural");
	if (plural == entry.end()) {
		auto translated = entry.find(L"msgstr");
		if (translated == entry.end()) {
			errorstream << "Could not load translation: entry for msgid \"" << wide_to_utf8(original) << "\" does not contain a msgstr field" << std::endl;
			return;
		}
		addTranslation(textdomain, original, translated->second);
	} else {
		std::vector<std::wstring> translations;
		for (int i = 0; ; i++) {
			auto translated = entry.find(L"msgstr[" + std::to_wstring(i) + L"]");
			if (translated == entry.end())
				break;
			translations.push_back(translated->second);
		}
		addPluralTranslation(textdomain, plural_form, original, translations);
		addPluralTranslation(textdomain, plural_form, plural->second, translations);
	}
}

bool Translations::inEscape(const std::wstring &line, size_t pos)
{
	if (pos == std::wstring::npos || pos == 0)
		return false;
	pos--;
	size_t count = 0;
	for (; line[pos] == L'\\'; pos--) {
		count++;
		if (pos == 0)
			break;
	}
	return count % 2 == 1;
}

std::optional<std::pair<std::wstring, std::wstring>> Translations::parsePoLine(const std::string &line)
{
	if (line.empty())
		return std::nullopt;
	if (line[0] == '#')
		return std::pair(L"#", utf8_to_wide(line.substr(1)));

	std::wstring wline = utf8_to_wide(line);
	// Defend against some possibly malformed utf8 string, which
	// is empty after converting to wide string
	if (wline.empty())
		return std::nullopt;

	std::size_t pos = wline.find(L'"');
	std::wstring s;
	if (pos == std::wstring::npos) {
		errorstream << "Unable to parse po file line: " << line << std::endl;
		return std::nullopt;
	}
	auto prefix = trim(wline.substr(0, pos));
	auto begin = pos;
	while (pos < wline.size()) {
		begin = wline.find(L'"', pos);
		if (begin == std::wstring::npos) {
			if (trim(wline.substr(pos)).empty()) {
				break;
			} else {
				errorstream << "Excessive content at the end of po file line: " << line << std::endl;
				return std::nullopt;
			}
		}
		if (!trim(wline.substr(pos, begin-pos)).empty()) {
			errorstream << "Excessive content within string concatenation in po file line: " << line << std::endl;
			return std::nullopt;
		}
		auto end = wline.find(L'"', begin+1);
		while (inEscape(wline, end)) {
			end = wline.find(L'"', end+1);
		}
		if (end == std::wstring::npos) {
			errorstream << "String extends beyond po file line: " << line << std::endl;
			return std::nullopt;
		}
		s.append(unescapeC(wline.substr(begin+1, end-begin-1)));
		pos = end+1;
	}
	return std::pair(prefix, s);
}

void Translations::loadPoTranslation(const std::string &basefilename, const std::string &data)
{
	std::istringstream is(data);
	std::string line;
	std::map<std::wstring, std::wstring> last_entry;
	std::wstring last_key;
	std::wstring wbasefilename = utf8_to_wide(basefilename);
	GettextPluralForm::Ptr plural;
	bool skip = false;
	bool skip_last = false;

	while (is.good()) {
		std::getline(is, line);
		// Trim last character if file was using a \r\n line ending
		if (line.length () > 0 && line[line.length() - 1] == '\r')
			line.resize(line.length() - 1);

		auto parsed = parsePoLine(line);
		if (!parsed)
			continue;
		auto prefix = parsed->first;
		auto s = parsed->second;

		if (prefix == L"#") {
			if (s[0] == L',') {
				// Skip fuzzy entries
				if ((s + L' ').find(L" fuzzy ") != line.npos) {
					if (last_entry.empty())
						skip_last = true;
					else
						skip = true;
				}
			}
			continue;
		}

		if (prefix.empty()) {
			// Continuation of previous line
			if (last_key == L"") {
				errorstream << "Unable to parse po file: continuation of non-existant previous line" << std::endl;
				continue;
			}

			last_entry[last_key].append(s);
			continue;
		}

		if (prefix == L"msgctxt" || (prefix == L"msgid" && last_entry.find(L"msgid") != last_entry.end())) {
			if (last_entry.find(L"msgid") != last_entry.end()) {
				if (!skip_last) {
					if (last_entry[L"msgid"].empty()) {
						if (last_entry.find(L"msgstr") == last_entry.end()) {
							errorstream << "Header entry has no \"msgstr\" field" << std::endl;
						} else if (plural) {
							errorstream << "Attempt to override existing po header entry" << std::endl;
						} else {
							for (auto &line: str_split(last_entry[L"msgstr"], L'\n')) {
								if (str_starts_with(line, L"Plural-Forms:")) {
									plural = GettextPluralForm::parseHeaderLine(line);
									if (!(plural && *plural)) {
										errorstream << "Invalid Plural-Forms line: " << wide_to_utf8(line) << std::endl;
									}
								}
							}
						}
					} else {
						loadPoEntry(wbasefilename, plural, last_entry);
					}
				}
				last_entry.clear();
				skip_last = skip;
			} else if (!last_entry.empty()) {
				errorstream << "Unable to parse po file: previous entry has no \"msgid\" field but is not empty" << std::endl;
				last_entry.clear();
				skip_last = skip;
			}
		} else {
			// prevent malpositioned fuzzy flag from influencing the following entry
			skip = false;
		}
		if (last_entry.find(prefix) != last_entry.end()) {
			errorstream << "Unable to parse po file: Key \"" << wide_to_utf8(prefix) << "\" was already present in previous entry" << std::endl;
			continue;
		}
		last_key = prefix;
		last_entry[prefix] = s;
	}

	if (last_entry.find(L"msgid") != last_entry.end()) {
		if (!skip_last && !last_entry[L"msgid"].empty())
			loadPoEntry(wbasefilename, plural, last_entry);
	} else if (!last_entry.empty()) {
		errorstream << "Unable to parse po file: Last entry has no \"msgid\" field" << std::endl;
	}
}

void Translations::loadMoEntry(const std::wstring &basefilename, const GettextPluralForm::Ptr &plural_form, const std::string &original, const std::string &translated)
{
	std::wstring textdomain = L"";
	size_t found;
	std::string noriginal = original;
	found = original.find('\x04'); // EOT character
	if (found != std::string::npos) {
		textdomain = utf8_to_wide(original.substr(0, found));
		noriginal = original.substr(found + 1);
	} else {
		textdomain = basefilename;
	}

	found = noriginal.find('\0');
	if (found != std::string::npos) {
		std::vector<std::wstring> translations = str_split(utf8_to_wide(translated), L'\0');
		addPluralTranslation(textdomain, plural_form, utf8_to_wide(noriginal.substr(0, found)), translations);
		addPluralTranslation(textdomain, plural_form, utf8_to_wide(noriginal.substr(found + 1)), translations);
	} else {
		addTranslation(textdomain, utf8_to_wide(noriginal), utf8_to_wide(translated));
	}
}

inline u32 readVarEndian(bool is_be, std::string_view data, size_t pos = 0)
{
	if (pos + 4 > data.size())
		return 0;
	if (is_be) {
		return
			((u32)(unsigned char)data[pos+0] << 24) | ((u32)(unsigned char)data[pos+1] << 16) |
			((u32)(unsigned char)data[pos+2] <<  8) | ((u32)(unsigned char)data[pos+3] <<  0);
	} else {
		return
			((u32)(unsigned char)data[pos+0] <<  0) | ((u32)(unsigned char)data[pos+1] <<  8) |
			((u32)(unsigned char)data[pos+2] << 16) | ((u32)(unsigned char)data[pos+3] << 24);
	}
}

void Translations::loadMoTranslation(const std::string &basefilename, const std::string &data)
{
	size_t length = data.length();
	std::wstring wbasefilename = utf8_to_wide(basefilename);
	GettextPluralForm::Ptr plural_form;

	if (length < 20) {
		errorstream << "Ignoring too short mo file" << std::endl;
		return;
	}

	u32 magic = readVarEndian(false, data);
	bool is_be;
	if (magic == 0x950412de) {
		is_be = false;
	} else if (magic == 0xde120495) {
		is_be = true;
	} else {
		errorstream << "Bad magic number for mo file: 0x" << hex_encode(data.substr(0, 4)) << std::endl;
		return;
	}

	u32 revision = readVarEndian(is_be, data, 4);
	if (revision != 0) {
		errorstream << "Unknown revision " << revision << " for mo file" << std::endl;
		return;
	}

	u32 nstring = readVarEndian(is_be, data, 8);
	u32 original_offset = readVarEndian(is_be, data, 12);
	u32 translated_offset = readVarEndian(is_be, data, 16);

	if (length < original_offset + 8 * (u64)nstring || length < translated_offset + 8 * (u64)nstring) {
		errorstream << "Ignoring truncated mo file" << std::endl;
		return;
	}

	for (u32 i = 0; i < nstring; i++) {
		u32 original_len = readVarEndian(is_be, data, original_offset + 8 * i);
		u32 original_off = readVarEndian(is_be, data, original_offset + 8 * i + 4);
		u32 translated_len = readVarEndian(is_be, data, translated_offset + 8 * i);
		u32 translated_off = readVarEndian(is_be, data, translated_offset + 8 * i + 4);

		if (length < original_off + (u64)original_len || length < translated_off + (u64)translated_len) {
			errorstream << "Ignoring translation out of mo file" << std::endl;
			continue;
		}

		if (data[original_off+original_len] != '\0' || data[translated_off+translated_len] != '\0') {
			errorstream << "String in mo entry does not have a trailing NUL" << std::endl;
			continue;
		}

		auto original = data.substr(original_off, original_len);
		auto translated = data.substr(translated_off, translated_len);

		if (original.empty()) {
			if (plural_form) {
				errorstream << "Attempt to override existing mo header entry" << std::endl;
			} else {
				for (auto &line: str_split(translated, '\n')) {
					if (str_starts_with(line, "Plural-Forms:")) {
						plural_form = GettextPluralForm::parseHeaderLine(utf8_to_wide(line));
						if (!(plural_form && *plural_form)) {
							errorstream << "Invalid Plural-Forms line: " << line << std::endl;
						}
					}
				}
			}
		} else {
			loadMoEntry(wbasefilename, plural_form, original, translated);
		}
	}

	return;
}

void Translations::loadTranslation(const std::string &filename, const std::string &data)
{
	const char *trExtension[] = { ".tr", NULL };
	const char *poExtension[] = { ".po", NULL };
	const char *moExtension[] = { ".mo", NULL };
	if (!removeStringEnd(filename, trExtension).empty()) {
		loadTrTranslation(data);
	} else if (!removeStringEnd(filename, poExtension).empty()) {
		std::string basefilename = str_split(filename, '.')[0];
		loadPoTranslation(basefilename, data);
	} else if (!removeStringEnd(filename, moExtension).empty()) {
		std::string basefilename = str_split(filename, '.')[0];
		loadMoTranslation(basefilename, data);
	} else {
		errorstream << "loadTranslation called with invalid filename: \"" << filename << "\"" << std::endl;
	}
}
