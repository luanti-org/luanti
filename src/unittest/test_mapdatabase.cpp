// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 sfan5

#include "cmake_config.h"

#include "test.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include "exceptions.h"
#include "porting.h"
#include "serialization.h"
#include "servermap.h"
#include "database/database-dummy.h"
#include "database/database-sqlite3.h"
#if USE_LEVELDB
#include "database/database-leveldb.h"
#endif
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif

namespace
{

class MapDatabaseProvider
{
public:
	typedef std::function<MapDatabase*(void)> F;

	MapDatabaseProvider(MapDatabase *db) : can_create(false), m_db(db) {
		sanity_check(m_db);
	}
	MapDatabaseProvider(const F &creator) : can_create(true), m_creator(creator) {
		sanity_check(m_creator);
	}

	~MapDatabaseProvider() {
		if (m_db)
			m_db->endSave();
		if (can_create)
			delete m_db;
	}

	MapDatabase *get() {
		if (m_db)
			m_db->endSave();
		if (can_create) {
			delete m_db;
			m_db = m_creator();
		}
		m_db->beginSave();
		return m_db;
	}

private:
	bool can_create;
	F m_creator;
	MapDatabase *m_db = nullptr;
};

}

class TestMapDatabase : public TestBase
{
public:
	TestMapDatabase() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMapDatabase"; }

	void runTests(IGameDef *gamedef);
	void runTestsForCurrentDB();

	void testSave();
	void testLoad();
	void testList(int expect);
	void testRemove();
	void testPositionEncoding();

private:
	MapDatabaseProvider *provider = nullptr;

	std::string test_data;
};

static TestMapDatabase g_test_instance;

