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
}

GUIDGenerator::GUIDGenerator() :
	m_uniform(0, UINT64_MAX)
{
	std::random_device rd;
	m_rand.seed(rd());
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
