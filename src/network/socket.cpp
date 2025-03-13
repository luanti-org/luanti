// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "socket.h"

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include "util/string.h"
#include "util/numeric.h"
#include "constants.h"
#include "debug.h"
#include "log.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define LAST_SOCKET_ERR() WSAGetLastError()
#define SOCKET_ERR_STR(e) itos(e)
typedef int socklen_t;
#else
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#define LAST_SOCKET_ERR() (errno)
#define SOCKET_ERR_STR(e) strerror(e)
#endif

static bool g_sockets_initialized = false;

// Initialize sockets
void sockets_init()
{
#ifdef _WIN32
	// Windows needs sockets to be initialized before use
	WSADATA WsaData;
	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != NO_ERROR)
		throw SocketException("WSAStartup failed");
#endif
	g_sockets_initialized = true;
}

void sockets_cleanup()
{
#ifdef _WIN32
	// On Windows, cleanup sockets after use
	WSACleanup();
#endif
	g_sockets_initialized = false;
}

/*
	UDPSocket
*/

UDPSocket::UDPSocket(bool ipv6)
{
	init(ipv6, false);
}

bool UDPSocket::init(bool ipv6, bool noExceptions)
{
	if (!g_sockets_initialized) {
		verbosestream << "Sockets not initialized" << std::endl;
		return false;
	}

	if (m_handle >= 0) {
		auto msg = "Cannot initialize socket twice";
		verbosestream << msg << std::endl;
		if (noExceptions)
			return false;
		throw SocketException(msg);
	}

	// Use IPv6 if specified
	m_addr_family = ipv6 ? AF_INET6 : AF_INET;
	m_handle = socket(m_addr_family, SOCK_DGRAM, IPPROTO_UDP);

	if (m_handle < 0) {
		auto msg = std::string("Failed to create socket: ") +
			SOCKET_ERR_STR(LAST_SOCKET_ERR());
		verbosestream << msg << std::endl;
		if (noExceptions)
			return false;
		throw SocketException(msg);
	}

	setTimeoutMs(0);

	return true;
}

UDPSocket::~UDPSocket()
{
	if (m_handle >= 0) {
#ifdef _WIN32
		closesocket(m_handle);
#else
		close(m_handle);
#endif
	}
}

void UDPSocket::Bind(Address addr)
{
	if (addr.getFamily() != m_addr_family) {
		const char *errmsg =
				"Socket and bind address families do not match";
		errorstream << "Bind failed: " << errmsg << std::endl;
		throw SocketException(errmsg);
	}

	if (m_addr_family == AF_INET6) {
		// Allow our socket to accept both IPv4 and IPv6 connections
		// required on Windows:
		// <https://msdn.microsoft.com/en-us/library/windows/desktop/bb513665(v=vs.85).aspx>
		int value = 0;
		if (setsockopt(m_handle, IPPROTO_IPV6, IPV6_V6ONLY,
				reinterpret_cast<char *>(&value), sizeof(value)) != 0) {
			auto errmsg = SOCKET_ERR_STR(LAST_SOCKET_ERR());
			errorstream << "Failed to disable V6ONLY: " << errmsg
				<< "\nTry disabling ipv6_server to fix this." << std::endl;
			throw SocketException(errmsg);
		}
	}

	int ret = 0;

	if (m_addr_family == AF_INET6) {
		struct sockaddr_in6 address;
		memset(&address, 0, sizeof(address));

		address.sin6_family = AF_INET6;
		address.sin6_addr = addr.getAddress6();
		address.sin6_port = htons(addr.getPort());

		ret = bind(m_handle, (const struct sockaddr *) &address,
				sizeof(struct sockaddr_in6));
	} else {
		struct sockaddr_in address;
		memset(&address, 0, sizeof(address));

		address.sin_family = AF_INET;
		address.sin_addr = addr.getAddress();
		address.sin_port = htons(addr.getPort());

		ret = bind(m_handle, (const struct sockaddr *) &address,
			sizeof(struct sockaddr_in));
	}

	if (ret < 0) {
		tracestream << (int)m_handle << ": Bind failed: "
			<< SOCKET_ERR_STR(LAST_SOCKET_ERR()) << std::endl;
		throw SocketException("Failed to bind socket");
	}
}

