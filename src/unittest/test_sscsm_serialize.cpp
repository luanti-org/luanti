// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti authors

#include "test.h"

#include "exceptions.h"
#include "log.h"
#include "script/sscsm/sscsm_events.h"
#include "script/sscsm/sscsm_irequest.h"
#include "script/sscsm/sscsm_requests.h"
#include "script/sscsm/sscsm_serialize.h"

class TestSSCSMSerialize : public TestBase
{
public:
	TestSSCSMSerialize() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSSCSMSerialize"; }

	void runTests(IGameDef *gamedef);

	void testRequestPollNextEvent();
	void testRequestSetFatalError();
	void testRequestPrint();
	void testRequestLog();
	void testRequestGetNode();
	void testGetNodeAnswer();

	void testEventTearDown();
	void testEventOnStep();
	void testEventUpdateVFSFiles();
	void testEventLoadMods();

	void testPollNextEventAnswer();
	void testEmptyVectors();
	void testLargePayload();
	void testEmptyStrings();

	void testUnknownRequestTag();
	void testUnknownEventTag();
};

static TestSSCSMSerialize g_test_instance;

void TestSSCSMSerialize::runTests(IGameDef *gamedef)
{
	TEST(testRequestPollNextEvent);
	TEST(testRequestSetFatalError);
	TEST(testRequestPrint);
	TEST(testRequestLog);
	TEST(testRequestGetNode);
	TEST(testGetNodeAnswer);

	TEST(testEventTearDown);
	TEST(testEventOnStep);
	TEST(testEventUpdateVFSFiles);
	TEST(testEventLoadMods);

	TEST(testPollNextEventAnswer);
	TEST(testEmptyVectors);
	TEST(testLargePayload);
	TEST(testEmptyStrings);

	TEST(testUnknownRequestTag);
	TEST(testUnknownEventTag);
}

////////////////////////////////////////////////////////////////////////////////
// Helpers

template <typename T>
static std::unique_ptr<T> roundtripRequest(const T &orig)
{
	auto bytes = serializeSSCSMRequest(orig);
	auto poly = deserializeSSCSMRequest(bytes);
	UASSERTEQ(int, static_cast<int>(poly->getType()),
			static_cast<int>(T::TYPE));
	auto *casted = dynamic_cast<T *>(poly.get());
	UASSERT(casted != nullptr);
	(void)poly.release();
	return std::unique_ptr<T>(casted);
}

template <typename T>
static T roundtripAnswer(const T &orig)
{
	auto bytes = serializeSSCSMAnswer(T(orig));
	return deserializeSSCSMAnswer<T>(bytes);
}

////////////////////////////////////////////////////////////////////////////////
// Request round-trip tests

void TestSSCSMSerialize::testRequestPollNextEvent()
{
	SSCSMRequestPollNextEvent orig;
	auto bytes = serializeSSCSMRequest(orig);
	// One byte: just the type tag
	UASSERTEQ(size_t, bytes.size(), 1);
	auto poly = deserializeSSCSMRequest(bytes);
	UASSERT(poly->getType() == SSCSMRequestType::PollNextEvent);
}

void TestSSCSMSerialize::testRequestSetFatalError()
{
	SSCSMRequestSetFatalError orig;
	orig.reason = "something broke: \x01\x00\x02 with embedded nulls";
	auto round = roundtripRequest(orig);
	UASSERTEQ(std::string, round->reason, orig.reason);
}

void TestSSCSMSerialize::testRequestPrint()
{
	SSCSMRequestPrint orig;
	orig.text = "hello world";
	auto round = roundtripRequest(orig);
	UASSERTEQ(std::string, round->text, orig.text);
}

void TestSSCSMSerialize::testRequestLog()
{
	SSCSMRequestLog orig;
	orig.level = LL_WARNING;
	orig.text = "warning message";
	auto round = roundtripRequest(orig);
	UASSERTEQ(int, static_cast<int>(round->level),
			static_cast<int>(orig.level));
	UASSERTEQ(std::string, round->text, orig.text);
}

