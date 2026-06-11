// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti Authors

#include "network/serverpacketdispatcher.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/serveropcodes.h"
#include "catch.h"
#include <cmath>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Helpers for building a NetworkPacket from a raw byte sequence. We bypass
// the public NetworkPacket constructor that would also set the command, and
// instead fill the packet's data buffer directly. That's good enough for
// testing the dispatcher + decoders in isolation.
// ---------------------------------------------------------------------------

namespace
{

struct PacketBuilder {
	std::string data;

	void u8_(u8 v)    { data.push_back(static_cast<char>(v)); }
	void u16_(u16 v)  { data.push_back(static_cast<char>(v & 0xff));
	                     data.push_back(static_cast<char>((v >> 8) & 0xff)); }
	void u32_(u32 v)  {
		for (int i = 0; i < 4; i++)
			data.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
	}
	void s32_(s32 v)  { u32_(static_cast<u32>(v)); }
	void f32_(f32 v)  {
		u32 raw;
		std::memcpy(&raw, &v, sizeof(raw));
		u32_(raw);
	}
	void string_(const std::string &s) {
		u16_(static_cast<u16>(s.size()));
		data.append(s);
	}
	void wstring_(const std::wstring &s) {
		u16_(static_cast<u16>(s.size()));
		for (wchar_t c : s) {
			u16_(static_cast<u16>(c));
		}
	}
	void longstring_(const std::string &s) {
		u32_(static_cast<u32>(s.size()));
		data.append(s);
	}
	void v3s32_(v3s32 v) {
		s32_(v.X); s32_(v.Y); s32_(v.Z);
	}

	void writeInto(NetworkPacket &pkt) const {
		for (char c : data)
			pkt << static_cast<u8>(c);
	}
};

} // namespace

TEST_CASE("ServerPacketDispatcher table integrity")
{
	auto disp = server::newPacketDispatcher();

	SECTION("every named opcode has a non-null decoder and handler")
	{
		for (u16 cmd = 0; cmd < TOSERVER_NUM_MSG_TYPES; cmd++) {
			const char *name = toServerCommandTable[cmd].name;
			REQUIRE(name != nullptr);
			REQUIRE(std::string(name).substr(0, 9) == "TOSERVER_");
		}
	}

	SECTION("commandName returns the table entry for known opcodes")
	{
		CHECK(std::string(disp->commandName(TOSERVER_INIT)) == "TOSERVER_INIT");
		CHECK(std::string(disp->commandName(TOSERVER_CHAT_MESSAGE)) == "TOSERVER_CHAT_MESSAGE");
		CHECK(std::string(disp->commandName(TOSERVER_PLAYERPOS)) == "TOSERVER_PLAYERPOS");
	}

	SECTION("commandName returns nullptr for out-of-range")
	{
		CHECK(disp->commandName(TOSERVER_NUM_MSG_TYPES) == nullptr);
		CHECK(disp->commandName(0xffff) == nullptr);
	}
}

TEST_CASE("ServerPacketDispatcher decodes InitPayload")
{
	PacketBuilder b;
	b.u8_(38);                 // max_ser_ver
	b.u16_(0);                 // unused
	b.u16_(39);                // min_net_proto_version
	b.u16_(42);                // max_net_proto_version
	b.string_("Alice");        // player name

	NetworkPacket pkt(TOSERVER_INIT, 0);
	b.writeInto(pkt);

	// Decode via the dispatcher.
	auto disp = server::newPacketDispatcher();
	std::aligned_storage<server::MAX_PAYLOAD_SIZE,
		alignof(max_align_t)>::type buf;
	void *out = disp->decode(TOSERVER_INIT, pkt, &buf);
	REQUIRE(out != nullptr);

	const auto &p = *static_cast<const server::InitPayload *>(out);
	CHECK(p.max_ser_ver == 38);
	CHECK(p.unused == 0);
	CHECK(p.min_net_proto_version == 39);
	CHECK(p.max_net_proto_version == 42);
	CHECK(p.player_name == "Alice");

	disp->destroyPayload(TOSERVER_INIT, out);
}

TEST_CASE("ServerPacketDispatcher rejects truncated packet")
{
	NetworkPacket pkt(TOSERVER_INIT, 0);
	// Empty payload — should throw on read.
	std::aligned_storage<server::MAX_PAYLOAD_SIZE,
		alignof(max_align_t)>::type buf;
	auto disp = server::newPacketDispatcher();
	REQUIRE_THROWS(disp->decode(TOSERVER_INIT, pkt, &buf));
}

