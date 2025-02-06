// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 SFENCE

#pragma once

#include "irrlichttypes.h"
#include "util/basic_macros.h"
#include <random>
#include <string>

class ServerEnvironment;

/**
 * A global unique identifier.
 * It is global because it stays valid in a world forever.
 * It is unique because there are no collisions.
 */
typedef std::string GUId;

/**
 * Generates infinitely many guids.
 */
class GUIdGenerator {
public:
	/**
	 * Creates a new uninitialized generator.
	 */
	GUIdGenerator();

	~GUIdGenerator() = default;
	DISABLE_CLASS_COPY(GUIdGenerator)

	/**
	 * Generates the next guid, which it will never return again.
	 * @return the new guid
	 */
	GUId next();

private:
	void setServerEnvironment(ServerEnvironment *env) { m_env = env; }

	ServerEnvironment *m_env;
	std::random_device m_rand;
	std::uniform_int_distribution<u64> m_uniform;

	friend class ServerEnvironment;
};
