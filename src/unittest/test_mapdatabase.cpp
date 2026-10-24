// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 sfan5

#include "cmake_config.h"

#include "test.h"

#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include "servermap.h"
#include "noise.h"
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
	void testWritePending();
	void testReplacePending();
	void testDeletePending();
	void testFailedWrite();
	void testFullQueue();

private:
	class FailingDatabase : public Database_Dummy
	{
	public:
		bool fail = false;

		bool saveBlock(const v3s16 &pos, std::string_view data) override
		{
			return !fail && Database_Dummy::saveBlock(pos, data);
		}
	};

	// Used to compare what a load returns to what was queued
	static std::string unpack(const std::string &from_db);

	const v3s16 pos{1, 2, 3};
	const v3s16 pos2{4, 5, 6};
	const std::string raw_a = std::string(1000, 'a');
	const std::string raw_b = std::string(2000, 'b');
};

static TestMapDatabaseAccessor g_test_instance_accessor;

void TestMapDatabaseAccessor::runTests(IGameDef *gamedef)
{
	TEST(testLoadPending);
	TEST(testWritePending);
	TEST(testReplacePending);
	TEST(testDeletePending);
	TEST(testFailedWrite);
	TEST(testFullQueue);
}

std::string TestMapDatabaseAccessor::unpack(const std::string &from_db)
{
	auto [is, version] = ServerMap::createBlockIStream(&from_db);
	std::ostringstream os(std::ios_base::binary);
	os << is->rdbuf();
	return os.str();
}

void TestMapDatabaseAccessor::testLoadPending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	accessor.queueBlock(pos, std::string(raw_a));
	accessor.loadBlock(pos, dest);
	UASSERT(unpack(dest) == raw_a);

	accessor.compressPending();
	accessor.loadBlock(pos, dest);
	UASSERT(unpack(dest) == raw_a);

	// nothing was written yet
	db.loadBlock(pos, &dest);
	UASSERT(dest.empty());
}

void TestMapDatabaseAccessor::testWritePending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	accessor.queueBlock(pos, std::string(raw_a));
	UASSERT(accessor.writePending());

	db.loadBlock(pos, &dest);
	UASSERT(unpack(dest) == raw_a);
	accessor.loadBlock(pos, dest);
	UASSERT(unpack(dest) == raw_a);

	std::vector<v3s16> list;
	accessor.listAllLoadableBlocks(list);
	UASSERTEQ(size_t, list.size(), 1);
	UASSERT(list.front() == pos);
}

void TestMapDatabaseAccessor::testReplacePending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	accessor.queueBlock(pos, std::string(raw_a));
	accessor.queueBlock(pos, std::string(raw_b));
	accessor.loadBlock(pos, dest);
	UASSERT(unpack(dest) == raw_b);

	UASSERT(accessor.writePending());
	db.loadBlock(pos, &dest);
	UASSERT(unpack(dest) == raw_b);

	// a newer version takes priority over the database
	accessor.queueBlock(pos, std::string(raw_a));
	accessor.loadBlock(pos, dest);
	UASSERT(unpack(dest) == raw_a);
}

void TestMapDatabaseAccessor::testDeletePending()
{
	Database_Dummy db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	// never written
	accessor.queueBlock(pos, std::string(raw_a));
	UASSERT(accessor.deleteBlock(pos));
	UASSERT(accessor.writePending());
	accessor.loadBlock(pos, dest);
	UASSERT(dest.empty());
	db.loadBlock(pos, &dest);
	UASSERT(dest.empty());

	// written, with a newer version pending
	accessor.queueBlock(pos, std::string(raw_a));
	UASSERT(accessor.writePending());
	accessor.queueBlock(pos, std::string(raw_b));
	UASSERT(accessor.deleteBlock(pos));
	UASSERT(accessor.writePending());
	accessor.loadBlock(pos, dest);
	UASSERT(dest.empty());
}

void TestMapDatabaseAccessor::testFailedWrite()
{
	FailingDatabase db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;
	std::string dest;

	// written, then a newer version that can't be written
	accessor.queueBlock(pos, std::string(raw_a));
	UASSERT(accessor.writePending());
	db.fail = true;
	accessor.queueBlock(pos, std::string(raw_b));
	accessor.queueBlock(pos2, std::string(raw_a));
	UASSERT(!accessor.writePending());

	accessor.loadBlock(pos, dest);
	UASSERT(unpack(dest) == raw_b);

	// must not wait for the queue, and lists each block once
	std::vector<v3s16> list;
	accessor.listAllLoadableBlocks(list);
	UASSERTEQ(size_t, list.size(), 2);

	db.fail = false;
	UASSERT(accessor.writePending());
	db.loadBlock(pos, &dest);
	UASSERT(unpack(dest) == raw_b);
	db.loadBlock(pos2, &dest);
	UASSERT(unpack(dest) == raw_a);
}

void TestMapDatabaseAccessor::testFullQueue()
{
	FailingDatabase db;
	MapDatabaseAccessor accessor;
	accessor.dbase = &db;

	db.fail = true;
	accessor.queueBlock(pos, std::string(raw_a));
	UASSERT(!accessor.writePending());

	// Random data does not compress, so the queue fills up
	std::string noise(1024 * 1024, '\0');
	PcgRandom(42).bytes(noise.data(), noise.size());
	int queued = 0;
	while (queued < 200 && accessor.queueBlock(v3s16(queued, 0, 0), std::string(noise)))
		queued++;
	UASSERT(queued < 200);

	db.fail = false;
	UASSERT(accessor.writePending());
	UASSERT(accessor.queueBlock(pos, std::string(raw_b)));
}
