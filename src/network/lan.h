/*
Luanti
Copyright (C) 2024 proller <proler@gmail.com> and contributors.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "json/json.h"
#include <map>
#include <shared_mutex>
#include <string>
#include <atomic>
#include "threading/thread.h"


class lan_adv : public Thread
{
public:
	void *run();

	lan_adv();
	void ask();
	void send_string(const std::string &str);

	void serve(unsigned short port);

	std::map<std::string, Json::Value> collected;
	std::shared_mutex mutex;

	std::atomic_bool fresh;
	std::atomic_int clients_num;

private:
	unsigned short server_port = 0;
	int socket;
};
