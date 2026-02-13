// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <type_traits>

class SSCSMEnvironment;

// Event triggered from the main env for the SSCSM env.
struct ISSCSMEvent
{
	virtual ~ISSCSMEvent() = default;

	// Note: No return value (difference to ISSCSMRequest). These are not callbacks
	// that you can run at arbitrary locations, because the untrusted code could
	// then clobber your local variables.
	virtual void exec(SSCSMEnvironment *cntrl) = 0;
};

// FIXME: actually serialize, and replace this with a string
using SerializedSSCSMEvent = std::unique_ptr<ISSCSMEvent>;

template <typename T>
inline SerializedSSCSMEvent serializeSSCSMEvent(const T &event)
{
	static_assert(std::is_base_of_v<ISSCSMEvent, T>);

	return std::make_unique<T>(event);
}

inline std::unique_ptr<ISSCSMEvent> deserializeSSCSMEvent(SerializedSSCSMEvent event_serialized)
{
	// The actual deserialization will have to use a type tag, and then choose
	// the appropriate deserializer.
	return event_serialized;
}
