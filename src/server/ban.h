// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "util/string.h"
#include "threading/thread.h"
#include "exceptions.h"
#include <map>
#include <string>
#include <mutex>

class BanManager
{
public:
	BanManager(const std::string &banfilepath);
	~BanManager();
	void load();
	void save();
	bool isIpBanned(const std::string &ip);
	// Supplying ip_or_name = "" lists all bans.
	std::string getBanDescription(const std::string &ip_or_name);
	std::string getBanName(const std::string &ip);
	void add(const std::string &ip, const std::string &name);
	void remove(const std::string &ip_or_name);
	bool isModified();

private:
	std::mutex m_mutex;
	std::string m_banfilepath = "";
	StringMap m_ips;
	bool m_modified = false;
};