void TestMapDatabase::runTests(IGameDef *gamedef)
{
	// fixed directory, for persistence
	const std::string test_dir = getTestTempDirectory();

	for (int c = 255; c >= 0; c--)
		test_data.push_back(static_cast<char>(c));
	sanity_check(!test_data.empty());

	TEST(testPositionEncoding);

	rawstream << "-------- Dummy" << std::endl;

	// We can't re-create this object since it would lose the data
	{
		auto dummy_db = std::make_unique<Database_Dummy>();
		provider = new MapDatabaseProvider(dummy_db.get());
		runTestsForCurrentDB();
		delete provider;
	}

	rawstream << "-------- SQLite3" << std::endl;

	provider = new MapDatabaseProvider([&] () {
		return new MapDatabaseSQLite3(test_dir);
	});
	runTestsForCurrentDB();
	delete provider;

#if USE_LEVELDB
	rawstream << "-------- LevelDB" << std::endl;

	provider = new MapDatabaseProvider([&] () {
		return new Database_LevelDB(test_dir);
	});
	runTestsForCurrentDB();
	delete provider;
#endif

#if USE_POSTGRESQL
	const char *connstr = getenv("MINETEST_POSTGRESQL_CONNECT_STRING");
	if (connstr) {
		rawstream << "-------- PostgreSQL" << std::endl;

		provider = new MapDatabaseProvider([&] () {
			return new MapDatabasePostgreSQL(connstr);
		});
		runTestsForCurrentDB();
		delete provider;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////

void TestMapDatabase::runTestsForCurrentDB()
{
	// order-sensitive
	TEST(testSave);
	TEST(testLoad);
	TEST(testList, 1);
	TEST(testRemove);
	TEST(testList, 0);
}

void TestMapDatabase::testSave()
{
	auto *db = provider->get();
	std::string_view stuff{"wrong wrong wrong"};
	UASSERT(db->saveBlock({1, 2, 3}, stuff));
	// overwriting is valid too
	UASSERT(db->saveBlock({1, 2, 3}, test_data));
}

void TestMapDatabase::testLoad()
{
	auto *db = provider->get();
	std::string dest;

	// successful load
	db->loadBlock({1, 2, 3}, &dest);
	UASSERT(!dest.empty());
	UASSERT(dest == test_data);

	// failed load
	v3s16 pp[] = {{1, 2, 4}, {0, 0, 0}, {-1, -2, -3}};
	for (v3s16 p : pp) {
		dest = "not empty";
		db->loadBlock(p, &dest);
		UASSERT(dest.empty());
	}
}

void TestMapDatabase::testList(int expect)
{
	auto *db = provider->get();
	std::vector<v3s16> dest;
	db->listAllLoadableBlocks(dest);
	UASSERTEQ(size_t, dest.size(), expect);
	if (expect == 1)
		UASSERT(dest.front() == v3s16(1, 2, 3));
}

void TestMapDatabase::testRemove()
{
	auto *db = provider->get();

	// successful remove
	UASSERT(db->deleteBlock({1, 2, 3}));

	// failed remove
	// FIXME: this isn't working consistently, maybe later
	//UASSERT(!db->deleteBlock({1, 2, 4}));
}

void TestMapDatabase::testPositionEncoding()
{
	auto db = std::make_unique<Database_Dummy>();

	// Unit vectors and extremes
	UASSERTEQ(s64, db->getBlockAsInteger({0, 0, 0}), 0)
	UASSERTEQ(s64, db->getBlockAsInteger({1, 0, 0}), 1)
	UASSERTEQ(s64, db->getBlockAsInteger({0, 1, 0}), 0x1000)
	UASSERTEQ(s64, db->getBlockAsInteger({0, 0, 1}), 0x1000000)
	UASSERTEQ(s64, db->getBlockAsInteger({-1, 0, 0}), -1)
	UASSERTEQ(s64, db->getBlockAsInteger({0, -1, 0}), -0x1000)
	UASSERTEQ(s64, db->getBlockAsInteger({0, 0, -1}), -0x1000000)
	UASSERTEQ(s64, db->getBlockAsInteger({2047, 2047, 2047}), 0x7FF7FF7FF)
	UASSERTEQ(s64, db->getBlockAsInteger({-2048, -2048, -2048}), -0x800800800)
	UASSERTEQ(s64, db->getBlockAsInteger({-123, 456, -789}), -0x314e3807b)

	UASSERT(db->getIntegerAsBlock(0) == v3s16(0, 0, 0))
	UASSERT(db->getIntegerAsBlock(1) == v3s16(1, 0, 0))
	UASSERT(db->getIntegerAsBlock(0x1000) == v3s16(0, 1, 0))
	UASSERT(db->getIntegerAsBlock(0x1000000) == v3s16(0, 0, 1))
	UASSERT(db->getIntegerAsBlock(-1) == v3s16(-1, 0, 0))
	UASSERT(db->getIntegerAsBlock(-0x1000) == v3s16(0, -1, 0))
	UASSERT(db->getIntegerAsBlock(-0x1000000) == v3s16(0, 0, -1))
	UASSERT(db->getIntegerAsBlock(0x7FF7FF7FF) == v3s16(2047, 2047, 2047))
	UASSERT(db->getIntegerAsBlock(-0x800800800) == v3s16(-2048, -2048, -2048))
	UASSERT(db->getIntegerAsBlock(-0x314e3807b) == v3s16(-123, 456, -789))
}

////////////////////////////////////////////////////////////////////////////////

class TestMapDatabaseAccessor : public TestBase
{
public:
	TestMapDatabaseAccessor() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMapDatabaseAccessor"; }

	void runTests(IGameDef *gamedef);

	void testLoadPending();
	void testRawBlock();
	void testWritePending();
	void testManyBlocks();
	void testReplaceWhileWriting();
	void testReplacePending();
	void testDeletePending();
	void testListPending();
	void testFailedWrite();
	void testThrowingWrite();
	void testFullQueue();
	void testThread();
	void testFatalError();

private:
	class FailingDatabase : public Database_Dummy
	{
	public:
		bool fail = false;
		bool throw_error = false;

		bool saveBlock(const v3s16 &pos, std::string_view data) override
		{
			if (throw_error)
				throw DatabaseException("test error");
			return !fail && Database_Dummy::saveBlock(pos, data);
		}
	};

	class HookDatabase : public Database_Dummy
	{
	public:
		std::function<void()> on_save;

		bool saveBlock(const v3s16 &pos, std::string_view data) override
		{
			if (on_save)
				on_save();
			return Database_Dummy::saveBlock(pos, data);
		}
	};

	static bool waitUntil(const std::function<bool()> &cond);

	const v3s16 pos{1, 2, 3};
	const v3s16 pos2{4, 5, 6};
	const std::string data_a = std::string(1000, 'a');
	const std::string data_b = std::string(2000, 'b');
};

static TestMapDatabaseAccessor g_test_instance_accessor;

void TestMapDatabaseAccessor::runTests(IGameDef *gamedef)
{
	TEST(testLoadPending);
	TEST(testRawBlock);
	TEST(testWritePending);
	TEST(testManyBlocks);
	TEST(testReplaceWhileWriting);
	TEST(testReplacePending);
	TEST(testDeletePending);
	TEST(testListPending);
	TEST(testFailedWrite);
	TEST(testThrowingWrite);
	TEST(testFullQueue);
	TEST(testThread);
	TEST(testFatalError);
}

bool TestMapDatabaseAccessor::waitUntil(const std::function<bool()> &cond)
{
	for (int i = 0; i < 500 && !cond(); i++)
		sleep_ms(10);
	return cond();
}

void TestMapDatabaseAccessor::testLoadPending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	accessor.queueBlock(pos, std::string(data_a));
	accessor.loadBlock(pos, dest);
	UASSERT(dest == data_a);

	// nothing was written yet
	db.loadBlock(pos, &dest);
	UASSERT(dest.empty());
}

void TestMapDatabaseAccessor::testRawBlock()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	const u8 version = SER_FMT_VER_HIGHEST_WRITE;
	std::string loaded, written;

	auto check = [&] (const std::string &value, const std::string &raw) {
		UASSERT(!value.empty());
		UASSERTEQ(int, (u8)value[0], version);
		std::istringstream is(value.substr(1));
		std::ostringstream os;
		decompress(is, os, version);
		UASSERT(os.str() == raw);
	};

	// loading compresses on demand
	accessor.queueRawBlock(pos, std::string(data_a), version, -1);
	accessor.loadBlock(pos, loaded);
	check(loaded, data_a);

	// a newer raw block wins
	accessor.queueRawBlock(pos, std::string(data_b), version, -1);
	UASSERT(accessor.writePending());
	db.loadBlock(pos, &written);
	check(written, data_b);

	// the queue was emptied
	accessor.loadBlock(pos, loaded);
	UASSERT(loaded == written);
	UASSERT(accessor.m_pending.empty());
	UASSERTEQ(size_t, accessor.m_pending_bytes, 0);
}

