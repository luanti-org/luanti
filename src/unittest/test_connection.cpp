// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include "log.h"
#include "porting.h"
#include "settings.h"
#include "util/serialize.h"
#include "network/peerhandler.h"
#include "network/mtp/internal.h"
#include "network/networkpacket.h"
#include "network/socket.h"

class TestConnection : public TestBase {
public:
	TestConnection()
	{
		if (INTERNET_SIMULATOR == false)
			TestManager::registerTestModule(this);
	}

	const char *getName() { return "TestConnection"; }

	void runTests(IGameDef *gamedef);

	void testNetworkPacketSerialize();
	void testHelpers();
	void testConnectSendReceive();
};

static TestConnection g_test_instance;

void TestConnection::runTests(IGameDef *gamedef)
{
	TEST(testNetworkPacketSerialize);
	TEST(testHelpers);
	TEST(testConnectSendReceive);
}

////////////////////////////////////////////////////////////////////////////////

struct Handler : public con::PeerHandler
{
	Handler(const char *a_name) : name(a_name) {}

	void peerAdded(con::IPeer *peer)
	{
		infostream << "Handler(" << name << ")::peerAdded(): "
			"id=" << peer->id << std::endl;
		last_id = peer->id;
		count++;
	}

	void deletingPeer(con::IPeer *peer, bool timeout)
	{
		infostream << "Handler(" << name << ")::deletingPeer(): "
			"id=" << peer->id << ", timeout=" << timeout << std::endl;
		last_id = peer->id;
		count--;
	}

	s32 count = 0;
	u16 last_id = 0;
	const char *name;
};

void TestConnection::testNetworkPacketSerialize()
{
	const static u8 expected[] = {
		0x00, 0x7b,
		0x00, 0x02, 0xd8, 0x42, 0xdf, 0x9a
	};

	if (sizeof(wchar_t) == 2)
		warningstream << __FUNCTION__ << " may fail on this platform." << std::endl;

	{
		NetworkPacket pkt(123, 0);

		// serializing wide strings should do surrogate encoding, we test that here
		pkt << std::wstring(L"\U00020b9a");

		auto buf = pkt.oldForgePacket();
		UASSERTEQ(int, buf.getSize(), sizeof(expected));
		UASSERT(!memcmp(expected, &buf[0], buf.getSize()));
	}

	{
		NetworkPacket pkt;
		pkt.putRawPacket(expected, sizeof(expected), 0);

		// same for decoding
		std::wstring pkt_s;
		pkt >> pkt_s;

		UASSERT(pkt_s == L"\U00020b9a");
	}
}

void TestConnection::testHelpers()
{
	// Some constants for testing
	u32 proto_id = 0x12345678;
	session_t peer_id = 123;
	u8 channel = 2;
	SharedBuffer<u8> data1(1);
	data1[0] = 100;
	Address a(127,0,0,1, 10);
	const u16 seqnum = 34352;

	con::BufferedPacketPtr p1 = con::makePacket(a, data1,
			proto_id, peer_id, channel);
	/*
		We should now have a packet with this data:
		Header:
			[0] u32 protocol_id
			[4] session_t sender_peer_id
			[6] u8 channel
		Data:
			[7] u8 data1[0]
	*/
	UASSERT(readU32(&p1->data[0]) == proto_id);
	UASSERT(readU16(&p1->data[4]) == peer_id);
	UASSERT(readU8(&p1->data[6]) == channel);
	UASSERT(readU8(&p1->data[7]) == data1[0]);

	//infostream<<"initial data1[0]="<<((u32)data1[0]&0xff)<<std::endl;

	SharedBuffer<u8> p2 = con::makeReliablePacket(data1, seqnum);

	/*infostream<<"p2.getSize()="<<p2.getSize()<<", data1.getSize()="
			<<data1.getSize()<<std::endl;
	infostream<<"readU8(&p2[3])="<<readU8(&p2[3])
			<<" p2[3]="<<((u32)p2[3]&0xff)<<std::endl;
	infostream<<"data1[0]="<<((u32)data1[0]&0xff)<<std::endl;*/

	UASSERT(p2.getSize() == 3 + data1.getSize());
	UASSERT(readU8(&p2[0]) == con::PACKET_TYPE_RELIABLE);
	UASSERT(readU16(&p2[1]) == seqnum);
	UASSERT(readU8(&p2[3]) == data1[0]);
}