void TestSSCSMSerialize::testRequestGetNode()
{
	SSCSMRequestGetNode orig;
	orig.pos = v3s16(-1234, 0, 5678);
	auto round = roundtripRequest(orig);
	UASSERT(round->pos == orig.pos);
}

void TestSSCSMSerialize::testGetNodeAnswer()
{
	SSCSMRequestGetNode::Answer orig;
	orig.node.param0 = 0xCAFE;
	orig.node.param1 = 0x12;
	orig.node.param2 = 0x34;
	orig.is_pos_ok = true;
	auto round = roundtripAnswer(orig);
	UASSERTEQ(u16, round.node.param0, orig.node.param0);
	UASSERTEQ(u8, round.node.param1, orig.node.param1);
	UASSERTEQ(u8, round.node.param2, orig.node.param2);
	UASSERT(round.is_pos_ok == orig.is_pos_ok);

	// Try the false case too
	orig.is_pos_ok = false;
	round = roundtripAnswer(orig);
	UASSERT(round.is_pos_ok == false);
}

////////////////////////////////////////////////////////////////////////////////
// Event round-trip tests
//
// Events are not serialized standalone in the current channel; they're
// embedded inside SSCSMRequestPollNextEvent::Answer. We exercise them via
// that path so the polymorphic dispatch is covered too.

template <typename T>
static std::unique_ptr<T> roundtripEvent(std::unique_ptr<T> orig)
{
	SSCSMRequestPollNextEvent::Answer ans;
	ans.next_event = std::move(orig);
	auto bytes = serializeSSCSMAnswer(std::move(ans));
	auto received = deserializeSSCSMAnswer<SSCSMRequestPollNextEvent::Answer>(bytes);
	UASSERT(received.next_event != nullptr);
	auto *casted = dynamic_cast<T *>(received.next_event.get());
	UASSERT(casted != nullptr);
	(void)received.next_event.release();
	return std::unique_ptr<T>(casted);
}

void TestSSCSMSerialize::testEventTearDown()
{
	auto round = roundtripEvent(std::make_unique<SSCSMEventTearDown>());
	UASSERT(round->getType() == SSCSMEventType::TearDown);
}

void TestSSCSMSerialize::testEventOnStep()
{
	auto orig = std::make_unique<SSCSMEventOnStep>();
	orig->dtime = 0.0625f;
	auto round = roundtripEvent(std::move(orig));
	UASSERTEQ(float, round->dtime, 0.0625f);
}

void TestSSCSMSerialize::testEventUpdateVFSFiles()
{
	auto orig = std::make_unique<SSCSMEventUpdateVFSFiles>();
	orig->files.emplace_back("modA:init.lua", "core.log('hi')");
	orig->files.emplace_back("modA:helpers.lua", std::string("\x00\x01\x02", 3));
	orig->files.emplace_back("modB:init.lua", "");
	auto round = roundtripEvent(std::move(orig));
	UASSERTEQ(size_t, round->files.size(), 3);
	UASSERTEQ(std::string, round->files[0].first, "modA:init.lua");
	UASSERTEQ(std::string, round->files[0].second, "core.log('hi')");
	UASSERTEQ(std::string, round->files[1].first, "modA:helpers.lua");
	UASSERTEQ(size_t, round->files[1].second.size(), 3);
	UASSERTEQ(int, (int)round->files[1].second[0], 0);
	UASSERTEQ(int, (int)round->files[1].second[1], 1);
	UASSERTEQ(int, (int)round->files[1].second[2], 2);
	UASSERTEQ(std::string, round->files[2].first, "modB:init.lua");
	UASSERTEQ(std::string, round->files[2].second, "");
}

