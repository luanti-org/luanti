// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mod_vfs.h"
#include "filesys.h"
#include "log.h"
#include <algorithm>

void ModVFS::scanModSubfolder(const std::string &virt_prefix,
		const std::string &real_path, std::string subpath)
{
	std::string full_path = real_path + DIR_DELIM + subpath;
	std::vector<fs::DirListNode> mod = fs::GetDirListing(full_path);
	for (const fs::DirListNode &j : mod) {
		if (j.name[0] == '.')
			continue;

		if (j.dir) {
			scanModSubfolder(virt_prefix, real_path, subpath + j.name + DIR_DELIM);
			continue;
		}
		std::replace(subpath.begin(), subpath.end(), DIR_DELIM_CHAR, '/');

		std::string real_subpath = full_path + j.name;
		std::string virt_subpath = virt_prefix + subpath + j.name;
		loadFile(virt_subpath, real_subpath);
	}
}

void ModVFS::loadFile(const std::string &virt_path, const std::string &real_path)
{

	infostream << "ModVFS::scanModSubfolder(): Loading \"" << real_path
			<< "\" as \"" << virt_path << "\"." << std::endl;

	std::string contents;
	if (!fs::ReadFile(real_path, contents)) {
		errorstream << "ModVFS::scanModSubfolder(): Can't read file \""
				<< real_path << "\"." << std::endl;
		return;
	}

	files.emplace(virt_path, contents);
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

	auto it = files.find(filename);
	if (it == files.end())
		return nullptr;
	return &it->second;
}
