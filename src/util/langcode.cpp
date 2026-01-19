// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <unordered_map>
#include "util/string.h"

std::vector<std::wstring> parse_language_list(const std::wstring &lang)
{
	std::unordered_map<std::wstring, std::wstring> added_by;
	std::vector<std::vector<std::wstring>> expanded;

	for (const auto &name: str_split(lang, L':')) {
		auto pos = name.find(L'.'); // strip encoding information
		const auto realname = pos == name.npos ? name : name.substr(0, pos);
		if (realname.empty())
			continue;

		std::vector<std::wstring> basenames = {};
		auto base = realname;
		do {
			if (added_by[base] == base)
				break;
			added_by[base] = realname;
			basenames.push_back(base);

			pos = base.find_last_of(L"_@");
			base = base.substr(0, pos);
		} while (pos != base.npos);
		if (!basenames.empty())
			expanded.push_back(std::move(basenames));
	}

	std::vector<std::wstring> langlist;
	for (auto &basenames: expanded)
	{
		auto first = basenames.front();
		for (auto &&name: basenames)
			if (added_by[name] == first)
				langlist.push_back(std::move(name));
	}
	return langlist;
}

std::wstring expand_language_list(const std::wstring &lang)
{
	return str_join(parse_language_list(lang), L":");
}