void TestMapDatabaseAccessor::testWritePending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	accessor.queueBlock(pos, std::string(data_a));
	UASSERT(accessor.writePending());

	db.loadBlock(pos, &dest);
	UASSERT(dest == data_a);
	accessor.loadBlock(pos, dest);
	UASSERT(dest == data_a);
	UASSERT(accessor.m_pending.empty());
}

void TestMapDatabaseAccessor::testManyBlocks()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	// more than fits in one lock hold
	for (int i = 0; i < 300; i++)
		accessor.queueBlock(v3s16(i, 0, 0), std::string(data_a));
	UASSERT(accessor.writePending());
	UASSERT(accessor.m_pending.empty());
	for (int i = 0; i < 300; i++) {
		db.loadBlock(v3s16(i, 0, 0), &dest);
		UASSERT(dest == data_a);
	}
}

void TestMapDatabaseAccessor::testReplaceWhileWriting()
{
	HookDatabase db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	bool replaced = false;
	db.on_save = [&] {
		if (!replaced)
			accessor.queueBlock(pos, std::string(data_b));
		replaced = true;
	};

	accessor.queueBlock(pos, std::string(data_a));
	UASSERT(accessor.writePending());

	// the newer version stays queued and is not lost
	accessor.loadBlock(pos, dest);
	UASSERT(dest == data_b);
	UASSERT(accessor.writePending());
	db.loadBlock(pos, &dest);
	UASSERT(dest == data_b);
	UASSERT(accessor.m_pending.empty());
}

void TestMapDatabaseAccessor::testReplacePending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	accessor.queueBlock(pos, std::string(data_a));
	accessor.queueBlock(pos, std::string(data_b));
	accessor.loadBlock(pos, dest);
	UASSERT(dest == data_b);

	UASSERT(accessor.writePending());
	db.loadBlock(pos, &dest);
	UASSERT(dest == data_b);

	// a newer version takes priority over the database
	accessor.queueBlock(pos, std::string(data_a));
	accessor.loadBlock(pos, dest);
	UASSERT(dest == data_a);
}

void TestMapDatabaseAccessor::testDeletePending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	// never written
	accessor.queueBlock(pos, std::string(data_a));
	accessor.deleteBlock(pos);
	UASSERT(accessor.writePending());
	accessor.loadBlock(pos, dest);
	UASSERT(dest.empty());
	db.loadBlock(pos, &dest);
	UASSERT(dest.empty());

	// written, with a newer version pending
	accessor.queueBlock(pos, std::string(data_a));
	UASSERT(accessor.writePending());
	accessor.queueBlock(pos, std::string(data_b));
	UASSERT(accessor.deleteBlock(pos));
	UASSERT(accessor.writePending());
	accessor.loadBlock(pos, dest);
	UASSERT(dest.empty());
}

