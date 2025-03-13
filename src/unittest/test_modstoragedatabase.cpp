// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 bendeutsch, Ben Deutsch <ben@bendeutsch.de>
// Copyright (C) 2021 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>

// This file is an edited copy of test_authdatabase.cpp

#include "cmake_config.h"

#include "test.h"

#include <algorithm>
#include <cstdlib>
#include "database/database-dummy.h"
#include "database/database-files.h"
#include "database/database-sqlite3.h"
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif
#include "filesys.h"

namespace
{
// Anonymous namespace to create classes that are only
// visible to this file
//
// These are helpers that return a *ModStorageDatabase and
// allow us to run the same tests on different databases and
// database acquisition strategies.

class ModStorageDatabaseProvider
{
public:
	virtual ~ModStorageDatabaseProvider() = default;
	virtual ModStorageDatabase *getModStorageDatabase() = 0;
};

class FixedProvider : public ModStorageDatabaseProvider
{
public:
	FixedProvider(ModStorageDatabase *db): m_db(db) {}

	~FixedProvider() = default;

	ModStorageDatabase *getModStorageDatabase() override { return m_db; }

private:
	ModStorageDatabase *m_db;
};

class FilesProvider : public ModStorageDatabaseProvider
{
public:
	FilesProvider(const std::string &dir): m_dir(dir) {}

	~FilesProvider()
	{
		if (m_db)
			m_db->endSave();
		delete m_db;
	}

	ModStorageDatabase *getModStorageDatabase() override
	{
		if (m_db)
			m_db->endSave();
		delete m_db;
		m_db = new ModStorageDatabaseFiles(m_dir);
		m_db->beginSave();
		return m_db;
	}

private:
	std::string m_dir;
	ModStorageDatabase *m_db = nullptr;
};

class SQLite3Provider : public ModStorageDatabaseProvider
{
public:
	SQLite3Provider(const std::string &dir): m_dir(dir) {}

	~SQLite3Provider()
	{
		if (m_db)
			m_db->endSave();
		delete m_db;
	}

	ModStorageDatabase *getModStorageDatabase() override
	{
		if (m_db)
			m_db->endSave();
		delete m_db;
		m_db = new ModStorageDatabaseSQLite3(m_dir);
		m_db->beginSave();
		return m_db;
	}

private:
	std::string m_dir;
	ModStorageDatabase *m_db = nullptr;
};

#if USE_POSTGRESQL
void clearPostgreSQLDatabase(const std::string &connect_string)
{
	ModStorageDatabasePostgreSQL db(connect_string);
	std::vector<std::string> modnames;
	db.beginSave();
	db.listMods(&modnames);
	for (const std::string &modname : modnames)
		db.removeModEntries(modname);
	db.endSave();
}

class PostgreSQLProvider : public ModStorageDatabaseProvider
{
public:
	PostgreSQLProvider(const std::string &connect_string): m_connect_string(connect_string) {}

	~PostgreSQLProvider()
	{
		if (m_db)
			m_db->endSave();
		delete m_db;
	}

	ModStorageDatabase *getModStorageDatabase() override
	{
		if (m_db)
			m_db->endSave();
		delete m_db;
		m_db = new ModStorageDatabasePostgreSQL(m_connect_string);
		m_db->beginSave();
		return m_db;
	};

private:
	std::string m_connect_string;
	ModStorageDatabase *m_db = nullptr;
};
#endif // USE_POSTGRESQL
}

class TestModStorageDatabase : public TestBase
{
public:
	TestModStorageDatabase() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestModStorageDatabase"; }

	void runTests(IGameDef *gamedef);
	void runTestsForCurrentDB();

	void testRecallFail();
	void testCreate();
	void testRecall();
	void testChange();
	void testRecallChanged();
	void testListMods();
	void testRemove();

private:
	ModStorageDatabaseProvider *mod_storage_provider;
};

static TestModStorageDatabase g_test_instance;

void TestModStorageDatabase::runTests(IGameDef *gamedef)
{
	// fixed directory, for persistence
	thread_local const std::string test_dir = getTestTempDirectory();

	// Each set of tests is run twice for each database type except dummy:
	// one where we reuse the same ModStorageDatabase object (to test local caching),
	// and one where we create a new ModStorageDatabase object for each call
	// (to test actual persistence).
	// Since the dummy database is only in-memory, it has no persistence to test.

	ModStorageDatabase *mod_storage_db;

	rawstream << "-------- Dummy database (same object only)" << std::endl;

	mod_storage_db = new Database_Dummy();
	mod_storage_provider = new FixedProvider(mod_storage_db);

	runTestsForCurrentDB();

	delete mod_storage_db;
	delete mod_storage_provider;

	rawstream << "-------- Files database (same object)" << std::endl;

	mod_storage_db = new ModStorageDatabaseFiles(test_dir);
	mod_storage_provider = new FixedProvider(mod_storage_db);

	runTestsForCurrentDB();

	delete mod_storage_db;
	delete mod_storage_provider;

	// reset database
	fs::RecursiveDelete(test_dir + DIR_DELIM + "mod_storage");

	rawstream << "-------- Files database (new objects)" << std::endl;

	mod_storage_provider = new FilesProvider(test_dir);

	runTestsForCurrentDB();

	delete mod_storage_provider;

	rawstream << "-------- SQLite3 database (same object)" << std::endl;

	mod_storage_db = new ModStorageDatabaseSQLite3(test_dir);
	mod_storage_provider = new FixedProvider(mod_storage_db);

	runTestsForCurrentDB();

	delete mod_storage_db;
	delete mod_storage_provider;

	// reset database
	fs::DeleteSingleFileOrEmptyDirectory(test_dir + DIR_DELIM + "mod_storage.sqlite");

	rawstream << "-------- SQLite3 database (new objects)" << std::endl;

	mod_storage_provider = new SQLite3Provider(test_dir);

	runTestsForCurrentDB();

	delete mod_storage_provider;

#if USE_POSTGRESQL
	const char *env_postgresql_connect_string = getenv("MINETEST_POSTGRESQL_CONNECT_STRING");
	if (env_postgresql_connect_string) {
		std::string connect_string(env_postgresql_connect_string);

		rawstream << "-------- PostgreSQL database (same object)" << std::endl;

		clearPostgreSQLDatabase(connect_string);
		mod_storage_db = new ModStorageDatabasePostgreSQL(connect_string);
		mod_storage_provider = new FixedProvider(mod_storage_db);

		runTestsForCurrentDB();

		delete mod_storage_db;
		delete mod_storage_provider;

		rawstream << "-------- PostgreSQL database (new objects)" << std::endl;

		clearPostgreSQLDatabase(connect_string);
		mod_storage_provider = new PostgreSQLProvider(connect_string);

		runTestsForCurrentDB();

		delete mod_storage_provider;
	}
#endif // USE_POSTGRESQL
}