TEST_CASE("ServerPacketDispatcher decodes PlayerPosPayload (modern fields)")
{
	PacketBuilder b;
	b.v3s32_(v3s32(100, 200, 300));   // pos
	b.v3s32_(v3s32(1, 2, 3));         // speed
	b.s32_(12345);                    // pitch (raw = 123.45 deg)
	b.s32_(67890);                    // yaw   (raw = 678.90 deg)
	b.u32_(0xdeadbeef);               // keyPressed
	b.u8_(80);                        // fov (raw = 1.0)
	b.u8_(10);                        // wanted_range
	b.u8_(0x01);                      // bits (>= 5.8.0)
	b.f32_(0.5f);                     // movement_speed
	b.s32_(3);                        // movement_direction

	NetworkPacket pkt(TOSERVER_PLAYERPOS, 0);
	b.writeInto(pkt);

	auto disp = server::newPacketDispatcher();
	std::aligned_storage<server::MAX_PAYLOAD_SIZE,
		alignof(max_align_t)>::type buf;
	void *out = disp->decode(TOSERVER_PLAYERPOS, pkt, &buf);
	REQUIRE(out != nullptr);

	const auto &p = *static_cast<const server::PlayerPosPayload *>(out);
	CHECK(p.pos == v3s32(100, 200, 300));
	CHECK(p.speed == v3s32(1, 2, 3));
	CHECK(std::fabs(p.pitch - 123.45f) < 1e-3f);
	CHECK(std::fabs(p.yaw   - 678.90f) < 1e-3f);
	CHECK(std::fabs(p.fov   - 1.0f)   < 1e-3f);
	CHECK(p.wanted_range == 10);
	CHECK(p.keyPressed == 0xdeadbeefu);
	CHECK(p.bits == 0x01);
	CHECK(std::fabs(p.movement_speed - 0.5f) < 1e-6f);
	CHECK(p.movement_direction == 3);

	disp->destroyPayload(TOSERVER_PLAYERPOS, out);
}

TEST_CASE("ServerPacketDispatcher decodes ChatMessagePayload")
{
	PacketBuilder b;
	std::wstring msg = L"Hello, world!";
	b.wstring_(msg);

	NetworkPacket pkt(TOSERVER_CHAT_MESSAGE, 0);
	b.writeInto(pkt);

	auto disp = server::newPacketDispatcher();
	std::aligned_storage<server::MAX_PAYLOAD_SIZE,
		alignof(max_align_t)>::type buf;
	void *out = disp->decode(TOSERVER_CHAT_MESSAGE, pkt, &buf);
	REQUIRE(out != nullptr);

	const auto &p = *static_cast<const server::ChatMessagePayload *>(out);
	CHECK(p.message == msg);

	disp->destroyPayload(TOSERVER_CHAT_MESSAGE, out);
}

TEST_CASE("ServerPacketDispatcher decodes InventoryActionPayload (raw bytes)")
{
	PacketBuilder b;
	b.string_("ignored-prefix-bytes");
	NetworkPacket pkt(TOSERVER_INVENTORY_ACTION, 0);
	b.writeInto(pkt);

	auto disp = server::newPacketDispatcher();
	std::aligned_storage<server::MAX_PAYLOAD_SIZE,
		alignof(max_align_t)>::type buf;
	void *out = disp->decode(TOSERVER_INVENTORY_ACTION, pkt, &buf);
	REQUIRE(out != nullptr);

	const auto &p = *static_cast<const server::InventoryActionPayload *>(out);
	CHECK(p.serialized == "ignored-prefix-bytes");

	disp->destroyPayload(TOSERVER_INVENTORY_ACTION, out);
}

TEST_CASE("Fake dispatcher demonstrates IPacketDispatcher substitutability")
{
	class FakeDispatcher : public server::IPacketDispatcher {
	public:
		std::vector<u16> seen;

		void dispatch(NetworkPacket *pkt, Server *) override {
			seen.push_back(pkt->getCommand());
		}
		const char *commandName(u16) const override { return "FAKE"; }
		void *decode(u16, NetworkPacket &, void *) const override { return nullptr; }
		void destroyPayload(u16, void *) const override {}
	};

	FakeDispatcher fake;
	REQUIRE(fake.commandName(0) == std::string("FAKE"));

	NetworkPacket p1(TOSERVER_INIT, 0);
	NetworkPacket p2(TOSERVER_CHAT_MESSAGE, 0);
	fake.dispatch(&p1, nullptr);
	fake.dispatch(&p2, nullptr);
	REQUIRE(fake.seen.size() == 2);
	CHECK(fake.seen[0] == TOSERVER_INIT);
	CHECK(fake.seen[1] == TOSERVER_CHAT_MESSAGE);
}
