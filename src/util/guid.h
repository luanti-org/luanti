// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 SFENCE

#pragma once

#include "irrlichttypes.h"
#include "util/basic_macros.h"
#include <random>
#include <string>
#include <array>

class ServerEnvironment;

/**
 * A global unique identifier.
 * It is global because it stays valid forever.
 * It is unique because there are no collisions.
 */
struct MyGUID
{
	std::array<char, 16> bytes;

	std::string base64() const;
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);
};

class GUIDGenerator
{
	DISABLE_CLASS_COPY(GUIDGenerator)

public:

	GUIDGenerator();

	/**
	 * Generates the next GUID, which it will never return again.
	 * @return the new GUID
	 */
	MyGUID next();

private:

	std::random_device m_rand;
	std::uniform_int_distribution<u64> m_uniform;
};
