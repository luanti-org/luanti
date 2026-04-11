// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sscsm_serialize.h"
#include <iosfwd>
#include <memory>
#include <sstream>
#include <string>

class SSCSMEnvironment;

// Event triggered from the main env for the SSCSM env.
struct ISSCSMEvent
{
	virtual ~ISSCSMEvent() = default;

	virtual SSCSMEventType getType() const = 0;
	virtual void serializeBody(std::ostream &os) const = 0;

	// Note: No return value (difference to ISSCSMRequest). These are not callbacks
	// that you can run at arbitrary locations, because the untrusted code could
	// then clobber your local variables.
	virtual void exec(SSCSMEnvironment *env) = 0;
};

// Wire format for an event: [type tag u8][body bytes].
using SerializedSSCSMEvent = std::string;

// Top-level: serialize a concrete event type, written into an existing stream.
template <typename T>
inline void serializeSSCSMEventInto(const T &event, std::ostream &os)
{
	os.put(static_cast<char>(static_cast<u8>(T::TYPE)));
	event.serializeBody(os);
}

// Top-level: serialize a polymorphic event into a fresh stream.
inline void serializeSSCSMEventInto(const ISSCSMEvent &event, std::ostream &os)
{
	os.put(static_cast<char>(static_cast<u8>(event.getType())));
	event.serializeBody(os);
}

// Top-level: deserialize a polymorphic event from a stream by reading its
// type tag and dispatching.
std::unique_ptr<ISSCSMEvent> deserializeSSCSMEvent(std::istream &is);
