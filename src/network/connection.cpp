// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "network/connection.h"
#include "network/mtp/impl.h"

namespace con
{

IConnection *createMTP(bool is_server, UDPSocket &&socket, PeerHandler *handler)
{
	// safe minimum across internet networks for ipv4 and ipv6
	constexpr u32 MAX_PACKET_SIZE = 512;
	return new con::Connection(MAX_PACKET_SIZE, CONNECTION_TIMEOUT, std::move(socket), is_server, handler);
}

}
