// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include "log.h"
#include "settings.h"
#include "network/socket.h"

class TestSocket : public TestBase {
public:
	TestSocket()
	{
		if (INTERNET_SIMULATOR == false)
			TestManager::registerTestModule(this);
	}

	const char *getName() { return "TestSocket"; }

	void runTests(IGameDef *gamedef);

	void testIPv4Socket();
	void testIPv6Socket();

	static const int port = 30003;
};

static TestSocket g_test_instance;

void TestSocket::runTests(IGameDef *gamedef)
{
	TEST(testIPv4Socket);

	if (g_settings->getBool("enable_ipv6"))
		TEST(testIPv6Socket);
}

////////////////////////////////////////////////////////////////////////////////

void TestSocket::testIPv4Socket()
{
	Address address(0, 0, 0, 0, port);
	Address bind_addr(0, 0, 0, 0, port);

	/*
	 * Try to use the bind_address for servers with no localhost address
	 * For example: FreeBSD jails
	 */
	std::string bind_str = g_settings->get("bind_address");
	try {
		bind_addr.Resolve(bind_str.c_str());

		if (!bind_addr.isIPv6()) {
			address = bind_addr;
		}
	} catch (ResolveError &e) {
	}

	UDPSocket socket(false);
	socket.Bind(address);

	const char sendbuffer[] = "hello world!";
	/*
	 * If there is a bind address, use it.
	 * It's useful in container environments
	 */
	if (address != Address(0, 0, 0, 0, port))
		socket.Send(address, sendbuffer, sizeof(sendbuffer));
	else
		socket.Send(Address(127, 0, 0, 1, port), sendbuffer, sizeof(sendbuffer));

	sleep_ms(50);

	char rcvbuffer[256] = { 0 };
	Address sender;
	for (;;) {
		if (socket.Receive(sender, rcvbuffer, sizeof(rcvbuffer)) < 0)
			break;
	}
	//FIXME: This fails on some systems
	UASSERT(strncmp(sendbuffer, rcvbuffer, sizeof(sendbuffer)) == 0);

	if (address != Address(0, 0, 0, 0, port)) {
		UASSERT(sender.getAddress().s_addr ==
				address.getAddress().s_addr);
	} else {
		UASSERT(sender.getAddress().s_addr ==
				Address(127, 0, 0, 1, 0).getAddress().s_addr);
	}
}

void TestSocket::testIPv6Socket()
{
	Address address6((IPv6AddressBytes *)NULL, port);
	UDPSocket socket6;

	if (!socket6.init(true, true)) {
		/* Note: Failing to create an IPv6 socket is not technically an
		   error because the OS may not support IPv6 or it may
		   have been disabled. IPv6 is not /required/ by
		   minetest and therefore this should not cause the unit
		   test to fail
		*/
		dstream << "WARNING: IPv6 socket creation failed (unit test)"
			<< std::endl;
		return;
	}

	const char sendbuffer[] = "hello world!";
	IPv6AddressBytes bytes;
	bytes.bytes[15] = 1;

	socket6.Bind(address6);

	{
		socket6.Send(Address(&bytes, port), sendbuffer, sizeof(sendbuffer));

		sleep_ms(50);

		char rcvbuffer[256] = { 0 };
		Address sender;

		for(;;) {
			if (socket6.Receive(sender, rcvbuffer, sizeof(rcvbuffer)) < 0)
				break;
		}
		//FIXME: This fails on some systems
		UASSERT(strncmp(sendbuffer, rcvbuffer, sizeof(sendbuffer)) == 0);

		UASSERT(memcmp(sender.getAddress6().s6_addr,
				Address(&bytes, 0).getAddress6().s6_addr, 16) == 0);
	}
}
