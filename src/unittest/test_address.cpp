// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <memory>
#include "log.h"
#include "settings.h"
#include "network/socket.h"

class TestAddress : public TestBase
{
public:
	TestAddress() { TestManager::registerTestModule(this); }

	const char *getName() { return "TestAddress"; }

	void runTests(IGameDef *gamedef);

	void testBasic();
	void testIsLocalhost();
	void testResolve();
};

static TestAddress g_test_instance;

void TestAddress::runTests(IGameDef *gamedef)
{
	TEST(testBasic);
	TEST(testIsLocalhost);
	TEST(testResolve);
}

void TestAddress::testBasic()
{
	Address tmp;

	UASSERT(!tmp.isValid());
	UASSERTEQ(int, tmp.getFamily(), 0);

	tmp = Address(static_cast<u32>(0), 0);
	UASSERT(tmp.isValid());
	UASSERTEQ(int, tmp.getFamily(), AF_INET);
	UASSERT(tmp.isAny());

	tmp = Address(static_cast<IPv6AddressBytes*>(nullptr), 0);
	UASSERT(tmp.isValid());
	UASSERTEQ(int, tmp.getFamily(), AF_INET6);
	UASSERT(tmp.isAny());
}

void TestAddress::testIsLocalhost()
{
	// v4
	UASSERT(Address(127, 0, 0, 1, 0).isLocalhost());
	UASSERT(Address(127, 254, 12, 99, 0).isLocalhost());
	UASSERT(Address(127, 188, 255, 247, 0).isLocalhost());
	UASSERT(!Address(126, 255, 255, 255, 0).isLocalhost());
	UASSERT(!Address(128, 0, 0, 0, 0).isLocalhost());
	UASSERT(!Address(1, 0, 0, 0, 0).isLocalhost());
	UASSERT(!Address(255, 255, 255, 255, 0).isLocalhost());
	UASSERT(!Address(36, 45, 99, 158, 0).isLocalhost());
	UASSERT(!Address(172, 45, 37, 68, 0).isLocalhost());

	// v6
	auto ipv6Bytes = std::make_unique<IPv6AddressBytes>();
	std::vector<u8> ipv6RawAddr = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
	memcpy(ipv6Bytes->bytes, &ipv6RawAddr[0], 16);
	UASSERT(Address(ipv6Bytes.get(), 0).isLocalhost())

	ipv6RawAddr = {16, 34, 0, 0, 0, 0, 29, 0, 0, 0, 188, 0, 0, 0, 0, 14};
	memcpy(ipv6Bytes->bytes, &ipv6RawAddr[0], 16);
	UASSERT(!Address(ipv6Bytes.get(), 0).isLocalhost())
}

void TestAddress::testResolve()
{
	// Empty test
	{
		Address tmp(1, 2, 3, 4, 5);
		tmp.Resolve("");

		UASSERT(tmp.isValid());
		UASSERT(tmp.isAny());
	}

	// Localhost test
	Address result, fallback;
	result.Resolve("localhost", &fallback);

	UASSERT(result.isValid());
	UASSERT(result.isLocalhost());

	if (fallback.isValid()) {
		UASSERT(fallback.isLocalhost());

		// the result should be ::1 and 127.0.0.1 so the fallback addr should be
		// of a different family
		UASSERTCMP(int, !=, result.getFamily(), fallback.getFamily());
	} else if (g_settings->getBool("enable_ipv6")) {
		warningstream << "Couldn't verify Address::Resolve fallback (no IPv6?)"
			<< std::endl;
	}
}
