// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "exceptions.h"
#include <memory>
#include <type_traits>

class SSCSMController;
class Client;

// FIXME: remove once we have actual serialization
// this is just here so we can instead put them into unique_ptr
struct ISSCSMAnswer
{
	virtual ~ISSCSMAnswer() = default;
};

// FIXME: actually serialize, and replace this with a std::vector<u8>.
//        also update function argument declarations, to take
//        `const SerializedSSCSMAnswer &` or whatever
// (not polymorphic. the receiving side will know the answer type that is in here)
using SerializedSSCSMAnswer = std::unique_ptr<ISSCSMAnswer>;

// Request made by the sscsm env to the main env.
struct ISSCSMRequest
{
	virtual ~ISSCSMRequest() = default;

	virtual SerializedSSCSMAnswer exec(Client *client) = 0;
};

// FIXME: as above, actually serialize
// (polymorphic. this can be any ISSCSMRequest. ==> needs type tag)
using SerializedSSCSMRequest = std::unique_ptr<ISSCSMRequest>;

template <typename T>
inline SerializedSSCSMRequest serializeSSCSMRequest(const T &request)
{
	static_assert(std::is_base_of_v<ISSCSMRequest, T>);

	// FIXME: this will need to use a type tag for T

	return std::make_unique<T>(request);
}

template <typename T>
inline T deserializeSSCSMAnswer(SerializedSSCSMAnswer answer_serialized)
{
	static_assert(std::is_base_of_v<ISSCSMAnswer, T>);

	// FIXME: should look something like this:
	// return sscsm::Serializer<T>{}.deserialize(answer_serialized);
	// (note: answer_serialized does not need a type tag)

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

	// FIXME: should look something like this:
	// return sscsm::Serializer<T>{}.serialize(request);

	return std::make_unique<T>(std::move(answer));
}

inline std::unique_ptr<ISSCSMRequest> deserializeSSCSMRequest(SerializedSSCSMRequest request_serialized)
{
	// FIXME: The actual deserialization will have to use a type tag, and then
	// choose the appropriate deserializer.
	return request_serialized;
}
