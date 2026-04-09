// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "address.h"

#include <iostream>
#include <cstring>
#include <cerrno>
#include "network/networkexceptions.h"
#include "settings.h"

#ifdef _WIN32
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
#define LAST_SOCKET_ERR() (errno)
typedef int socket_t;
#endif

/*
	Address
*/

Address::Address()
{
	memset(&m_address, 0, sizeof(m_address));
}

Address::Address(u32 address, u16 port)
{
	memset(&m_address, 0, sizeof(m_address));
	setAddress(address);
	setPort(port);
}

Address::Address(u8 a, u8 b, u8 c, u8 d, u16 port)
{
	memset(&m_address, 0, sizeof(m_address));
	setAddress(a, b, c, d);
	setPort(port);
}

Address::Address(const IPv6AddressBytes *ipv6_bytes, u16 port)
{
	memset(&m_address, 0, sizeof(m_address));
	setAddress(ipv6_bytes);
	setPort(port);
}

// Equality (address family, IP and port must be equal)
bool Address::operator==(const Address &other) const
{
	if (other.m_addr_family != m_addr_family || other.m_port != m_port)
		return false;

	if (m_addr_family == AF_INET) {
		return m_address.ipv4.s_addr == other.m_address.ipv4.s_addr;
	}

	if (m_addr_family == AF_INET6) {
		return memcmp(m_address.ipv6.s6_addr,
				other.m_address.ipv6.s6_addr, 16) == 0;
	}

	return false;
}

void Address::Resolve(const char *name, Address *fallback)
{
	if (!name || name[0] == 0) {
		if (m_addr_family == AF_INET)
			setAddress(static_cast<u32>(0));
		else if (m_addr_family == AF_INET6)
			setAddress(static_cast<IPv6AddressBytes*>(nullptr));
		if (fallback)
			*fallback = Address();
		return;
	}

	const auto &copy_from_ai = [] (const struct addrinfo *ai, Address *to) {
		if (ai->ai_family == AF_INET) {
			struct sockaddr_in *t = (struct sockaddr_in *)ai->ai_addr;
			to->m_addr_family = AF_INET;
			to->m_address.ipv4 = t->sin_addr;
		} else if (ai->ai_family == AF_INET6) {
			struct sockaddr_in6 *t = (struct sockaddr_in6 *)ai->ai_addr;
			to->m_addr_family = AF_INET6;
			to->m_address.ipv6 = t->sin6_addr;
		} else {
			to->m_addr_family = 0;
		}
	};

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));

	// set a type, so every unique address is only returned once
	hints.ai_socktype = SOCK_DGRAM;
	if (g_settings->getBool("enable_ipv6")) {
		// AF_UNSPEC allows both IPv6 and IPv4 addresses to be returned
		hints.ai_family = AF_UNSPEC;
	} else {
		hints.ai_family = AF_INET;
	}
	hints.ai_flags = AI_ADDRCONFIG;

	// Do getaddrinfo()
	struct addrinfo *resolved = nullptr;
	int e = getaddrinfo(name, nullptr, &hints, &resolved);
	if (e != 0)
		throw ResolveError(gai_strerror(e));
	assert(resolved);

	// Copy data
	copy_from_ai(resolved, this);
	if (fallback) {
		*fallback = Address();
		if (resolved->ai_next)
			copy_from_ai(resolved->ai_next, fallback);
	}

	freeaddrinfo(resolved);
}

// IP address -> textual representation
std::string Address::serializeString() const
{
	const void *src = nullptr;
	switch (m_addr_family) {
		case AF_INET:  src = &m_address.ipv4; break;
		case AF_INET6: src = &m_address.ipv6; break;
	}

	if (!src)
		return "<unhandled-addr-family>";

	char str[INET6_ADDRSTRLEN];
	if (inet_ntop(m_addr_family, src, str, sizeof(str)) == nullptr)
		return "";
	return str;
}

bool Address::isAny() const
{
	if (m_addr_family == AF_INET) {
		return m_address.ipv4.s_addr == 0;
	} else if (m_addr_family == AF_INET6) {
		static const char zero[16] = {0};
		return memcmp(m_address.ipv6.s6_addr, zero, 16) == 0;
	}

	return false;
}

void Address::setAddress(u32 address)
{
	m_addr_family = AF_INET;
	m_address.ipv4.s_addr = htonl(address);
}

void Address::setAddress(u8 a, u8 b, u8 c, u8 d)
{
	u32 addr = (a << 24) | (b << 16) | (c << 8) | d;
	setAddress(addr);
}

void Address::setAddress(const IPv6AddressBytes *ipv6_bytes)
{
	m_addr_family = AF_INET6;
	if (ipv6_bytes)
		memcpy(m_address.ipv6.s6_addr, ipv6_bytes->bytes, 16);
	else
		memset(m_address.ipv6.s6_addr, 0, 16);
}

void Address::setPort(u16 port)
{
	m_port = port;
}

void Address::print(std::ostream& s) const
{
	if (m_addr_family == AF_INET6)
		s << "[" << serializeString() << "]:" << m_port;
	else if (m_addr_family == AF_INET)
		s << serializeString() << ":" << m_port;
	else
		s << "(undefined)";
}

bool Address::isLocalhost() const
{
	if (m_addr_family == AF_INET6) {
		static const u8 localhost_bytes[] = {
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
		static const u8 mapped_ipv4_localhost[] = {
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 0};

		auto *addr = m_address.ipv6.s6_addr;

		return memcmp(addr, localhost_bytes, 16) == 0 ||
			memcmp(addr, mapped_ipv4_localhost, 13) == 0;
	} else if (m_addr_family == AF_INET) {
		auto addr = ntohl(m_address.ipv4.s_addr);
		return (addr >> 24) == 0x7f;
	}
	return false;
}

bool Address::isLan() const
{
	if (isLocalhost())
		return true;

	if (m_addr_family == AF_INET) {
		auto addr = ntohl(m_address.ipv4.s_addr);
		// 10.0.0.0/8
		if ((addr >> 24) == 10)
			return true;
		// 172.16.0.0/12
		if ((addr >> 20) == (172 << 4 | 1))
			return true;
		// 192.168.0.0/16
		if ((addr >> 16) == (192 << 8 | 168))
			return true;
		// 169.254.0.0/16 (link-local)
		if ((addr >> 16) == (169 << 8 | 254))
			return true;
	} else if (m_addr_family == AF_INET6) {
		auto *addr = m_address.ipv6.s6_addr;
		// fe80::/10 (link-local)
		if (addr[0] == 0xfe && (addr[1] & 0xc0) == 0x80)
			return true;
		// fc00::/7 (unique local)
		if ((addr[0] & 0xfe) == 0xfc)
			return true;
		// ::ffff:0:0/96 (IPv4-mapped) — check the mapped IPv4 address
		static const u8 mapped_prefix[] = {
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
		if (memcmp(addr, mapped_prefix, 12) == 0) {
			u32 ipv4 = (static_cast<u32>(addr[12]) << 24) |
					(static_cast<u32>(addr[13]) << 16) |
					(static_cast<u32>(addr[14]) << 8) | addr[15];
			if ((ipv4 >> 24) == 10)
				return true;
			if ((ipv4 >> 20) == (172 << 4 | 1))
				return true;
			if ((ipv4 >> 16) == (192 << 8 | 168))
				return true;
			if ((ipv4 >> 16) == (169 << 8 | 254))
				return true;
		}
	}
	return false;
}