void TestSSCSMSerialize::testEventLoadMods()
{
	auto orig = std::make_unique<SSCSMEventLoadMods>();
	orig->mods.emplace_back("modA", "modA:init.lua");
	orig->mods.emplace_back("modB", "modB:init.lua");
	auto round = roundtripEvent(std::move(orig));
	UASSERTEQ(size_t, round->mods.size(), 2);
	UASSERTEQ(std::string, round->mods[0].first, "modA");
	UASSERTEQ(std::string, round->mods[0].second, "modA:init.lua");
	UASSERTEQ(std::string, round->mods[1].first, "modB");
	UASSERTEQ(std::string, round->mods[1].second, "modB:init.lua");
}

////////////////////////////////////////////////////////////////////////////////
// Embedded event answer

void TestSSCSMSerialize::testPollNextEventAnswer()
{
	// Verify that a non-trivial event survives the answer wrap+unwrap
	auto inner = std::make_unique<SSCSMEventOnStep>();
	inner->dtime = 1.5f;

	SSCSMRequestPollNextEvent::Answer ans;
	ans.next_event = std::move(inner);
	auto bytes = serializeSSCSMAnswer(std::move(ans));

	auto received = deserializeSSCSMAnswer<SSCSMRequestPollNextEvent::Answer>(bytes);
	UASSERT(received.next_event != nullptr);
	UASSERT(received.next_event->getType() == SSCSMEventType::OnStep);
	auto *step = dynamic_cast<SSCSMEventOnStep *>(received.next_event.get());
	UASSERT(step != nullptr);
	UASSERTEQ(float, step->dtime, 1.5f);
}

////////////////////////////////////////////////////////////////////////////////
// Edge cases

void TestSSCSMSerialize::testEmptyVectors()
{
	auto orig = std::make_unique<SSCSMEventUpdateVFSFiles>();
	auto round = roundtripEvent(std::move(orig));
	UASSERTEQ(size_t, round->files.size(), 0);

	auto orig2 = std::make_unique<SSCSMEventLoadMods>();
	auto round2 = roundtripEvent(std::move(orig2));
	UASSERTEQ(size_t, round2->mods.size(), 0);
}

void TestSSCSMSerialize::testLargePayload()
{
	auto orig = std::make_unique<SSCSMEventUpdateVFSFiles>();
	std::string big_content(64 * 1024, 'x');
	for (u32 i = 0; i < 100; ++i) {
		orig->files.emplace_back("mod:file" + std::to_string(i) + ".lua",
				big_content);
	}
	auto round = roundtripEvent(std::move(orig));
	UASSERTEQ(size_t, round->files.size(), 100);
	for (u32 i = 0; i < 100; ++i) {
		UASSERTEQ(std::string, round->files[i].first,
				"mod:file" + std::to_string(i) + ".lua");
		UASSERTEQ(size_t, round->files[i].second.size(), 64 * 1024);
		UASSERT(round->files[i].second[0] == 'x');
		UASSERT(round->files[i].second[64 * 1024 - 1] == 'x');
	}
}

void TestSSCSMSerialize::testEmptyStrings()
{
	SSCSMRequestSetFatalError orig;
	orig.reason = "";
	auto round = roundtripRequest(orig);
	UASSERTEQ(std::string, round->reason, "");

	SSCSMRequestPrint orig2;
	orig2.text = "";
	auto round2 = roundtripRequest(orig2);
	UASSERTEQ(std::string, round2->text, "");
}

////////////////////////////////////////////////////////////////////////////////
// Failure modes

void TestSSCSMSerialize::testUnknownRequestTag()
{
	std::string bad;
	bad.push_back(static_cast<char>(0xFF));
	EXCEPTION_CHECK(SerializationError, deserializeSSCSMRequest(bad));
}

void TestSSCSMSerialize::testUnknownEventTag()
{
	// Build a fake PollNextEvent answer whose embedded event has an unknown tag
	std::string bad;
	bad.push_back(static_cast<char>(0xFE));
	EXCEPTION_CHECK(SerializationError,
			deserializeSSCSMAnswer<SSCSMRequestPollNextEvent::Answer>(bad));
}
