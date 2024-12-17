// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "exceptions.h"
#include <memory>
#include <type_traits>

class SSCSMController;
class Client;

struct ISSCSMAnswer
{
	virtual ~ISSCSMAnswer() = default;
};

// FIXME: actually serialize, and replace this with a string
using SerializedSSCSMAnswer = std::unique_ptr<ISSCSMAnswer>;

// Request made by the sscsm env to the main env.
struct ISSCSMRequest
{
	virtual ~ISSCSMRequest() = default;

	virtual SerializedSSCSMAnswer exec(Client *client) = 0;
};

// FIXME: actually serialize, and replace this with a string
using SerializedSSCSMRequest = std::unique_ptr<ISSCSMRequest>;

template <typename T>
inline SerializedSSCSMRequest serializeSSCSMRequest(const T &request)
{
	static_assert(std::is_base_of_v<ISSCSMRequest, T>);

	return std::make_unique<T>(request);
}

template <typename T>
inline T deserializeSSCSMAnswer(SerializedSSCSMAnswer answer_serialized)
{
	static_assert(std::is_base_of_v<ISSCSMAnswer, T>);

	// dynamic cast in place of actual deserialization
	auto ptr = dynamic_cast<T *>(answer_serialized.get());
	if (!ptr) {
		throw SerializationError("deserializeSSCSMAnswer failed");
	}
	return std::move(*ptr);
}

template <typename T>
inline SerializedSSCSMAnswer serializeSSCSMAnswer(T &&answer)
{
	static_assert(std::is_base_of_v<ISSCSMAnswer, T>);

	return std::make_unique<T>(std::move(answer));
}

inline std::unique_ptr<ISSCSMRequest> deserializeSSCSMRequest(SerializedSSCSMRequest request_serialized)
{
	// The actual deserialization will have to use a type tag, and then choose
	// the appropriate deserializer.
	return request_serialized;
}
