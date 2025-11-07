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
#include "networkservice_namecol.h"
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
#include <mdns.h>

#ifndef __ANDROID__
#include <ifaddrs.h>
#define HAVE_IFADDRS 1
#endif

#define LAST_SOCKET_ERR() (errno)
typedef int socket_t;
#endif

const char* adv_multicast_addr = "224.1.1.1";
const static unsigned short int adv_port = 5353;
const std::string service_name = "Luanti";
const mdns_record_type_t record_type = MDNS_RECORDTYPE_PTR;
static std::string ask_str;

// Note to reviewers:
// I will be removing ipv6 support. This is to 
// simplify the code as it is really hard to
// work on this without having to juggle two
// ip versions. I can readd ipv6 later if it's
// required.
bool use_ipv6 = true;

static int mdns_parse_callback(
    int sock, const struct sockaddr* from, size_t addrlen,
    mdns_entry_type_t entry, uint16_t query_id, uint16_t rtype,
    uint16_t rclass, uint32_t ttl, const void* data, size_t size,
    size_t name_offset, size_t name_length, size_t record_offset,
    size_t record_length, void* user_data)
{
    char namebuf[256];
    switch (rtype) {
        case MDNS_RECORDTYPE_PTR: {
            mdns_string_t s = mdns_record_parse_ptr(data, size, record_offset, record_length, namebuf, sizeof(namebuf));
            // s.str is the instance name (e.g. "MyServer._luanti._udp.local")
            // Save or print this name
			actionstream << "Found service instance: " << std::string(s.str, s.length) << std::endl;
            break;
        }
        case MDNS_RECORDTYPE_SRV: {
            mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length, namebuf, sizeof(namebuf));
            // srv.name.str is the target host, srv.port is the port
			actionstream << "Service target: " << std::string(srv.name.str, srv.name.length)
						 << " Port: " << srv.port << std::endl;
            break;
        }
        case MDNS_RECORDTYPE_TXT: {
			mdns_record_txt_t txt[8];
			size_t count = mdns_record_parse_txt(
				data, size, record_offset, record_length, txt, 8
			);
			// Not sure why this should be a loop, we only send one key
			// value pair.
			for (size_t i = 0; i < count; ++i) {
				// WHAT IS GOING ON HERE???
				// I really don't understand pointers sometimes.
				lan_adv* lan_inst = static_cast<lan_adv*>(user_data);

				std::string instance_name(namebuf, name_length); // namebuf from callback args
				Json::Value server = parseJson(std::string(txt[i].value.str, txt[i].value.length));
				std::string address = server["address"].asString();
				std::string port_str = server["port"].asString();
				std::string server_id = address + ":" + port_str;
				
				std::unique_lock lock(lan_inst->mutex);
				
				if (ttl == 0) {
					lan_inst->collected.erase(server_id);
					lan_inst->fresh = true;
				} else {
					// Normal TXT record, add/update server info
					lan_inst->collected.insert_or_assign(server_id, server);
					lan_inst.fresh = true;
				}
			}
            break;
        }
        case MDNS_RECORDTYPE_A: {
            struct sockaddr_in addr;
            mdns_record_parse_a(data, size, record_offset, record_length, &addr);
            // addr.sin_addr contains the IPv4 address
			actionstream << "IPv4 Address: " << inet_ntoa(addr.sin_addr) << std::endl;
            break;
        }
        default:
            break;
    }
    return 0; // Return nonzero to stop parsing early
}

lan_adv::lan_adv() : Thread("lan_adv")
{
}

void lan_adv::ask()
{
	if (!isRunning()) start();

	// God I hope this is the correct way to do this.
	alignas(4) void* buffer[2048];

	// Oh boy, I have to use void pointers. Fun.
	
	// From my VERY limited understanding, buffer is a void pointer
	// to the data we want to send.
	int error = mdns_query_send(
		socket, record_type, *service_name, service_name.length(),
		buffer, sizeof(buffer), 0
	);

	if (error < 0) {
		errorstream << "[Lan] failed to request available local server records. Error Code: " << error << std::endl;
	}
}