////////////////////////////////////////////////////////////////////////////////

void TestModStorageDatabase::runTestsForCurrentDB()
{
	TEST(testRecallFail);
	TEST(testCreate);
	TEST(testRecall);
	TEST(testChange);
	TEST(testRecallChanged);
	TEST(testListMods);
	TEST(testRemove);
	TEST(testRecallFail);
}

void TestModStorageDatabase::testRecallFail()
{
	ModStorageDatabase *mod_storage_db = mod_storage_provider->getModStorageDatabase();
	StringMap recalled;
	std::vector<std::string> recalled_keys;
	mod_storage_db->getModEntries("mod1", &recalled);
	mod_storage_db->getModKeys("mod1", &recalled_keys);
	UASSERT(recalled.empty());
	UASSERT(recalled_keys.empty());
	std::string key1_value;
	UASSERT(!mod_storage_db->getModEntry("mod1", "key1", &key1_value));
	UASSERT(!mod_storage_db->hasModEntry("mod1", "key1"));
}

void TestModStorageDatabase::testCreate()
{
	ModStorageDatabase *mod_storage_db = mod_storage_provider->getModStorageDatabase();
	UASSERT(mod_storage_db->setModEntry("mod1", "key1", "value1"));
}

void TestModStorageDatabase::testRecall()
{
	ModStorageDatabase *mod_storage_db = mod_storage_provider->getModStorageDatabase();
	StringMap recalled;
	std::vector<std::string> recalled_keys;
	mod_storage_db->getModEntries("mod1", &recalled);
	mod_storage_db->getModKeys("mod1", &recalled_keys);
	UASSERTCMP(std::size_t, ==, recalled.size(), 1);
	UASSERTCMP(std::size_t, ==, recalled_keys.size(), 1);
	UASSERTCMP(std::string, ==, recalled["key1"], "value1");
	UASSERTCMP(std::string, ==, recalled_keys[0], "key1");
	std::string key1_value;
	UASSERT(mod_storage_db->getModEntry("mod1", "key1", &key1_value));
	UASSERTCMP(std::string, ==, key1_value, "value1");
	UASSERT(mod_storage_db->hasModEntry("mod1", "key1"));
}

void TestModStorageDatabase::testChange()
{
	ModStorageDatabase *mod_storage_db = mod_storage_provider->getModStorageDatabase();
	UASSERT(mod_storage_db->setModEntry("mod1", "key1", "value2"));
}

void TestModStorageDatabase::testRecallChanged()
{
	ModStorageDatabase *mod_storage_db = mod_storage_provider->getModStorageDatabase();
	StringMap recalled;
	mod_storage_db->getModEntries("mod1", &recalled);
	UASSERTCMP(std::size_t, ==, recalled.size(), 1);
	UASSERTCMP(std::string, ==, recalled["key1"], "value2");
	std::string key1_value;
	UASSERT(mod_storage_db->getModEntry("mod1", "key1", &key1_value));
	UASSERTCMP(std::string, ==, key1_value, "value2");
	UASSERT(mod_storage_db->hasModEntry("mod1", "key1"));
}

void TestModStorageDatabase::testListMods()
{
	ModStorageDatabase *mod_storage_db = mod_storage_provider->getModStorageDatabase();
	UASSERT(mod_storage_db->setModEntry("mod2", "key1", "value1"));
	UASSERT(mod_storage_db->setModEntry("mod2", "key2", "value1"));
	std::vector<std::string> mod_list;
	mod_storage_db->listMods(&mod_list);
	UASSERTCMP(size_t, ==, mod_list.size(), 2);
	UASSERT(std::find(mod_list.cbegin(), mod_list.cend(), "mod1") != mod_list.cend());
	UASSERT(std::find(mod_list.cbegin(), mod_list.cend(), "mod2") != mod_list.cend());
}

void TestModStorageDatabase::testRemove()
{
	ModStorageDatabase *mod_storage_db = mod_storage_provider->getModStorageDatabase();
	UASSERT(mod_storage_db->removeModEntry("mod1", "key1"));
	UASSERT(!mod_storage_db->removeModEntries("mod1"));
	UASSERT(mod_storage_db->removeModEntries("mod2"));
}
