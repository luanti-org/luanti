// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <functional>
#include <exception>
#include <sstream>
#include <vector>

#include "irrlichttypes_bloated.h"
#include "porting.h"
#include "filesys.h"
#include "mapnode.h"

class TestFailedException { // don’t derive from std::exception to avoid accidental catch
public:
	TestFailedException(std::string in_message, const char *in_file, int in_line)
		: message(std::move(in_message))
		, file(fs::GetFilenameFromPath(in_file))
		, line(in_line)
	{}

	const std::string message;
	const std::string file;
	const int line;
};

// Runs a unit test and reports results
#define TEST(fxn, ...) runTest(#fxn, [&] () { fxn(__VA_ARGS__); });

// Asserts the specified condition is true, or fails the current unit test
#define UASSERT(x) \
	if (!(x)) { \
		throw TestFailedException(#x, __FILE__, __LINE__); \
	}

// Asserts the specified condition is true, or fails the current unit test
// and prints the format specifier fmt
#define UTEST(x, fmt, ...) \
	if (!(x)) { \
		char utest_buf[1024]; \
		porting::mt_snprintf(utest_buf, sizeof(utest_buf), fmt, __VA_ARGS__); \
		throw TestFailedException(utest_buf, __FILE__, __LINE__); \
	}

// Asserts the comparison specified by CMP is true, or fails the current unit test
#define UASSERTCMP(T, CMP, actual, expected) { \
	T a = (actual); \
	T e = (expected); \
	if (!(a CMP e)) { \
		std::ostringstream message; \
		message << #actual " " #CMP " " #expected; \
		message << std::endl << "    actual  : " << a; \
		message << std::endl << "    expected: " << e; \
		throw TestFailedException(message.str(), __FILE__, __LINE__); \
	} \
}

#define UASSERTEQ(T, actual, expected) UASSERTCMP(T, ==, actual, expected)

// UASSERTs that the specified exception occurs
#define EXCEPTION_CHECK(EType, code) {    \
	bool exception_thrown = false;        \
	try {                                 \
		code;                             \
	} catch (EType &e) {                  \
		exception_thrown = true;          \
	}                                     \
	UTEST(exception_thrown, "Exception %s not thrown", #EType); \
}

class IGameDef;

class TestBase {
public:
	bool testModule(IGameDef *gamedef);
	std::string getTestTempDirectory();
	std::string getTestTempFile();

	virtual void runTests(IGameDef *gamedef) = 0;
	virtual const char *getName() = 0;

	u32 num_tests_failed;
	u32 num_tests_run;

	void runTest(const char *name, std::function<void()> &&test);

private:
	std::string m_test_dir;
};

class TestManager {
public:
	static std::vector<TestBase *> &getTestModules()
	{
		static std::vector<TestBase *> m_modules_to_test;
		return m_modules_to_test;
	}

	static void registerTestModule(TestBase *module)
	{
		getTestModules().push_back(module);
	}
};

// A few item and node definitions for those tests that need them
extern content_t t_CONTENT_STONE;
extern content_t t_CONTENT_GRASS;
extern content_t t_CONTENT_TORCH;
extern content_t t_CONTENT_WATER;
extern content_t t_CONTENT_LAVA;
extern content_t t_CONTENT_BRICK;

bool run_tests();
bool run_tests(const std::string &module_name);
