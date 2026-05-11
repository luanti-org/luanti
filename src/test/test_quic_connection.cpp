// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"

#include "network/quic_connection.h"
#include "network/quic_tls.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "network/peerhandler.h"
#include "constants.h"
#include "filesys.h"
#include "porting.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

namespace {

constexpr u16 test_server_port = 30002;

struct PeerCountHandler : public con::PeerHandler {
	std::atomic<int> added{0};
	std::atomic<int> deleted{0};
	std::atomic<session_t> last_added_id{0};

	void peerAdded(con::IPeer *peer) override
	{
		last_added_id = peer->id;
		++added;
	}
	void deletingPeer(con::IPeer *, bool) override
	{
		++deleted;
	}
};

struct ServerCert {
	std::string dir;
	std::string cert;
	std::string key;

	ServerCert()
	{
		dir = fs::TempPath() + DIR_DELIM "luanti-quic-test";
		fs::CreateAllDirs(dir);
		cert = dir + DIR_DELIM "server.cert.pem";
		key = dir + DIR_DELIM "server.key.pem";
		con::generate_self_signed_cert(cert, key, "localhost");
	}
	~ServerCert()
	{
		std::remove(cert.c_str());
		std::remove(key.c_str());
		fs::RecursiveDelete(dir);
	}
};

} // namespace

TEST_CASE("QUIC client and server connect and send packets", "[quic][connection]")
{
	ServerCert ck;
	REQUIRE(fs::IsFile(ck.cert));
	REQUIRE(fs::IsFile(ck.key));

	const std::string pin_b64 = con::compute_pem_fingerprint_b64(ck.cert);
	REQUIRE_FALSE(pin_b64.empty());

	PeerCountHandler server_h;
	std::unique_ptr<con::IConnection> server(con::createQUIC(30.0f, false, &server_h));

	auto *quic_server = dynamic_cast<con::QuicConnection *>(server.get());
	REQUIRE(quic_server != nullptr);
	quic_server->setServerCertificates(ck.cert, ck.key);

	Address bind_addr(127, 0, 0, 1, test_server_port);
	try {
		server->Serve(bind_addr);
	} catch (const con::ConnectionBindFailed &e) {
		SKIP(std::string("could not bind loopback test port: ") + e.what());
	}

	PeerCountHandler client_h;
	std::unique_ptr<con::IConnection> client(con::createQUIC(30.0f, false, &client_h));

	// Build the address with our base64 certificate digest suffix
	Address remote;
	remote.setPort(test_server_port);
	remote.Resolve(("127.0.0.1+" + pin_b64).c_str());
	client->Connect(remote);

	// Let them keep receiving packets until the handshake is complete
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (std::chrono::steady_clock::now() < deadline) {
		NetworkPacket sp, cp;
		server->ReceiveTimeoutMs(&sp, 20);
		client->ReceiveTimeoutMs(&cp, 20);
		if (client->Connected() && server_h.added.load() > 0)
			break;
	}

	REQUIRE(client->Connected());
	REQUIRE(client_h.added.load() == 1);
	REQUIRE(server_h.added.load() == 1);
	const session_t client_pid_on_server = server_h.last_added_id.load();
	REQUIRE(client_pid_on_server != 0);

	// Send a packet from the client to the server
	{
		NetworkPacket pkt(0x4b, 0);
		pkt.putRawString("Hello, server", 13);
		client->Send(PEER_ID_SERVER, 0, &pkt, true);
	}

	NetworkPacket packet_from_client;
	const bool got_client_packet = server->ReceiveTimeoutMs(&packet_from_client, 3000);
	REQUIRE(got_client_packet);
	CHECK(packet_from_client.getCommand() == 0x4b);
	CHECK(packet_from_client.getPeerId() == client_pid_on_server);
	CHECK(std::string(packet_from_client.getRemainingString(), 13) == "Hello, server");

	// Let the client and server send some other packets
	const auto deadline2 =
		std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
	while (std::chrono::steady_clock::now() < deadline2) {
		NetworkPacket sp, cp;
		server->ReceiveTimeoutMs(&sp, 20);
		client->ReceiveTimeoutMs(&cp, 20);
	}

	// Send a packet back to the client
	{
		NetworkPacket pkt(0x01, 14);
		pkt.putRawString("Hello, client", 14);
		server->Send(client_pid_on_server, 0, &pkt, true);
	}

	NetworkPacket packet_from_server;
	const bool got_server_packet = client->ReceiveTimeoutMs(&packet_from_server, 3000);
	REQUIRE(got_server_packet);
	CHECK(packet_from_server.getCommand() == 0x01);
	CHECK(packet_from_server.getPeerId() == PEER_ID_SERVER);
	CHECK(packet_from_server.getSize() == 14);
	CHECK(packet_from_server.getString(0) == std::string("Hello, client"));

	client->Disconnect();
	server->Disconnect();
}

TEST_CASE("QUIC client refuses raw IP without a cert pin", "[quic][connection]")
{
	PeerCountHandler client_h;
	std::unique_ptr<con::IConnection> client(con::createQUIC(30.0f, false, &client_h));

	Address remote;
	remote.setPort(30000);
	// No pinned certificate digest here
	remote.Resolve("127.0.0.1");

	CHECK_THROWS_AS(client->Connect(remote), con::ConnectionException);
	CHECK(client_h.added.load() == 0);
}

TEST_CASE("QUIC client rejects a wrong cert fingerprint", "[quic][connection]")
{
	ServerCert ck;
	REQUIRE(fs::IsFile(ck.cert));

	PeerCountHandler server_h, client_h;
	std::unique_ptr<con::IConnection> server(con::createQUIC(30.0f, false, &server_h));
	auto *quic_server = dynamic_cast<con::QuicConnection *>(server.get());
	REQUIRE(quic_server);
	quic_server->setServerCertificates(ck.cert, ck.key);

	Address bind_addr(127, 0, 0, 1, test_server_port + 1);
	try {
		server->Serve(bind_addr);
	} catch (const con::ConnectionBindFailed &e) {
		SKIP(std::string("Could not bind loopback test port: ") + e.what());
	}

	std::unique_ptr<con::IConnection> client(con::createQUIC(30.0f, false, &client_h));

	const std::string bogus_pinned_cert = "QUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUE=";
	Address remote;
	remote.setPort(test_server_port + 1);
	remote.Resolve(("127.0.0.1+" + bogus_pinned_cert).c_str());
	client->Connect(remote);

	// Confirm the client never reaches the Connected() state
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < deadline) {
		NetworkPacket pkt;
		server->ReceiveTimeoutMs(&pkt, 20);
		client->ReceiveTimeoutMs(&pkt, 20);
		if (client->Connected())
			break;
	}
	CHECK_FALSE(client->Connected());

	client->Disconnect();
	server->Disconnect();
}