// Thanks to the mDNS lib, this function might be unneeded.
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

						setsockopt(socket_send.GetHandle(), IPservice_name_IP, IP_ADD_MEMBERSHIP,
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

			setsockopt(socket_send.GetHandle(), IPservice_name_IP, IP_ADD_MEMBERSHIP,
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
	// Switch from client to server mode.
	server_port = port;
	stop();
	start();
}

void *lan_adv::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER;

	setName("lan_adv " + (server_port ? std::string("server") : std::string("client")));
	//UDPSocket socket_recv(g_settings->getBool("ipv6_server"));

	const sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_port = MDNS_PORT;

	// For the sake of simplicty, I will use mdns_socket_open_ipv4
	// to open a socket instead of mdns_socket_setup_ipv4 with
	// Luanti's UDPSocket class. I hope that's ok.
	socket = mdns_socket_open_ipv4(*address);

	if (socket < 0) {
		errorstream << "[Lan] Failed to bind lan socket. Error Code: " << socket << std::endl;
		return nullptr;
	}

	/*
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

	setsockopt(socket_recv.GetHandle(), IPservice_name_IP, IP_ADD_MEMBERSHIP,
			(const char *)&mreq, sizeof(mreq));
	setsockopt(socket_recv.GetHandle(), IPservice_name_IPV6, IPV6_V6ONLY,
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
	*/

	/*std::unordered_map<std::string, uint64_t> limiter;
	const unsigned int packet_maxsize = 16384;
	char buffer[packet_maxsize];
	Json::Reader reader;
	std::string answer_str;
	*/
	Json::Value server;

	// mDNS records.
	mdns_record_t answer;
	mdns_record_t authority;
	mdns_record_t additional;

	int error = 0;

	// NOTE: This way of bundling server data should be consistant
	//       with how it's done in serverlist.cpp with sendAnnounce.
	//       Perhaps we can move such a function here to prevent
	//       cyclical referances because serverlist.cpp uses lan.cpp.
	if (server_port) {
		server["name"] = g_settings->get("server_name");
		server["description"] = g_settings->get("server_description");
		server["version"] = g_version_string;
		bool strict_checking = g_settings->getBool("strict_service_namecol_version_checking");
		server["service_name_min"] =
				strict_checking ? LATEST_service_nameCOL_VERSION : SERVER_service_nameCOL_VERSION_MIN;
		server["service_name_max"] = LATEST_service_nameCOL_VERSION;
		server["url"] = g_settings->get("server_url");
		server["creative"] = g_settings->getBool("creative_mode");
		server["damage"] = g_settings->getBool("enable_damage");
		server["password"] = g_settings->getBool("disallow_empty_password");
		server["pvp"] = g_settings->getBool("enable_pvp");
		server["port"] = server_port;
		server["clients"] = clients_num.load();
		server["clients_max"] = g_settings->getU16("max_users");
		server["service_name"] = service_name;

		std::string data = fastWriteJson(server);

		// Perhaps this bundling of mDNS records should be
		// it's own function?
		alignas(4) char buffer[2048];

		// Service type and instance
		const char* service_type = "_luanti._udp.local";
		const char* instance_name = "MyServer._luanti._udp.local"; // can be dynamic

		// PTR record: service type -> instance name
		answer = {};
		answer.name.str = service_type;
		answer.name.length = strlen(service_type);
		answer.type = MDNS_RECORDTYPE_PTR;
		answer.data.ptr.name.str = instance_name;
		answer.data.ptr.name.length = strlen(instance_name);
		answer.rclass = MDNS_CLASS_IN;
		answer.ttl = 60;

		// SRV record: instance name -> host:port
		authority[1] = {};
		authority[0].name.str = instance_name;
		authority[0].name.length = strlen(instance_name);
		authority[0].type = MDNS_RECORDTYPE_SRV;
		authority[0].data.srv.priority = 0;
		authority[0].data.srv.weight = 0;
		authority[0].data.srv.port = htons(server_port); // your server port
		authority[0].data.srv.name.str = "myhost.local"; // your hostname
		authority[0].data.srv.name.length = strlen("myhost.local");
		authority[0].rclass = MDNS_CLASS_IN;
		authority[0].ttl = 60;

		// TXT record: instance name -> key/values (e.g. version, description)
		txt[1] = {};
		txt[0].name.str = instance_name;
		txt[0].name.length = strlen(instance_name);
		txt[0].type = MDNS_RECORDTYPE_TXT;
		txt[0].data.txt.key.str = "json_data";
		txt[0].data.txt.key.length = strlen("json_data");
		txt[0].data.txt.value.str = data.c_str();
		txt[0].data.txt.value.length = strlen(data.c_str());
		txt[0].rclass = MDNS_CLASS_IN;
		txt[0].ttl = 60;

		// A record: hostname -> IPv4 address
		additional[1] = {};
		additional[0].name.str = "myhost.local";
		additional[0].name.length = strlen("myhost.local");
		additional[0].type = MDNS_RECORDTYPE_A;
		additional[0].rclass = MDNS_CLASS_IN;
		additional[0].ttl = 60;
		additional[0].data.a.addr.sin_family = AF_INET;
		inet_pton(AF_INET, "192.168.1.5", &additional[0].data.a.addr.sin_addr); // your IP

		// A server has started!
		error = mdns_announce_multicast(
			socket, buffer, sizeof(buffer),
			answer, authority, 1, additional, 1
		);

		if (error != 0) {
			errorstream << "[Lan] failed to announce mDNS service. Error Code: " << error << std::endl;
			return nullptr;
		}
	}

	while (isRunning() && !stopRequested()) {
		BEGIN_DEBUG_EXCEPTION_HANDLER;

		// Listen for data.
		alignas(4) char query_buffer[2048];
		alignas(4) void* user_data = this;
		mdns_query_recv(
			socket, query_buffer, sizeof(query_buffer), mdns_parse_callback,
			user_data, 0
		);
		/*
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
						<< "lan: want play " << addr_str << " " << p["service_name"] << std::endl;

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
				actionstream << "lan: want play " << addr_str << " " << p["service_name"] << std::endl;
			}
			if (p["port"].isInt()) {
				p["address"] = addr_str;
				auto key = addr_str + ":" + p["port"].asString();
				std::unique_lock lock(mutex);
				if (p["cmd"].asString() == "shutdown") {
					//infostream << "server shutdown " << key << "\n";
					collected.erase(key);
					fresh = true;
				} else if (p["service_name"] == service_name) {
					if (!collected.count(key))
						actionstream << "lan server start " << key << "\n";
					collected.insert_or_assign(key, p);
					fresh = true;
				}
			}

			//errorstream<<" current list: ";for (auto & i : collected) {errorstream<< i.first <<" ; ";}errorstream<<std::endl;
		}
		*/
		END_DEBUG_EXCEPTION_HANDLER;
	}

	if (server_port) {
		error = mdns_goodbye_multicast(
			socket, buffer, sizeof(buffer),
            answer, authority, 1, additional, 1
		);
		if (error != 0) {
			errorstream << "[Lan] failed to send mDNS goodbye. Error Code: " << error << std::endl;
		}
	}

	END_DEBUG_EXCEPTION_HANDLER;

	return nullptr;
}
