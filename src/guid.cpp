// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 SFENCE

#include "guid.h"
#include <sstream>

#include "serverenvironment.h"
#include "util/serialize.h"

GUId::GUId(const std::string &str) :
	text(str)
{
}

void GUId::generateText() {
	std::stringstream s_guid;

	s_guid << std::hex << std::setfill('0');
	s_guid << std::setw(8) << p1 << "-";
	s_guid << std::setw(4) << p2 << "-";
	s_guid << std::setw(4) << p3 << "-";
	s_guid << std::setw(4) << p4 << "-";
	s_guid << std::setw(8) << p5;
	s_guid << std::setw(4) << p6;

	text = s_guid.str();
}

void GUId::serialize(std::ostringstream &os) const {
	writeU32(os, p1);
	writeU16(os, p2);
	writeU16(os, p3);
	writeU16(os, p4);
	writeU32(os, p5);
	writeU16(os, p6);
}

void GUId::deSerialize(std::istream &is) {
	p1 = readU32(is);
	p2 = readU16(is);
	p3 = readU16(is);
	p4 = readU16(is);
	p5 = readU32(is);
	p6 = readU16(is);

	generateText();
}

GUIdGenerator::GUIdGenerator() :
	m_uniform(0, UINT64_MAX)
{
	if (m_rand.entropy() <= 0.010)
		throw BaseException("The system's provided random generator does not match "
				"the entropy requirements for the GUId generator.");
}

GUId GUIdGenerator::next()
{
	GUId result;

	u64 rand1 = m_uniform(m_rand);
	u64 rand2 = m_uniform(m_rand);

	result.p1 = rand1 & 0xFFFFFFFF;
	result.p2 = (rand1 >> 32) & 0xFFFF;
	result.p3 = (rand1 >> 48) & 0xFFFF;
	result.p4 = rand2 & 0xFFFF;
	result.p5 = (rand2 >> 16) & 0xFFFFFFFF;
	result.p6 = (rand2 >> 48) & 0xFFFF;

	result.generateText();

	return result;
}
