// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "network/connection.h"
#include "network/mtp/impl.h"

#ifdef USE_NGTCP2_QUIC
#include "network/quic_connection.h"
#endif

namespace con
{

IConnection *createMTP(float timeout, bool ipv6, PeerHandler *handler)
{
#ifdef USE_NGTCP2_QUIC
	return createQUIC(timeout, ipv6, handler);
#else
	// safe minimum across internet networks for ipv4 and ipv6
	constexpr u32 MAX_PACKET_SIZE = 512;
	return new con::Connection(MAX_PACKET_SIZE, timeout, ipv6, handler);
#endif
}

}
