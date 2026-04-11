// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "exceptions.h"
#include "sscsm_serialize.h"
#include <iosfwd>
#include <memory>
#include <sstream>
#include <string>

class SSCSMController;
class Client;

// Base for "answer" payloads. Each request type has its own concrete Answer
// struct that knows how to (de)serialize itself.
struct ISSCSMAnswer
{
	virtual ~ISSCSMAnswer() = default;
};

// Wire format for an answer: just the answer body. The receiving side already
// knows the expected concrete type from the request it sent.
using SerializedSSCSMAnswer = std::string;

// Request made by the sscsm env to the main env.
struct ISSCSMRequest
{
	virtual ~ISSCSMRequest() = default;

	virtual SSCSMRequestType getType() const = 0;
	virtual void serializeBody(std::ostream &os) const = 0;

	virtual SerializedSSCSMAnswer exec(Client *client) = 0;
};

// Wire format for a request: [type tag u8][body bytes].
using SerializedSSCSMRequest = std::string;

// Top-level: serialize a concrete request type.
template <typename T>
inline SerializedSSCSMRequest serializeSSCSMRequest(const T &request)
{
	std::ostringstream os{std::ios::binary};
	os.put(static_cast<char>(static_cast<u8>(T::TYPE)));
	request.serializeBody(os);
	return os.str();
}

// Top-level: serialize a concrete answer type.
template <typename T>
inline SerializedSSCSMAnswer serializeSSCSMAnswer(T &&answer)
{
	std::ostringstream os{std::ios::binary};
	answer.serializeBody(os);
	return os.str();
}

// Top-level: deserialize an answer when the receiver knows the expected type.
template <typename T>
inline T deserializeSSCSMAnswer(const SerializedSSCSMAnswer &data)
{
	std::istringstream is{data, std::ios::binary};
	return T::deserializeBody(is);
}

// Top-level: deserialize a polymorphic request by reading its type tag and
// dispatching to the appropriate concrete deserializer.
std::unique_ptr<ISSCSMRequest> deserializeSSCSMRequest(const SerializedSSCSMRequest &data);
