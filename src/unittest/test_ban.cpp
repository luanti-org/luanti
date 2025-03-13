// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include "test.h"

#include "server/ban.h"

class TestBan : public TestBase
{
public:
	TestBan() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestBan"; }

	void runTests(IGameDef *gamedef);

private:
	void testCreate();
	void testAdd();
	void testRemove();
	void testModificationFlag();
	void testGetBanName();
	void testGetBanDescription();

	std::string m_testbm, m_testbm2;
	void reinitTestEnv();
};

static TestBan g_test_instance;

void TestBan::runTests(IGameDef *gamedef)
{
	reinitTestEnv();
	TEST(testCreate);

	reinitTestEnv();
	TEST(testAdd);

	reinitTestEnv();
	TEST(testRemove);

	reinitTestEnv();
	TEST(testModificationFlag);

	reinitTestEnv();
	TEST(testGetBanName);

	reinitTestEnv();
	TEST(testGetBanDescription);
}

void TestBan::reinitTestEnv()
{
	m_testbm = getTestTempDirectory().append(DIR_DELIM "testbm.txt");
	m_testbm2 = getTestTempDirectory().append(DIR_DELIM "testbm2.txt");

	fs::DeleteSingleFileOrEmptyDirectory(m_testbm);
	fs::DeleteSingleFileOrEmptyDirectory(m_testbm2);
}

void TestBan::testCreate()
{
	// test save on object removal
	{
		BanManager bm(m_testbm);
	}

	UASSERT(fs::IsFile(m_testbm));

	// test manual save
	{
		BanManager bm(m_testbm2);
		bm.save();
		UASSERT(fs::IsFile(m_testbm2));
	}
}

void TestBan::testAdd()
{
	std::string bm_test1_entry = "192.168.0.246";
	std::string bm_test1_result = "test_username";

	BanManager bm(m_testbm);
	bm.add(bm_test1_entry, bm_test1_result);

	UASSERT(bm.getBanName(bm_test1_entry) == bm_test1_result);
}

void TestBan::testRemove()
{
	std::string bm_test1_entry = "192.168.0.249";
	std::string bm_test1_result = "test_username";

	std::string bm_test2_entry = "192.168.0.250";
	std::string bm_test2_result = "test_username7";

	BanManager bm(m_testbm);

	// init data
	bm.add(bm_test1_entry, bm_test1_result);
	bm.add(bm_test2_entry, bm_test2_result);

	// the test
	bm.remove(bm_test1_entry);
	UASSERT(bm.getBanName(bm_test1_entry).empty());

	bm.remove(bm_test2_result);
	UASSERT(bm.getBanName(bm_test2_result).empty());
}

void TestBan::testModificationFlag()
{
	BanManager bm(m_testbm);
	bm.add("192.168.0.247", "test_username");
	UASSERT(bm.isModified());

	bm.remove("192.168.0.247");
	UASSERT(bm.isModified());

	// Clear the modification flag
	bm.save();

	// Test modification flag is entry was not present
	bm.remove("test_username");
	UASSERT(!bm.isModified());
}

void TestBan::testGetBanName()
{
	std::string bm_test1_entry = "192.168.0.247";
	std::string bm_test1_result = "test_username";

	BanManager bm(m_testbm);
	bm.add(bm_test1_entry, bm_test1_result);

	// Test with valid entry
	UASSERT(bm.getBanName(bm_test1_entry) == bm_test1_result);

	// Test with invalid entry
	UASSERT(bm.getBanName("---invalid---").empty());
}

void TestBan::testGetBanDescription()
{
	std::string bm_test1_entry = "192.168.0.247";
	std::string bm_test1_entry2 = "test_username";

	std::string bm_test1_result = "192.168.0.247|test_username";

	BanManager bm(m_testbm);
	bm.add(bm_test1_entry, bm_test1_entry2);

	UASSERT(bm.getBanDescription(bm_test1_entry) == bm_test1_result);
	UASSERT(bm.getBanDescription(bm_test1_entry2) == bm_test1_result);
}
