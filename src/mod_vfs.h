// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>
#include <unordered_map>

struct ModVFS
{
	/// Scan the given real path recursively, mapping relative paths
	/// to virtual paths by prepending the virtual prefix.
	/// @note hidden entries (starting with `.`) are ignored
	void scanModSubfolder(const std::string &virt_prefix, const std::string &real_path,
			std::string subpath);

	/// Load the given file into the VFS under the given virtual path
	void loadFile(const std::string &virt_path, const std::string &real_path);

	inline void scanModIntoMemory(const std::string &mod_name, const std::string &mod_path)
	{
		scanModSubfolder(mod_name, mod_path, "");
	}

	const std::string *getModFile(std::string filename);

	std::unordered_map<std::string, std::string> files;
};