void UDPSocket::Send(const Address &destination, const void *data, int size)
{
	bool dumping_packet = false; // for INTERNET_SIMULATOR

	if (INTERNET_SIMULATOR)
		dumping_packet = myrand() % INTERNET_SIMULATOR_PACKET_LOSS == 0;

	if (dumping_packet) {
		// Lol let's forget it
		tracestream << "UDPSocket::Send(): INTERNET_SIMULATOR: dumping packet."
			<< std::endl;
		return;
	}

	if (destination.getFamily() != m_addr_family)
		throw SendFailedException("Address family mismatch");

	int sent;
	if (m_addr_family == AF_INET6) {
		struct sockaddr_in6 address = {};
		address.sin6_family = AF_INET6;
		address.sin6_addr = destination.getAddress6();
		address.sin6_port = htons(destination.getPort());

		sent = sendto(m_handle, (const char *)data, size, 0,
				(struct sockaddr *)&address, sizeof(struct sockaddr_in6));
	} else {
		struct sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr = destination.getAddress();
		address.sin_port = htons(destination.getPort());

		sent = sendto(m_handle, (const char *)data, size, 0,
				(struct sockaddr *)&address, sizeof(struct sockaddr_in));
	}

	if (sent != size)
		throw SendFailedException("Failed to send packet");
}

int UDPSocket::Receive(Address &sender, void *data, int size)
{
	// Return on timeout
	assert(m_timeout_ms >= 0);
	if (!WaitData(m_timeout_ms))
		return -1;

	size = MYMAX(size, 0);

	int received;
	if (m_addr_family == AF_INET6) {
		struct sockaddr_in6 address;
		memset(&address, 0, sizeof(address));
		socklen_t address_len = sizeof(address);

		received = recvfrom(m_handle, (char *)data, size, 0,
				(struct sockaddr *)&address, &address_len);

		if (received < 0)
			return -1;

		u16 address_port = ntohs(address.sin6_port);
		const auto *bytes = reinterpret_cast<IPv6AddressBytes*>
			(address.sin6_addr.s6_addr);
		sender = Address(bytes, address_port);
	} else {
		struct sockaddr_in address;
		memset(&address, 0, sizeof(address));

		socklen_t address_len = sizeof(address);

		received = recvfrom(m_handle, (char *)data, size, 0,
				(struct sockaddr *)&address, &address_len);

		if (received < 0)
			return -1;

		u32 address_ip = ntohl(address.sin_addr.s_addr);
		u16 address_port = ntohs(address.sin_port);

		sender = Address(address_ip, address_port);
	}

	return received;
}

void UDPSocket::setTimeoutMs(int timeout_ms)
{
	m_timeout_ms = timeout_ms;
}

bool UDPSocket::WaitData(int timeout_ms)
{
	timeout_ms = MYMAX(timeout_ms, 0);

#ifdef _WIN32
	WSAPOLLFD pfd;
	pfd.fd = m_handle;
	pfd.events = POLLRDNORM;

	int result = WSAPoll(&pfd, 1, timeout_ms);
#else
	struct pollfd pfd;
	pfd.fd = m_handle;
	pfd.events = POLLIN;

	int result = poll(&pfd, 1, timeout_ms);
#endif

	if (result == 0) {
		return false; // No data
	} else if (result > 0) {
		// There might be data
		return pfd.revents != 0;
	}

	// Error case
	int e = LAST_SOCKET_ERR();

#ifdef _WIN32
	if (e == WSAEINTR || e == WSAEBADF) {
#else
	if (e == EINTR || e == EBADF) {
#endif
		// N.B. poll() fails when sockets are destroyed on Connection's dtor
		// with EBADF. Instead of doing tricky synchronization, allow this
		// thread to exit but don't throw an exception.
		return false;
	}

	tracestream << (int)m_handle << ": poll failed: "
		<< SOCKET_ERR_STR(e) << std::endl;

	throw SocketException("poll failed");
}
