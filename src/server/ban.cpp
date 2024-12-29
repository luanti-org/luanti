// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include "ban.h"
#include <fstream>
#include "threading/mutex_auto_lock.h"
#include <sstream>
#include "util/strfnd.h"
#include "util/string.h"
#include "log.h"
#include "filesys.h"
#include "exceptions.h"

BanManager::BanManager(const std::string &banfilepath):
		m_banfilepath(banfilepath)
{
	try {
		load();
	} catch(SerializationError &e) {
		infostream << "BanManager: creating "
				<< m_banfilepath << std::endl;
	}
}

BanManager::~BanManager()
{
	save();
}

void BanManager::load()
{
	MutexAutoLock lock(m_mutex);
	infostream<<"BanManager: loading from "<<m_banfilepath<<std::endl;
	auto is = open_ifstream(m_banfilepath.c_str(), false);
	if (!is.good()) {
		throw SerializationError("BanManager::load(): Couldn't open file");
	}

	while (!is.eof() && is.good()) {
		std::string line;
		std::getline(is, line, '\n');
		Strfnd f(line);
		std::string ip = trim(f.next("|"));
		std::string name = trim(f.next("|"));
		if(!ip.empty()) {
			m_ips[ip] = name;
		}
	}
	m_modified = false;
}

void BanManager::save()
{
	MutexAutoLock lock(m_mutex);
	infostream << "BanManager: saving to " << m_banfilepath << std::endl;
	std::ostringstream ss(std::ios_base::binary);

	for (const auto &ip : m_ips)
		ss << ip.first << "|" << ip.second << "\n";

	if (!fs::safeWriteToFile(m_banfilepath, ss.str())) {
		infostream << "BanManager: failed saving to " << m_banfilepath << std::endl;
		throw SerializationError("BanManager::save(): Couldn't write file");
	}

	m_modified = false;
}

bool BanManager::isIpBanned(const std::string &ip) const
{
	MutexAutoLock lock(m_mutex);
	return m_ips.find(ip) != m_ips.end();
}

std::string BanManager::getBanDescription(const std::string &ip_or_name) const
{
	MutexAutoLock lock(m_mutex);
	std::string s;
	for (const auto &ip : m_ips) {
		if (ip.first  == ip_or_name || ip.second == ip_or_name
				|| ip_or_name.empty()) {
			s += ip.first + "|" + ip.second + ", ";
		}
	}
	s = s.substr(0, s.size() - 2);
	return s;
}

std::string BanManager::getBanName(const std::string &ip) const
{
	MutexAutoLock lock(m_mutex);
	StringMap::const_iterator it = m_ips.find(ip);
	if (it == m_ips.end())
		return "";
	return it->second;
}

void BanManager::add(const std::string &ip, const std::string &name)
{
	MutexAutoLock lock(m_mutex);
	m_ips[ip] = name;
	m_modified = true;
}

void BanManager::remove(const std::string &ip_or_name)
{
	MutexAutoLock lock(m_mutex);
	for (auto it = m_ips.begin(); it != m_ips.end();) {
		if ((it->first == ip_or_name) || (it->second == ip_or_name)) {
			it = m_ips.erase(it);
			m_modified = true;
		} else {
			++it;
		}
	}
}


bool BanManager::isModified() const
{
	MutexAutoLock lock(m_mutex);
	return m_modified;
}
