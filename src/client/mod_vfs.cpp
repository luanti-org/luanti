// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mod_vfs.h"

#include "builtin_files.h"
#include "exceptions.h"
#include "filesys.h"
#include "log.h"
#include "util/hashing.h"
#include "util/hex.h"
#include <algorithm>
#include <sstream>

void ModVFS::scanModSubfolder(const std::string &mod_name, const std::string &mod_path,
		std::string mod_subpath)
{
	std::string full_path = mod_path + DIR_DELIM + mod_subpath;
	std::vector<fs::DirListNode> mod = fs::GetDirListing(full_path);
	for (const fs::DirListNode &j : mod) {
		if (j.name[0] == '.')
			continue;

		if (j.dir) {
			scanModSubfolder(mod_name, mod_path, mod_subpath + j.name + DIR_DELIM);
			continue;
		}
		std::replace(mod_subpath.begin(), mod_subpath.end(), DIR_DELIM_CHAR, '/');

		std::string real_path = full_path + j.name;
		std::string vfs_path = mod_name + ":" + mod_subpath + j.name;
		infostream << "ModVFS::scanModSubfolder(): Loading \"" << real_path
				<< "\" as \"" << vfs_path << "\"." << std::endl;

		std::string contents;
		if (!fs::ReadFile(real_path, contents)) {
			errorstream << "ModVFS::scanModSubfolder(): Can't read file \""
					<< real_path << "\"." << std::endl;
			continue;
		}

		m_vfs.emplace(std::move(vfs_path), std::move(contents));
	}
}

const std::string *ModVFS::getModFile(std::string filename)
{
	// strip dir delimiter from beginning of path
	auto pos = filename.find_first_of(':');
	if (pos == std::string::npos)
		return nullptr;
	++pos;
	auto pos2 = filename.find_first_not_of('/', pos);
	if (pos2 > pos)
		filename.erase(pos, pos2 - pos);

	auto it = m_vfs.find(filename);
	if (it == m_vfs.end())
		return nullptr;
	return &it->second;
}

void ModVFS::scanSSCSMClientBuiltin(const std::string &builtin_path)
{
	for (auto rel_path : g_builtin_sscsm_client_files) {
		std::string rel_path_os{rel_path};
		std::replace(rel_path_os.begin(), rel_path_os.end(), '/', DIR_DELIM_CHAR);

		std::string real_path = builtin_path + DIR_DELIM + rel_path_os;
		std::string vfs_path = std::string("*client_builtin*:") + std::string(rel_path);
		infostream << "Client::Client(): Loading sscsm client-builtin file \""
				<< real_path << "\" as \"" << vfs_path << "\"." << std::endl;

		std::string contents;
		if (!fs::ReadFile(real_path, contents)) {
			errorstream << "Client::Client(): Can't read sscsm client-builtin file \""
					<< real_path << "\"." << std::endl;
			continue;
		}

		// Check sha256 digest of file (to prevent cheating without rebuilding)
		{
			auto digest = hex_encode(hashing::sha256(contents));
			auto it = g_builtin_file_sha256_map.find(rel_path);
			if (it == g_builtin_file_sha256_map.end()) {
				std::ostringstream err;
				err << "No SHA256 known for SSCSM client-builtin file \""
						<< rel_path << "\"";
				throw BaseException(err.str());
			}
			if (it->second != digest) {
				std::ostringstream err;
				err << "SHA256 of SSCSM client-builtin file \"" << rel_path
						<< "\" does not match."
						<< "\nExpected: " << it->second
						<< "\nFound:    " << digest;
				throw BaseException(err.str());
			}
		}

		m_vfs.emplace(std::move(vfs_path), std::move(contents));
	}
}

void ModVFS::merge(ModVFS &&other)
{
	for (auto &&p : other.m_vfs) {
		m_vfs.insert(std::move(p));
	}
	other.m_vfs.clear();
}
