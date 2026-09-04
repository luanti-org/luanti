// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "address.h"

void sockets_init();
void sockets_cleanup();

class UDPSocket
{
public:
	UDPSocket() = default;
	UDPSocket(UDPSocket &&other):
		m_handle(other.m_handle),
		m_timeout_ms(other.m_timeout_ms),
		m_addr_family(other.m_addr_family)
	{
		other.m_handle = -1;
	}
	~UDPSocket()
	{
		Close();
	}
	UDPSocket &operator=(UDPSocket &&other)
	{
		Close();
		m_handle = other.m_handle;
		m_timeout_ms = other.m_timeout_ms;
		m_addr_family = other.m_addr_family;
		other.m_handle = -1;
		return *this;
	}

	static UDPSocket Create(Address addr)
	{
		UDPSocket socket;
		socket.Bind(addr);
		return socket;
	}

	static UDPSocket CreateEphemeral(bool ipv6)
	{
		if (ipv6)
			return Create(Address(nullptr, 0));
		else
			return Create(Address(u32(0), 0));
	}

	void Init(bool ipv6);
	void Bind(Address addr);
	void Close();

	Address GetLocalAddress();

	void Send(const Address &destination, const void *data, int size);
	// Returns -1 if there is no data
	int Receive(Address &sender, void *data, int size);
	void setTimeoutMs(int timeout_ms);
	// Returns true if there is data, false if timeout occurred
	bool WaitData(int timeout_ms);

	// Debugging purposes only
	int GetHandle() const { return m_handle; };

private:
	int m_handle = -1;
	int m_timeout_ms = -1;
	unsigned short m_addr_family = 0;
};
