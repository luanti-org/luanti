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

#include "network/lan.h"
#include <cstdint>
#include "convert_json.h"
#include "socket.h"
#include "log.h"
#include "settings.h"
#include "version.h"
#include "networkprotocol.h"
#include "server/serverlist.h"
#include "debug.h"
#include "json/json.h"
#include <mutex>
#include <shared_mutex>
#include "porting.h"
#include "threading/thread.h"
#include "network/address.h"

//copypaste from ../socket.cpp
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Without this some of the network functions are not found on mingw
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define LAST_SOCKET_ERR() WSAGetLastError()
typedef SOCKET socket_t;
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#ifndef __ANDROID__
#include <ifaddrs.h>
#define HAVE_IFADDRS 1
#endif

#define LAST_SOCKET_ERR() (errno)
typedef int socket_t;
#endif

const char* adv_multicast_addr = "224.1.1.1";
const static unsigned short int adv_port = 29998;
const std::string proto = "lanti";
static std::string ask_str;

bool use_ipv6 = true;

lan_adv::lan_adv() : Thread("lan_adv")
{
}

void lan_adv::ask()
{
	if (!isRunning()) start();

	if (ask_str.empty()) {
		Json::Value j;
		j["cmd"] = "ask";
		j["proto"] = proto;
		ask_str = fastWriteJson(j);
	}

	send_string(ask_str);
}

void lan_adv::send_string(const std::string &str)
{
	if (g_settings->getBool("ipv6_server")) {
		std::vector<uint32_t> scopes;
		// todo: windows and android

#if HAVE_IFADDRS
		struct ifaddrs *ifaddr = nullptr, *ifa = nullptr;
		if (getifaddrs(&ifaddr) < 0) {
		} else {
			for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
				if (!ifa->ifa_addr)
					continue;
				if (ifa->ifa_addr->sa_family != AF_INET6)
					continue;

				auto sa = *((struct sockaddr_in6 *)ifa->ifa_addr);
			if (sa.sin6_scope_id)
					scopes.push_back(sa.sin6_scope_id);

				/*errorstream<<"in=" << ifa->ifa_name << " a="<<Address(*((struct sockaddr_in6*)ifa->ifa_addr)).serializeString()<<" ba=" << ifa->ifa_broadaddr <<" sc=" << sa.sin6_scope_id <<" fl=" <<  ifa->ifa_flags
				//<< " bn=" << Address(*((struct sockaddr_in6*)ifa->ifa_broadaddr)).serializeString()
				<<"\n"; */
			}
		}
		freeifaddrs(ifaddr);
#endif

		if (scopes.empty())
			scopes.push_back(0);

		struct addrinfo hints
		{
		};

		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
		struct addrinfo *result = nullptr;
		if (!getaddrinfo("ff02::1", nullptr, &hints, &result)) {
			for (auto info = result; info; info = info->ai_next) {
				try {
					sockaddr_in6 addr = *((struct sockaddr_in6 *)info->ai_addr);
					addr.sin6_port = htons(adv_port);
					UDPSocket socket_send(true);
					int set_option_on = 1;
					//setsockopt(socket_send.GetHandle(), SOL_SOCKET, IPV6_MULTICAST_HOPS,
					//		(const char *)&set_option_on, sizeof(set_option_on));
					auto use_scopes = scopes;
					if (addr.sin6_scope_id) {
						use_scopes.clear();
						use_scopes.push_back(addr.sin6_scope_id);
					}
					for (auto &scope : use_scopes) {
						addr.sin6_scope_id = scope;
						socket_send.Send(Address(addr), str.c_str(), str.size());
					}
				} catch (const std::exception &e) {
					verbosestream << "udp multicast send over ipv6 fail [" << e.what() << "]\n" << "Trying ipv4.\n";
					try {
						sockaddr_in addr = {};
						addr.sin_family = AF_INET;
						addr.sin_port = htons(adv_port);
						UDPSocket socket_send(false);

						inet_pton(AF_INET, adv_multicast_addr, &(addr.sin_addr));

						struct ip_mreq mreq;

						mreq.imr_multiaddr.s_addr = inet_addr(adv_multicast_addr);
						mreq.imr_interface.s_addr = htonl(INADDR_ANY);

						setsockopt(socket_send.GetHandle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
								(const char *)&mreq, sizeof(mreq));

						//int set_option_on = 2;
						//setsockopt(socket_send.GetHandle(), SOL_SOCKET, IP_MULTICAST_TTL,
						//		(const char *)&set_option_on, sizeof(set_option_on));

						socket_send.Send(Address(addr), str.c_str(), str.size());
					} catch (const std::exception &e) {
						verbosestream << "udp mulitcast send over ipv4 fail too. " << e.what() << "\n";
					}
				}
			}
			freeaddrinfo(result);
		}
	} else {
		try {
			sockaddr_in addr = {};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(adv_port);
			UDPSocket socket_send(false);

			inet_pton(AF_INET, adv_multicast_addr, &(addr.sin_addr));

			struct ip_mreq mreq;

			mreq.imr_multiaddr.s_addr = inet_addr(adv_multicast_addr);
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);

			setsockopt(socket_send.GetHandle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
					(const char *)&mreq, sizeof(mreq));

			//int set_option_on = 2;
			//setsockopt(socket_send.GetHandle(), SOL_SOCKET, IP_MULTICAST_TTL,
			//		(const char *)&set_option_on, sizeof(set_option_on));

			socket_send.Send(Address(addr), str.c_str(), str.size());
		} catch (const std::exception &e) {
			verbosestream << "udp mulitcast send over ipv4 fail " << e.what() << "\n";
		}
	}
}

