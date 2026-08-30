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

	/** Adds all SSCSM client-builtin files (as defined by builtin's CMakeLists).
	 *
	 * Checks for integrity: Files must be the same as when the binary was built.
	 * There's multiple purposes for this:
	 * * Notify users that their installation is broken.
	 * * Prevent cheating on remote server without rebuilding (requires force_integrity).
	 *
	 * @param builtin_path Base path of builtin.
	 * @param force_integrity If true, throws an exception on integrity check failure,
	 * otherwise just prints a warning.
	 */
	void scanSSCSMClientBuiltin(const std::string &builtin_path, bool force_integrity);

	// virtual path -> file content
	std::unordered_map<std::string, std::string> m_vfs;
};
