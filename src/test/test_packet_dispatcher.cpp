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
#include <vector>

// ---------------------------------------------------------------------------
// Helper for building a NetworkPacket from typed fields. We append big-endian
// bytes to an in-memory buffer and then frame the buffer with the command word
// before installing it via putRawPacket, which sets the read offset to 0
// (matching how packets arrive on the wire). putRawPacket asserts that no
// command has been set on the target packet, so the NetworkPacket must be
// default-constructed here.
// ---------------------------------------------------------------------------

namespace
{

struct PacketBuilder {
	std::vector<u8> data;

	void u8_(u8 v)    { data.push_back(v); }
	void u16_(u16 v)  {
		data.push_back((v >> 8) & 0xff);
		data.push_back((v >> 0) & 0xff);
	}
	void u32_(u32 v)  {
		data.push_back((v >> 24) & 0xff);
		data.push_back((v >> 16) & 0xff);
		data.push_back((v >>  8) & 0xff);
		data.push_back((v >>  0) & 0xff);
	}
	void s32_(s32 v)  { u32_(static_cast<u32>(v)); }
	void f32_(f32 v)  {
		u32 raw;
		std::memcpy(&raw, &v, sizeof(raw));
		u32_(raw);
	}
	void string_(const std::string &s) {
		u16_(static_cast<u16>(s.size()));
		data.insert(data.end(), s.begin(), s.end());
	}
	void wstring_(const std::wstring &s) {
		u16_(static_cast<u16>(s.size()));
		for (wchar_t c : s) {
			u16_(static_cast<u16>(c));
		}
	}
	void longstring_(const std::string &s) {
		u32_(static_cast<u32>(s.size()));
		data.insert(data.end(), s.begin(), s.end());
	}
	void v3s32_(v3s32 v) {
		s32_(v.X); s32_(v.Y); s32_(v.Z);
	}

	void writeInto(NetworkPacket &pkt, u16 cmd) const {
		std::vector<u8> framed;
		framed.reserve(2 + data.size());
		framed.push_back((cmd >> 8) & 0xff);
		framed.push_back((cmd >> 0) & 0xff);
		framed.insert(framed.end(), data.begin(), data.end());
		pkt.putRawPacket(framed.data(), framed.size(), 0);
	}
};

} // namespace

TEST_CASE("ServerPacketDispatcher table integrity")
{
	auto disp = server::newPacketDispatcher(nullptr);

	SECTION("every named opcode has a TOSERVER_-prefixed name")
	{
		for (u16 cmd = 0; cmd < TOSERVER_NUM_MSG_TYPES; cmd++) {
			const char *name = toServerCommandTable[cmd].name;
			if (name == nullptr)
				continue; // reserved / unused opcode
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

	NetworkPacket pkt;
	b.writeInto(pkt, TOSERVER_INIT);

	// Decode via the dispatcher.
	auto disp = server::newPacketDispatcher(nullptr);
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
	auto disp = server::newPacketDispatcher(nullptr);
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

	NetworkPacket pkt;
	b.writeInto(pkt, TOSERVER_PLAYERPOS);

	auto disp = server::newPacketDispatcher(nullptr);
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

	NetworkPacket pkt;
	b.writeInto(pkt, TOSERVER_CHAT_MESSAGE);

	auto disp = server::newPacketDispatcher(nullptr);
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
	const std::string blob = "Move 99 player:foo list:inv slot:0 player:bar list:inv slot:1";
	for (char c : blob)
		b.u8_(static_cast<u8>(c));
	NetworkPacket pkt;
	b.writeInto(pkt, TOSERVER_INVENTORY_ACTION);

	auto disp = server::newPacketDispatcher(nullptr);
	std::aligned_storage<server::MAX_PAYLOAD_SIZE,
		alignof(max_align_t)>::type buf;
	void *out = disp->decode(TOSERVER_INVENTORY_ACTION, pkt, &buf);
	REQUIRE(out != nullptr);

	const auto &p = *static_cast<const server::InventoryActionPayload *>(out);
	CHECK(p.serialized == blob);

	disp->destroyPayload(TOSERVER_INVENTORY_ACTION, out);
}

TEST_CASE("Fake dispatcher demonstrates IPacketDispatcher substitutability")
{
	class FakeDispatcher : public server::IPacketDispatcher {
	public:
		std::vector<u16> seen;

		void processIncomingPackets(con::IConnection *, Server *, float) override {}
		const char *commandName(u16) const override { return "FAKE"; }
		void *decode(u16, NetworkPacket &, void *) const override { return nullptr; }
		void destroyPayload(u16, void *) const override {}
	};

	FakeDispatcher fake;
	REQUIRE(fake.commandName(0) == std::string("FAKE"));

	// Smoke-check that pump can be called through the interface pointer.
	con::IConnection *con = nullptr;
	fake.processIncomingPackets(con, nullptr, 0.0f);
	CHECK(fake.seen.empty());
}