void TestMapDatabaseAccessor::testListPending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::vector<v3s16> list;

	accessor.queueBlock(pos, std::string(data_a));
	accessor.listAllLoadableBlocks(list);
	UASSERTEQ(size_t, list.size(), 1);

	// in the database and pending again, still listed once
	UASSERT(accessor.writePending());
	accessor.queueBlock(pos, std::string(data_b));
	accessor.queueBlock(pos2, std::string(data_a));
	list.clear();
	accessor.listAllLoadableBlocks(list);
	UASSERTEQ(size_t, list.size(), 2);

	// what the caller already had is left alone
	list = {v3s16(9, 9, 9), v3s16(0, 0, 0), v3s16(9, 9, 9)};
	accessor.listAllLoadableBlocks(list);
	UASSERTEQ(size_t, list.size(), 5);
	UASSERT(list[0] == v3s16(9, 9, 9) && list[2] == v3s16(9, 9, 9));
}

void TestMapDatabaseAccessor::testFailedWrite()
{
	FailingDatabase db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	// written, then a newer version that can't be written
	accessor.queueBlock(pos, std::string(data_a));
	UASSERT(accessor.writePending());
	db.fail = true;
	accessor.queueBlock(pos, std::string(data_b));
	accessor.queueBlock(pos2, std::string(data_a));
	UASSERT(!accessor.writePending());
	UASSERTEQ(size_t, accessor.m_pending.size(), 2);

	accessor.loadBlock(pos, dest);
	UASSERT(dest == data_b);

	db.fail = false;
	UASSERT(accessor.writePending());
	UASSERTEQ(size_t, accessor.m_pending.size(), 0);
	db.loadBlock(pos, &dest);
	UASSERT(dest == data_b);
	db.loadBlock(pos2, &dest);
	UASSERT(dest == data_a);
}

void TestMapDatabaseAccessor::testThrowingWrite()
{
	FailingDatabase db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;

	db.throw_error = true;
	accessor.queueBlock(pos, std::string(data_a));
	bool thrown = false;
	try {
		accessor.writePending();
	} catch (DatabaseException &) {
		thrown = true;
	}
	UASSERT(thrown);
	UASSERTEQ(size_t, accessor.m_pending.size(), 1);

	db.throw_error = false;
	UASSERT(accessor.writePending());
	UASSERTEQ(size_t, accessor.m_pending.size(), 0);
}

void TestMapDatabaseAccessor::testFullQueue()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;

	const std::string big(1024 * 1024, 'x');
	for (int i = 0; i < 64; i++)
		accessor.queueBlock(v3s16(i, 0, 0), std::string(big));

	std::atomic<bool> queued = false;
	std::thread t([&] {
		accessor.queueBlock(pos, std::string(data_a));
		queued = true;
	});

	sleep_ms(100);
	UASSERT(!queued);
	UASSERT(accessor.writePending());
	t.join();
	UASSERT(queued);
	UASSERTEQ(size_t, accessor.m_pending.size(), 1);
}

void TestMapDatabaseAccessor::testThread()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::atomic<bool> reported = false;
	accessor.start([&](const std::string &) { reported = true; });
	std::string dest;

	accessor.queueBlock(pos, std::string(data_a));
	UASSERT(waitUntil([&] {
		MutexAutoLock dblock(accessor.mutex);
		db.loadBlock(pos, &dest);
		return !dest.empty();
	}));
	UASSERT(dest == data_a);

	// the rest is written when stopping
	accessor.queueBlock(pos2, std::string(data_b));
	accessor.stop();
	db.loadBlock(pos2, &dest);
	UASSERT(dest == data_b);
	UASSERT(!reported);
}

void TestMapDatabaseAccessor::testFatalError()
{
	FailingDatabase db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	accessor.retry_delay_ms = 1;

	std::atomic<bool> reported = false;
	accessor.start([&](const std::string &) { reported = true; });

	db.fail = true;
	accessor.queueBlock(pos, std::string(data_a));
	UASSERT(waitUntil([&] { return reported.load(); }));

	// nothing is written anymore, so this must not wait for space
	const std::string big(1024 * 1024, 'x');
	for (int i = 0; i < 70; i++)
		accessor.queueBlock(v3s16(i, 0, 0), std::string(big));

	// and the blocks are dropped
	std::string dest;
	accessor.loadBlock(v3s16(0, 0, 0), dest);
	UASSERT(dest.empty());
	accessor.stop();
}
