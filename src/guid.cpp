// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 SFENCE

#include "guid.h"
#include <sstream>

#include "serverenvironment.h"
#include "util/base64.h"

GUIdGenerator::GUIdGenerator() :
	m_uniform(0, UINT64_MAX)
{
	if (m_rand.entropy() <= 0.010)
		throw BaseException("The system's provided random generator does not match "
				"the entropy requirements for the GUId generator.");
}

GUId GUIdGenerator::next()
{
	std::stringstream s_guid;

	s_guid << std::hex << std::setfill('0');
	s_guid << std::setw(8) << (m_uniform(m_rand) & 0xFFFFFFFF) << "-";
	s_guid << std::setw(4) << (m_uniform(m_rand) & 0xFFFF) << "-";
	s_guid << std::setw(4) << (m_uniform(m_rand) & 0xFFFF) << "-";
	s_guid << std::setw(4) << (m_uniform(m_rand) & 0xFFFF) << "-";
	s_guid << std::setw(8) << (m_uniform(m_rand) & 0xFFFFFFFF);
	s_guid << std::setw(4) << (m_uniform(m_rand) & 0xFFFF);

	return s_guid.str();
}