void lan_adv::serve(unsigned short port)
{
	server_port = port;
	stop();
	start();
}

void *lan_adv::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER;

	setName("lan_adv " + (server_port ? std::string("server") : std::string("client")));
	UDPSocket socket_recv(g_settings->getBool("ipv6_server"));

	int set_option_off = 0, set_option_on = 1;
	setsockopt(socket_recv.GetHandle(), SOL_SOCKET, SO_REUSEADDR,
			(const char *)&set_option_on, sizeof(set_option_on));
#ifdef SO_REUSEPORT
	setsockopt(socket_recv.GetHandle(), SOL_SOCKET, SO_REUSEPORT,
			(const char *)&set_option_on, sizeof(set_option_on));
#endif
	struct ip_mreq mreq;

	mreq.imr_multiaddr.s_addr = inet_addr(adv_multicast_addr);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	setsockopt(socket_recv.GetHandle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
			(const char *)&mreq, sizeof(mreq));
	setsockopt(socket_recv.GetHandle(), IPPROTO_IPV6, IPV6_V6ONLY,
			(const char *)&set_option_off, sizeof(set_option_off));
	socket_recv.setTimeoutMs(200);

	if (g_settings->getBool("ipv6_server")) {
		try {
			socket_recv.Bind(Address(in6addr_any, adv_port));
		} catch (const std::exception &e) {
			warningstream << m_name << ": cant bind ipv6 address [" << e.what()
						<< "], trying ipv4. " << std::endl;
			try {
				socket_recv.Bind(Address((u32)INADDR_ANY, adv_port));
			} catch (const std::exception &e) {
				warningstream << m_name << ": cant bind ipv4 too [" << e.what() << "]"
							<< std::endl;
				return nullptr;
			}
		}
	} else {
		try {
			socket_recv.Bind(Address((u32)INADDR_ANY, adv_port));
		} catch (const std::exception &e) {
			warningstream << m_name << ": cant bind ipv4 [" << e.what() << "]"
						<< std::endl;
			return nullptr;
		}
	}
	std::unordered_map<std::string, uint64_t> limiter;
	const unsigned int packet_maxsize = 16384;
	char buffer[packet_maxsize];
	Json::Reader reader;
	std::string answer_str;
	Json::Value server;
	if (server_port) {
		server["name"] = g_settings->get("server_name");
		server["description"] = g_settings->get("server_description");
		server["version"] = g_version_string;
		bool strict_checking = g_settings->getBool("strict_protocol_version_checking");
		server["proto_min"] =
				strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MIN;
		server["proto_max"] = LATEST_PROTOCOL_VERSION;
		server["url"] = g_settings->get("server_url");
		server["creative"] = g_settings->getBool("creative_mode");
		server["damage"] = g_settings->getBool("enable_damage");
		server["password"] = g_settings->getBool("disallow_empty_password");
		server["pvp"] = g_settings->getBool("enable_pvp");
		server["port"] = server_port;
		server["clients"] = clients_num.load();
		server["clients_max"] = g_settings->getU16("max_users");
		server["proto"] = proto;

		send_string(fastWriteJson(server));
	}

	while (isRunning() && !stopRequested()) {
		BEGIN_DEBUG_EXCEPTION_HANDLER;
		Address addr;
		int rlen = socket_recv.Receive(addr, buffer, packet_maxsize);
		if (rlen <= 0)
			continue;
		Json::Value p;
		if (!reader.parse(std::string(buffer, rlen), p)) {
			//errorstream << "cant parse "<< s << "\n";
			continue;
		}
		auto addr_str = addr.serializeString();
		auto now = porting::getTimeMs();
		//errorstream << " a=" << addr.serializeString() << " : " << addr.getPort() << " l=" << rlen << " b=" << p << " ;  server=" << server_port << "\n";

		if (server_port) {
			if (p["cmd"] == "ask" && limiter[addr_str] < now) {
				(clients_num.load() ? infostream : actionstream)
						<< "lan: want play " << addr_str << " " << p["proto"] << std::endl;

				server["clients"] = clients_num.load();
				answer_str = fastWriteJson(server);
				limiter[addr_str] = now + 3000;
				send_string(answer_str);
				//UDPSocket socket_send(true);
				//addr.setPort(adv_port);
				//socket_send.Send(addr, answer_str.c_str(), answer_str.size());
			}
		} else {
			if (p["cmd"] == "ask") {
				actionstream << "lan: want play " << addr_str << " " << p["proto"] << std::endl;
			}
			if (p["port"].isInt()) {
				p["address"] = addr_str;
				auto key = addr_str + ":" + p["port"].asString();
				std::unique_lock lock(mutex);
				if (p["cmd"].asString() == "shutdown") {
					//infostream << "server shutdown " << key << "\n";
					collected.erase(key);
					fresh = true;
				} else if (p["proto"] == proto) {
					if (!collected.count(key))
						actionstream << "lan server start " << key << "\n";
					collected.insert_or_assign(key, p);
					fresh = true;
				}
			}

			//errorstream<<" current list: ";for (auto & i : collected) {errorstream<< i.first <<" ; ";}errorstream<<std::endl;
		}
		END_DEBUG_EXCEPTION_HANDLER;
	}

	if (server_port) {
		Json::Value answer_json;
		answer_json["port"] = server_port;
		answer_json["cmd"] = "shutdown";
		send_string(fastWriteJson(answer_json));
	}

	END_DEBUG_EXCEPTION_HANDLER;

	return nullptr;
}
