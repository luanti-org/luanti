// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "inetsender.h"

namespace server {

/**
 * Outgoing-network facade used by `Server`.
 *
 * `INetServer` publicly extends `INetSender` (so every per-packet
 * `sendXxx` helper lives on the same object) and additionally
 * bundles a small set of convenience read-only getters and typed
 * lookups the rest of the server needs while building packets
 * (protocol-version queries, settings, etc.). All packet
 * serialization is concentrated in `netsender.cpp`.
 *
 * Production implementation: `ConnectionNetServer`
 * (`src/network/netserver.h`).
 *
 * See `.kilo/plans/luanti-server-refactor-modular.md` §10.5 and
 * the step-9 follow-up (typed sender API).
 */
class INetServer : public INetSender
{
public:
	virtual ~INetServer() = default;
};

} // namespace server
