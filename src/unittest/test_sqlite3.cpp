// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"
#include "database/database-sqlite3.h"
#include <memory>

namespace {

// Observe SQLite's real sleep calls and optionally release a competing writer.
// The default VFS is restored before another test module can use SQLite.
class SleepObserverVFS : public sqlite3_vfs
{
public:
	SleepObserverVFS() : sqlite3_vfs(*sqlite3_vfs_find(nullptr)),
			original(sqlite3_vfs_find(nullptr))
	{
		zName = "unittest-sqlite-sleep";
		xSleep = [](sqlite3_vfs *vfs, int microseconds) {
			auto *self = static_cast<SleepObserverVFS *>(vfs);
			++self->sleeps;
			if (self->release_on_sleep) {
				self->release_result = sqlite3_exec(self->release_on_sleep,
						"COMMIT", nullptr, nullptr, nullptr);
				self->release_on_sleep = nullptr;
			}
			return self->original->xSleep(self->original, microseconds);
		};
		UASSERTEQ(int, sqlite3_vfs_register(this, 1), SQLITE_OK);
	}

	~SleepObserverVFS()
	{
		sqlite3_vfs_register(original, 1);
		sqlite3_vfs_unregister(this);
	}

	sqlite3 *release_on_sleep = nullptr;
	int release_result = SQLITE_ERROR;
	unsigned int sleeps = 0;

private:
	sqlite3_vfs *original;
};

using Connection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

Connection lockDatabase(const std::string &path)
{
	sqlite3 *raw = nullptr;
	int result = sqlite3_open_v2((path + DIR_DELIM + "map.sqlite").c_str(),
			&raw, SQLITE_OPEN_READWRITE, nullptr);
	Connection connection(raw, sqlite3_close);
	UASSERTEQ(int, result, SQLITE_OK);
	UASSERTEQ(int, sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr),
			SQLITE_OK);
	return connection;
}

}

class TestSQLite3 : public TestBase
{
public:
	TestSQLite3() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSQLite3"; }
	void runTests(IGameDef *gamedef)
	{
		TEST(testUncontendedWrite);
		TEST(testReleasedLock);
		TEST(testBusyTimeout);
	}

	void testUncontendedWrite()
	{
		SleepObserverVFS observer;
		MapDatabaseSQLite3 database(getTestTempDirectory());
		UASSERT(database.saveBlock({0, 0, 0}, "uncontended"));
		UASSERTEQ(unsigned int, observer.sleeps, 0);
	}

	void testReleasedLock()
	{
		SleepObserverVFS observer;
		const std::string path = getTestTempDirectory();
		MapDatabaseSQLite3 database(path);
		database.verifyDatabase();
		auto blocker = lockDatabase(path);
		observer.release_on_sleep = blocker.get();

		UASSERT(database.saveBlock({1, 2, 3}, "after lock release"));
		UASSERT(observer.sleeps > 0);
		UASSERTEQ(int, observer.release_result, SQLITE_OK);
		std::string data;
		database.loadBlock({1, 2, 3}, &data);
		UASSERTEQ(std::string, data, "after lock release");
	}

	void testBusyTimeout()
	{
		SleepObserverVFS observer;
		const std::string path = getTestTempDirectory();
		MapDatabaseSQLite3 database(path);
		database.verifyDatabase();
		auto blocker = lockDatabase(path);

		EXCEPTION_CHECK(DatabaseException, database.saveBlock({4, 5, 6}, "locked"));
		UASSERT(observer.sleeps > 0);
		UASSERTEQ(int, sqlite3_exec(blocker.get(), "ROLLBACK", nullptr, nullptr, nullptr),
				SQLITE_OK);
		std::string data;
		database.loadBlock({4, 5, 6}, &data);
		UASSERT(data.empty());
	}
};

static TestSQLite3 g_test_instance;