void TestConnection::testConnectSendReceive()
{

	constexpr u32 timeout_ms = 100;

	/*
		Test some real connections

		NOTE: This mostly tests the legacy interface.
	*/

	Handler hand_server("server");
	Handler hand_client("client");

	Address address(0, 0, 0, 0, 30001);
	Address bind_addr(0, 0, 0, 0, 30001);
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

	infostream << "** Creating server Connection" << std::endl;
	con::Connection server(512, 5.0f, false, &hand_server);
	server.Serve(address);

	infostream << "** Creating client Connection" << std::endl;
	con::Connection client(512, 5.0f, false, &hand_client);

	UASSERT(hand_server.count == 0);
	UASSERT(hand_client.count == 0);

	sleep_ms(50);

	Address server_address(127, 0, 0, 1, 30001);
	if (address != Address(0, 0, 0, 0, 30001)) {
		server_address = bind_addr;
	}

	infostream << "** running client.Connect()" << std::endl;
	client.Connect(server_address);

	sleep_ms(50);

	// Client should not have added client yet
	UASSERT(hand_client.count == 0);

	NetworkPacket pkt;
	infostream << "** running client.Receive()" << std::endl;
	if (client.ReceiveTimeoutMs(&pkt, timeout_ms)) {
		infostream << "** Client received: peer_id=" << pkt.getPeerId()
			<< ", size=" << pkt.getSize() << std::endl;
	}

	// Client should have added server now
	UASSERT(hand_client.count == 1);
	UASSERT(hand_client.last_id == 1);
	// Server should not have added client yet
	UASSERT(hand_server.count == 0);

	sleep_ms(100);

	NetworkPacket pkt1;
	infostream << "** running server.Receive()" << std::endl;
	if (server.ReceiveTimeoutMs(&pkt, timeout_ms)) {
		infostream << "** Server received: peer_id=" << pkt.getPeerId()
			<< ", size=" << pkt.getSize()
			<< std::endl;
	}
	else {
		// No actual data received, but the client has
		// probably been connected
	}

	// Client should be the same
	UASSERT(hand_client.count == 1);
	UASSERT(hand_client.last_id == 1);
	// Server should have the client
	UASSERT(hand_server.count == 1);
	UASSERT(hand_server.last_id >= 2);

	//sleep_ms(50);

	while (client.Connected() == false) {
		NetworkPacket pkt;
		infostream << "** running client.Receive()" << std::endl;
		if (client.TryReceive(&pkt)) {
			infostream << "** Client received: peer_id=" << pkt.getPeerId()
				<< ", size=" << pkt.getSize() << std::endl;
		}
		sleep_ms(50);
	}

	sleep_ms(50);

	NetworkPacket pkt2;
	infostream << "** running server.Receive()" << std::endl;
	if (server.ReceiveTimeoutMs(&pkt, timeout_ms)) {
		infostream << "** Server received: peer_id=" << pkt.getPeerId()
			<< ", size=" << pkt.getSize()
			<< std::endl;
	}

	/*
		Simple send-receive test
	*/
	{
		NetworkPacket pkt(0x4b, 0);
		pkt.putRawString("Hello World !", 14);

		auto sentdata = pkt.oldForgePacket();

		infostream<<"** running client.Send()"<<std::endl;
		client.Send(PEER_ID_SERVER, 0, &pkt, true);

		sleep_ms(50);

		NetworkPacket recvpacket;
		infostream << "** running server.Receive()" << std::endl;
		UASSERT(server.ReceiveTimeoutMs(&recvpacket, timeout_ms));
		infostream << "** Server received: peer_id=" << pkt.getPeerId()
				<< ", size=" << pkt.getSize()
				<< ", data=" << pkt.getString(0)
				<< std::endl;

		auto recvdata = pkt.oldForgePacket();

		UASSERT(memcmp(*sentdata, *recvdata, recvdata.getSize()) == 0);
	}

	const session_t peer_id_client = hand_server.last_id;
	/*
		Send a large packet
	*/
	{
		const int datasize = 30000;
		NetworkPacket pkt(0xff, datasize);
		for (u16 i=0; i<datasize; i++) {
			pkt << static_cast<u8>(i/4);
		}

		infostream << "Sending data (size=" << datasize << "):";
		for (int i = 0; i < datasize && i < 20; i++) {
			if (i % 2 == 0)
				infostream << " ";
			char buf[10];
			porting::mt_snprintf(buf, sizeof(buf), "%.2X",
				((int)(pkt.getString(0))[i]) & 0xff);
			infostream<<buf;
		}
		if (datasize > 20)
			infostream << "...";
		infostream << std::endl;

		auto sentdata = pkt.oldForgePacket();

		server.Send(peer_id_client, 0, &pkt, true);

		//sleep_ms(3000);

		Buffer<u8> recvdata;
		infostream << "** running client.Receive()" << std::endl;
		session_t peer_id = 132;
		u16 size = 0;
		bool received = false;
		u64 timems0 = porting::getTimeMs();
		for (;;) {
			if (porting::getTimeMs() - timems0 > 5000 || received)
				break;
			NetworkPacket pkt;
			if (client.ReceiveTimeoutMs(&pkt, timeout_ms)) {
				size = pkt.getSize();
				peer_id = pkt.getPeerId();
				recvdata = pkt.oldForgePacket();
				received = true;
			}
			sleep_ms(10);
		}
		UASSERT(received);
		infostream << "** Client received: peer_id=" << peer_id
			<< ", size=" << size << std::endl;

		infostream << "Received data (size=" << size << "): ";
		for (int i = 0; i < size && i < 20; i++) {
			if (i % 2 == 0)
				infostream << " ";
			char buf[10];
			porting::mt_snprintf(buf, sizeof(buf), "%.2X", ((int)(recvdata[i])) & 0xff);
			infostream << buf;
		}
		if (size > 20)
			infostream << "...";
		infostream << std::endl;

		UASSERT(memcmp(*sentdata, *recvdata, recvdata.getSize()) == 0);
		UASSERT(peer_id == PEER_ID_SERVER);
	}

	// Check peer handlers
	UASSERT(hand_client.count == 1);
	UASSERT(hand_client.last_id == 1);
	UASSERT(hand_server.count == 1);
	UASSERT(hand_server.last_id >= 2);
}
