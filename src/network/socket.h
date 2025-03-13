// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <ostream>
#include <cstring>
#include "address.h"
#include "irrlichttypes.h"
#include "networkexceptions.h"

void sockets_init();
void sockets_cleanup();

class UDPSocket
{
public:
	UDPSocket() = default;
	UDPSocket(bool ipv6); // calls init()
	~UDPSocket();
	bool init(bool ipv6, bool noExceptions = false);

	void Bind(Address addr);

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
