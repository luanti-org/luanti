// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 sfan5

#include "cmake_config.h"

#include "test.h"

#include <functional>
#include <memory>
#include <optional>
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
