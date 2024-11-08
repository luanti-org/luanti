// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013 Jonathan Neuschäfer <j.neuschaefer@gmx.net>

#include "filecache.h"

#include "log.h"
#include "filesys.h"
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>

void FileCache::createDir()
{
	if (!fs::CreateAllDirs(m_dir)) {
		errorstream << "Could not create cache directory: "
			<< m_dir << std::endl;
	}
}

bool FileCache::loadByPath(const std::string &path, std::ostream &os)
{
	auto fis = open_ifstream(path.c_str(), false);
	if (!fis.good())
		return false;

	bool bad = false;
	for(;;){
		char buf[4096];
		fis.read(buf, sizeof(buf));
		std::streamsize len = fis.gcount();
		os.write(buf, len);
		if(fis.eof())
			break;
		if(!fis.good()){
			bad = true;
			break;
		}
	}
	if(bad){
		errorstream<<"FileCache: Failed to read file from cache: \""
				<<path<<"\""<<std::endl;
	}

	return !bad;
}

bool FileCache::updateByPath(const std::string &path, std::string_view data)
{
	createDir();

	auto file = open_ofstream(path.c_str(), true);
	if (!file.good())
		return false;

	file << data;
	file.close();

	return !file.fail();
}

bool FileCache::update(const std::string &name, std::string_view data)
{
	std::string path = m_dir + DIR_DELIM + name;
	return updateByPath(path, data);
}

bool FileCache::load(const std::string &name, std::ostream &os)
{
	std::string path = m_dir + DIR_DELIM + name;
	return loadByPath(path, os);
}

bool FileCache::exists(const std::string &name)
{
	std::string path = m_dir + DIR_DELIM + name;
	return fs::PathExists(path);
}

bool FileCache::updateCopyFile(const std::string &name, const std::string &src_path)
{
	std::string path = m_dir + DIR_DELIM + name;

	createDir();
	return fs::CopyFileContents(src_path, path);
}
