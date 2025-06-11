// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>
#include <unordered_map>

struct ModVFS
{
	void scanModSubfolder(const std::string &mod_name, const std::string &mod_path,
			std::string mod_subpath);

	inline void scanModIntoMemory(const std::string &mod_name, const std::string &mod_path)
	{
		scanModSubfolder(mod_name, mod_path, "");
	}

	const std::string *getModFile(std::string filename);

	std::unordered_map<std::string, std::string> m_vfs;
};
