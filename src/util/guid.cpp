// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 SFENCE

#include "guid.h"
#include <cstring>
#include <sstream>
#include <string_view>

#include "exceptions.h"
#include "util/base64.h"
#include "log.h"
#include "porting.h"
#include "util/numeric.h"

std::string MyGUID::base64() const
{
	return base64_encode(std::string_view(&bytes[0], bytes.size()));
}

void MyGUID::serialize(std::ostream &os) const
{
	os.write(&bytes[0], bytes.size());
}

void MyGUID::deSerialize(std::istream &is)
{
	is.read(&bytes[0], bytes.size());
	if (is.eof())
	    throw SerializationError("GUID data truncated");
}

GUIDGenerator::GUIDGenerator() :
	m_uniform(0, UINT64_MAX)
{
	u64 seed;
	if (!porting::secure_rand_fill_buf(&seed, sizeof(seed))) {
		// main.cpp initializes our internal RNG as good as possible, so fall back to it
		myrand_bytes(&seed, sizeof(seed));
	}

	// Make sure we're not losing entropy or providing too few
	static_assert(sizeof(seed) == sizeof(decltype(m_rand)::result_type), "seed type mismatch");
	m_rand.seed(seed);
}

MyGUID GUIDGenerator::next()
{
	u64 rand1 = m_uniform(m_rand);
	u64 rand2 = m_uniform(m_rand);

	std::array<char, 16> bytes;
	std::memcpy(&bytes[0], reinterpret_cast<char*>(&rand1), 8);
	std::memcpy(&bytes[8], reinterpret_cast<char*>(&rand2), 8);
	return MyGUID{bytes};
}
