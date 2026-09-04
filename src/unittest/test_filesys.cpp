// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <sstream>
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

#include "log.h"
#include "serialization.h"
#include "nodedef.h"
#include "noise.h"

class TestFileSys : public TestBase
{
public:
	TestFileSys() {	TestManager::registerTestModule(this); }
	const char *getName() {	return "TestFileSys"; }

	void runTests(IGameDef *gamedef);

	void testIsDirDelimiter();
	void testPathsEqual();
	void testPathStartsWith();
	void testMakePathRelativeTo();
	void testRemoveLastPathComponent();
	void testRemoveLastPathComponentWithTrailingDelimiter();
	void testRemoveRelativePathComponent();
	void testAbsolutePath();
	void testSafeWriteToFile();
	void testCopyFileContents();
	void testNonExist();
	void testRecursiveDelete();
	void testGetRecursiveSubPaths();
	void testUnicodePathsFuzz();
};

static TestFileSys g_test_instance;

void TestFileSys::runTests(IGameDef *gamedef)
{
	TEST(testIsDirDelimiter);
	TEST(testPathsEqual);
	TEST(testPathStartsWith);
	TEST(testMakePathRelativeTo);
	TEST(testRemoveLastPathComponent);
	TEST(testRemoveLastPathComponentWithTrailingDelimiter);
	TEST(testRemoveRelativePathComponent);
	TEST(testAbsolutePath);
	TEST(testSafeWriteToFile);
	TEST(testCopyFileContents);
	TEST(testNonExist);
	TEST(testRecursiveDelete);
	TEST(testGetRecursiveSubPaths);
	TEST(testUnicodePathsFuzz);
}

////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
static constexpr bool win32 = true;
#else
static constexpr bool win32 = false;
#endif

// adjusts a POSIX path to system-specific conventions
// -> changes '/' to DIR_DELIM
// -> absolute paths start with "C:\\" on windows
static std::string p(std::string path)
{
	str_replace(path, '/', DIR_DELIM_CHAR);

#ifdef _WIN32
	if (path[0] == '\\')
		path.insert(0, "C:");
#endif

	return path;
}


void TestFileSys::testIsDirDelimiter()
{
	UASSERT(strlen(DIR_DELIM) == 1);
	UASSERT((DIR_DELIM)[0] == DIR_DELIM_CHAR);

	UASSERT(fs::IsDirDelimiter('/') == true);
	UASSERT(fs::IsDirDelimiter('A') == false);
	UASSERT(fs::IsDirDelimiter(0) == false);
	UASSERT(fs::IsDirDelimiter('\\') == win32);

	UASSERT(my_tolower(DIR_DELIM_CHAR) == DIR_DELIM_CHAR);
	for (int c = 0; c < 256; c++) {
		if (c == DIR_DELIM_CHAR)
			continue;
		UASSERT(my_tolower((char)c) != DIR_DELIM_CHAR); // this would be very funny
	}
}


void TestFileSys::testPathsEqual()
{
	const int numpaths = 6;
	std::string paths[numpaths] = {
		"",
		p("/"),
		p("/home/user/luanti"),
		p("/home/user/LUANTI"),
		p("//home//user//luanti"),
		p("/home/user/luanti/"),
	};
	/*
		expected fs::PathsEqual results
		0 = returns false
		1 = returns true
		4 = returns true only when FILESYS_CASE_INSENSITIVE
	*/
	u8 expected_results[numpaths][numpaths] = {
		{1,0,0,0,0,0},
		{0,1,0,0,0,0},
		{0,0,1,4,1,1},
		{0,0,4,1,4,4},
		{0,0,1,4,1,1},
		{0,0,1,4,1,1},
	};

	for (int i = 0; i < numpaths; i++)
	for (int j = 0; j < numpaths; j++){
		bool equal = fs::PathsEqual(paths[i], paths[j]);
		int expected = expected_results[i][j];
		if(expected == 0){
			UASSERT(equal == false);
		} else if(expected == 1) {
			UASSERT(equal == true);
		} else  if(expected == 4) {
			UASSERT(equal == (bool)FILESYS_CASE_INSENSITIVE);
		}
	}
}


void TestFileSys::testPathStartsWith()
{
	const int numpaths = 12;
	std::string paths[numpaths] = {
		"",
		p("/"),
		p("/home/user/minetest"),
		p("/home/user/minetest/bin"),
		p("/home/user/.minetest"),
		p("/tmp/dir/file"),
		p("/tmp/file/"),
		p("/tmP/file"),
		p("/tmp"),
		p("/tmp/dir"),
		p("/home/user2/minetest/worlds"),
		p("/home/user2/minetest/world"),
	};
	/*
		expected fs::PathStartsWith results
		(row for every path, column for every prefix)
		0 = returns false
		1 = returns true
		2 = returns false on windows, true elsewhere
		3 = returns true on windows, false elsewhere
		4 = returns true only when FILESYS_CASE_INSENSITIVE
	*/
	u8 expected_results[numpaths][numpaths] = {
		{1,2,0,0,0,0,0,0,0,0,0,0},
		{0,1,0,0,0,0,0,0,0,0,0,0},
		{0,1,1,0,0,0,0,0,0,0,0,0},
		{0,1,1,1,0,0,0,0,0,0,0,0},
		{0,1,0,0,1,0,0,0,0,0,0,0},
		{0,1,0,0,0,1,0,0,1,1,0,0},
		{0,1,0,0,0,0,1,4,1,0,0,0},
		{0,1,0,0,0,0,4,1,4,0,0,0},
		{0,1,0,0,0,0,0,0,1,0,0,0},
		{0,1,0,0,0,0,0,0,1,1,0,0},
		{0,1,0,0,0,0,0,0,0,0,1,0},
		{0,1,0,0,0,0,0,0,0,0,0,1},
	};

	for (int i = 0; i < numpaths; i++)
	for (int j = 0; j < numpaths; j++){
		bool starts = fs::PathStartsWith(paths[i], paths[j]);
		int expected = expected_results[i][j];
		if(expected == 0){
			UASSERT(starts == false);
		} else if(expected == 1) {
			UASSERT(starts == true);
		} else if(expected == 2) {
			UASSERT(starts == !win32);
		} else if(expected == 3) {
			UASSERT(starts == win32);
		} else if(expected == 4) {
			UASSERT(starts == (bool)FILESYS_CASE_INSENSITIVE);
		}
	}
}


void TestFileSys::testMakePathRelativeTo()
{
	const auto dir_path = getTestTempDirectory() + DIR_DELIM "testMakePathRelativeToTestDir";
	UASSERT(fs::CreateAllDirs(dir_path));

	std::string dirs[] = {
		dir_path + DIR_DELIM "d1",
		dir_path + DIR_DELIM "d1" DIR_DELIM "d2",
		dir_path + DIR_DELIM "_d3",
		dir_path + DIR_DELIM "d12",
		dir_path + DIR_DELIM "d22",
	};
	std::string files[] = {
		dirs[0] + DIR_DELIM "f1",
		dirs[1] + DIR_DELIM "f2",
		dirs[0] + DIR_DELIM ".f3",
	};

	for (auto &it : dirs)
		fs::CreateDir(it);
	for (auto &it : files)
		open_ofstream(it.c_str(), false).close();

	auto rel = [&](auto &&child, auto &&parent) {
		return fs::MakePathRelativeTo(
				dir_path + DIR_DELIM + p(child),
				dir_path + DIR_DELIM + p(parent)
			);
	};

	UASSERTEQ(auto, rel("", ""), p("."));
	UASSERTEQ(auto, rel(".", ""), p("."));
	UASSERTEQ(auto, rel("./.", ""), p("."));
	UASSERTEQ(auto, rel("d1", ""), p("d1"));
	UASSERTEQ(auto, rel("d1/", ""), p("d1"));
	UASSERTEQ(auto, rel("d1/d2", ""), p("d1/d2"));
	UASSERTEQ(auto, rel("d1///d2/", ""), p("d1/d2"));
	UASSERTEQ(auto, rel("_d3", ""), p("_d3"));
	UASSERTEQ(auto, rel("d12", ""), p("d12"));
	UASSERTEQ(auto, rel("d22", ""), p("d22"));
	UASSERTEQ(auto, rel("non_existent", ""), p("non_existent"));
	UASSERTEQ(auto, rel("d22/non_existent", ""), p("d22/non_existent"));
	UASSERTEQ(auto, rel("non_existent/non_existent", ""), p("non_existent/non_existent"));
	UASSERTEQ(auto, rel("noexist/.///noexist", ""), p("noexist/noexist"));
	UASSERTEQ(auto, rel("d1/f1", ""), p("d1/f1"));

	UASSERTEQ(auto, rel("", "."), p("."));
	UASSERTEQ(auto, rel(".", ""), p("."));
	UASSERTEQ(auto, rel(".", "."), p("."));
	UASSERTEQ(auto, rel("d1", "."), p("d1"));
	UASSERTEQ(auto, rel("d1", "d1"), p("."));
	UASSERTEQ(auto, rel("d1/", "d1"), p("."));
	UASSERTEQ(auto, rel("d1", "d1/."), p("."));
	UASSERTEQ(auto, rel("d1/./d2", "d1/."), p("d2"));
	UASSERTEQ(auto, rel("d1/..", "d1"), "");
	UASSERTEQ(auto, rel("d1/../d12", "d1"), "");
	UASSERTEQ(auto, rel("d1/../d1/d2/", "d1"), p("d2"));
}


void TestFileSys::testRemoveLastPathComponent()
{
	std::string path, result, removed;

	UASSERT(fs::RemoveLastPathComponent("") == "");

	path = p("/home/user/minetest/bin/..//worlds/world1");
	result = fs::RemoveLastPathComponent(path, &removed, 0);
	UASSERT(result == path);
	UASSERT(removed == "");
	result = fs::RemoveLastPathComponent(path, &removed, 1);
	UASSERT(result == p("/home/user/minetest/bin/..//worlds"));
	UASSERT(removed == p("world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 2);
	UASSERT(result == p("/home/user/minetest/bin/.."));
	UASSERT(removed == p("worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 3);
	UASSERT(result == p("/home/user/minetest/bin"));
	UASSERT(removed == p("../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 4);
	UASSERT(result == p("/home/user/minetest"));
	UASSERT(removed == p("bin/../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 5);
	UASSERT(result == p("/home/user"));
	UASSERT(removed == p("minetest/bin/../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 6);
	UASSERT(result == p("/home"));
	UASSERT(removed == p("user/minetest/bin/../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 7);
	UASSERTEQ(auto, result, win32 ? "C:" : "/");
	UASSERT(removed == p("home/user/minetest/bin/../worlds/world1"));

	path = p("./README.txt");
	result = fs::RemoveLastPathComponent(path, &removed);
	UASSERT(result == ".");
	UASSERT(removed == "README.txt");

#ifdef __unix__
	path = "/README.txt";
	result = fs::RemoveLastPathComponent(path, &removed);
	UASSERT(result == "/");
	UASSERT(removed == "README.txt");

	path = "README.txt";
	result = fs::RemoveLastPathComponent(path, &removed);
	UASSERT(result == ""); // working directory
	UASSERT(removed == "README.txt");

	path = "///";
	result = fs::RemoveLastPathComponent(path, &removed);
	UASSERT(result == "/");
	UASSERT(removed == "");
#endif
}


void TestFileSys::testRemoveLastPathComponentWithTrailingDelimiter()
{
	std::string path, result, removed;

	path = p("/home/user/minetest/bin/..//worlds/world1/");
	result = fs::RemoveLastPathComponent(path, &removed, 0);
	UASSERT(result == path);
	UASSERT(removed == "");
	result = fs::RemoveLastPathComponent(path, &removed, 1);
	UASSERT(result == p("/home/user/minetest/bin/..//worlds"));
	UASSERT(removed == p("world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 2);
	UASSERT(result == p("/home/user/minetest/bin/.."));
	UASSERT(removed == p("worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 3);
	UASSERT(result == p("/home/user/minetest/bin"));
	UASSERT(removed == p("../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 4);
	UASSERT(result == p("/home/user/minetest"));
	UASSERT(removed == p("bin/../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 5);
	UASSERT(result == p("/home/user"));
	UASSERT(removed == p("minetest/bin/../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 6);
	UASSERT(result == p("/home"));
	UASSERT(removed == p("user/minetest/bin/../worlds/world1"));
	result = fs::RemoveLastPathComponent(path, &removed, 7);
	UASSERTEQ(auto, result, win32 ? "C:" : "/");
	UASSERT(removed == p("home/user/minetest/bin/../worlds/world1"));
}


void TestFileSys::testRemoveRelativePathComponent()
{
	std::string path, result;

	path = p("/home/user/minetest/bin");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, path);
	path = p("/home/user/minetest/bin/../worlds/world1");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, p("/home/user/minetest/worlds/world1"));
	path = p("/home/user/minetest/bin/../worlds/world1/");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, p("/home/user/minetest/worlds/world1"));
	path = p(".");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, "");
	path = p("../a");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, "");
	path = p("./subdir/../..");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, "");
	path = p("/a/b/c/.././../d/../e/f/g/../h/i/j/../../../..");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, p("/a/e"));
	path = p("somewhere//.//here");
	result = fs::RemoveRelativePathComponents(path);
	UASSERTEQ(auto, result, p("somewhere/here"));
}


void TestFileSys::testAbsolutePath()
{
	const auto dir_path = getTestTempDirectory();

	/* AbsolutePath */
	UASSERTEQ(auto, fs::AbsolutePath(""), ""); // empty is a not valid path
	const auto cwd = fs::AbsolutePath(".");
	UASSERTCMP(auto, !=, cwd, "");
	{
		const auto dir_path2 = getTestTempFile();
		UASSERTEQ(auto, fs::AbsolutePath(dir_path2), ""); // doesn't exist
		fs::CreateDir(dir_path2);
		const auto absolute_dir_path = fs::AbsolutePath(dir_path2);
		UASSERTCMP(auto, !=, absolute_dir_path, "");// now it does
		const std::filesystem::path absolute_path(absolute_dir_path,
				std::filesystem::path::format::native_format);
		const std::string root_path = absolute_path.root_path().string();
		UASSERTEQ(auto, fs::AbsolutePath(root_path), root_path);
		UASSERTEQ(auto, fs::AbsolutePath(dir_path2 + DIR_DELIM), absolute_dir_path);
		UASSERTEQ(auto, fs::AbsolutePath(dir_path2 + DIR_DELIM + DIR_DELIM), absolute_dir_path);
		UASSERTEQ(auto, fs::AbsolutePath(dir_path2 + DIR_DELIM ".."), fs::AbsolutePath(dir_path));
		// excess . and / are removed
		UASSERTEQ(auto, fs::AbsolutePath(dir_path2 + p("//..")), fs::AbsolutePath(dir_path));
		UASSERTEQ(auto, fs::AbsolutePath(dir_path2 + p("/./.././//")), fs::AbsolutePath(dir_path));
	}

	/* AbsolutePathPartial */
	// equivalent to AbsolutePath if it exists
	UASSERTEQ(auto, fs::AbsolutePathPartial("."), cwd);
	UASSERTEQ(auto, fs::AbsolutePathPartial(dir_path), fs::AbsolutePath(dir_path));
	// usual usage of the function with a partially existing path
	auto expect = cwd + DIR_DELIM + p("does/not/exist");
	UASSERTEQ(auto, fs::AbsolutePathPartial("does/not/exist"), expect);
	UASSERTEQ(auto, fs::AbsolutePathPartial(expect), expect);

	// a nonsense combination as you couldn't actually access it, but allowed by function
	UASSERTEQ(auto, fs::AbsolutePathPartial("bla/blub/../.."), cwd);
	UASSERTEQ(auto, fs::AbsolutePathPartial("./bla/blub/../.."), cwd);

#ifdef __unix__
	// one way to produce the error case is to remove more components than there are
	// but only if the path does not actually exist ("/.." does exist).
	UASSERTEQ(auto, fs::AbsolutePathPartial("/.."), "/");
	UASSERTEQ(auto, fs::AbsolutePathPartial("/noexist/../.."), "");
#endif
	// or with an empty path
	UASSERTEQ(auto, fs::AbsolutePathPartial(""), "");
}


void TestFileSys::testSafeWriteToFile()
{
	{
		const std::string test_data("hello\0world", 11);
		const std::string dest_path = getTestTempFile();
		fs::safeWriteToFile(dest_path, test_data);
		UASSERT(fs::PathExists(dest_path));
		std::string contents_actual;
		UASSERT(fs::ReadFile(dest_path, contents_actual));
		UASSERTEQ(auto, contents_actual, test_data);
	}

	// Writing directly to /tmp could trigger an edge case
	// also try with a bigger amount of data
	{
		std::string test_data;
		test_data.append(499 * 1024, '\v');
		const std::string filename = itos(rand()) + itos(rand());
		const std::string dest_path = fs::TempPath() + DIR_DELIM + filename;

		bool ok = fs::safeWriteToFile(dest_path, test_data);
		ok &= fs::IsFile(dest_path);
		fs::DeleteSingleFileOrEmptyDirectory(dest_path);
		UASSERT(ok);
	}
}

void TestFileSys::testCopyFileContents()
{
	const auto dir_path = getTestTempDirectory();
	const auto file1 = dir_path + DIR_DELIM "src", file2 = dir_path + DIR_DELIM "dst";
	const std::string test_data("hello\0world", 11);

	// error case
	UASSERT(!fs::CopyFileContents(file1, "somewhere"));

	{
		std::ofstream ofs(file1);
		ofs << test_data;
	}

	// normal case
	UASSERT(fs::CopyFileContents(file1, file2));
	std::string contents_actual;
	UASSERT(fs::ReadFile(file2, contents_actual));
	UASSERTEQ(auto, contents_actual, test_data);

	// should overwrite and truncate
	{
		std::ofstream ofs(file2);
		for (int i = 0; i < 10; i++)
			ofs << "OH MY GAH";
	}
	UASSERT(fs::CopyFileContents(file1, file2));
	contents_actual.clear();
	UASSERT(fs::ReadFile(file2, contents_actual));
	UASSERTEQ(auto, contents_actual, test_data);
}

void TestFileSys::testNonExist()
{
	const auto path = getTestTempFile();
	fs::DeleteSingleFileOrEmptyDirectory(path);

	UASSERT(!fs::IsFile(path));
	UASSERT(!fs::IsDir(path));
	UASSERT(!fs::IsExecutable(path));

	std::string s;
	UASSERT(!fs::ReadFile(path, s));
	UASSERT(s.empty());

	UASSERT(!fs::Rename(path, getTestTempFile()));

	std::filebuf buf;
	// with logging enabled to test that code path
	UASSERT(!fs::OpenStream(buf, path.c_str(), std::ios::in, false, true));
	UASSERT(!buf.is_open());

	auto ifs = open_ifstream(path.c_str(), false);
	UASSERT(!ifs.good());
}

void TestFileSys::testRecursiveDelete()
{
	std::string dirs[2];
	dirs[0] = getTestTempDirectory() + DIR_DELIM "a";
	dirs[1] = dirs[0] + DIR_DELIM "b";

	std::string files[2] = {
		dirs[0] + DIR_DELIM "file1",
		dirs[1] + DIR_DELIM "file2"
	};

	for (auto &it : dirs)
		fs::CreateDir(it);
	for (auto &it : files)
		open_ofstream(it.c_str(), false).close();

	for (auto &it : dirs)
		UASSERT(fs::IsDir(it));
	for (auto &it : files)
		UASSERT(fs::IsFile(it));

	UASSERT(fs::RecursiveDelete(dirs[0]));

	for (auto &it : dirs)
		UASSERT(!fs::IsDir(it));
	for (auto &it : files)
		UASSERT(!fs::IsFile(it));

	// Deleting something that doesn't exist is *not* an error
	UASSERT(fs::RecursiveDelete(dirs[0]));
}

void TestFileSys::testGetRecursiveSubPaths()
{
	const auto dir_path = getTestTempDirectory() + DIR_DELIM "recursivetest";
	UASSERT(fs::CreateAllDirs(dir_path));

	std::string dirs[] = {
		dir_path + DIR_DELIM "d1",
		dir_path + DIR_DELIM "d1" DIR_DELIM "d2",
		dir_path + DIR_DELIM "_d3"
	};
	std::string files[] = {
		dirs[0] + DIR_DELIM "f1",
		dirs[1] + DIR_DELIM "f2",
		dirs[0] + DIR_DELIM ".f3",
	};

	for (auto &it : dirs)
		fs::CreateDir(it);
	for (auto &it : files)
		open_ofstream(it.c_str(), false).close();

	std::vector<std::string> dst;
	fs::GetRecursiveSubPaths(dir_path, dst, false);
	UASSERT(CONTAINS(dst, dirs[0]));
	UASSERT(CONTAINS(dst, dirs[1]));
	UASSERT(CONTAINS(dst, dirs[2]));
	UASSERTEQ(size_t, dst.size(), 3);

	dst.clear();
	fs::GetRecursiveSubPaths(dir_path, dst, true);
	UASSERT(CONTAINS(dst, dirs[0]));
	UASSERT(CONTAINS(dst, dirs[1]));
	UASSERT(CONTAINS(dst, dirs[2]));
	UASSERT(CONTAINS(dst, files[0]));
	UASSERT(CONTAINS(dst, files[1]));
	UASSERT(CONTAINS(dst, files[2]));
	UASSERTEQ(size_t, dst.size(), 3+3);

	dst.clear();
	fs::GetRecursiveSubPaths(dir_path, dst, true, "_zzzabczzzz.");
	UASSERT(CONTAINS(dst, dirs[0]));
	UASSERT(CONTAINS(dst, dirs[1]));
	UASSERT(CONTAINS(dst, files[0]));
	UASSERT(CONTAINS(dst, files[1]));
	UASSERTEQ(size_t, dst.size(), 2+2);
}

/* Unicode path fuzzing */

namespace {

struct CodepointRange { u32 first, last; };

/*
  To make exact comparison easy, code points are intentionally restricted
  to characters that are assigned and printable, caseless, and canonically
  stable (NFC and NFD are the same).
*/

// Lower case only, see above. Omit '.' so no name can be "." or ".."
const CodepointRange ascii_ranges[] = {
	{'a', 'z'}, {'0', '9'}, {'-', '-'}, {'_', '_'},
};

// Includes code points outside the BMP which turn into surrogate pairs when
// Windows converts them to UTF-16.
const CodepointRange unicode_ranges[] = {
	// two bytes in utf8
	{0x00A1, 0x00A9}, {0x00AB, 0x00AC}, {0x00AE, 0x00B4},
	{0x00B6, 0x00B9}, // skips U+00B5 which case folds
	{0x00BB, 0x00BB}, {0x00BF, 0x00BF}, {0x00D7, 0x00D7}, {0x00F7, 0x00F7},
	{0x05D0, 0x05EA}, // Hebrew letters
	{0x0627, 0x063A}, // Arabic letters, skipping the ones that decompose
	{0x0641, 0x064A},
	// three bytes in utf8
	{0x0E01, 0x0E2E}, // Thai consonants
	{0x2600, 0x26B0}, // miscellaneous symbols
	{0x3400, 0x4DB5}, // CJK extension A
	{0x4E00, 0x9FA5}, // CJK
	// four bytes in utf8
	{0x1F600, 0x1F64F}, // emoticons
	{0x20000, 0x2A6D6}, // CJK extension B
};

void appendUtf8(std::string &out, u32 cp)
{
	if (cp < 0x80) {
		out += static_cast<char>(cp);
	} else if (cp < 0x800) {
		out += static_cast<char>(0xC0 | (cp >> 6));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		out += static_cast<char>(0xE0 | (cp >> 12));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else {
		out += static_cast<char>(0xF0 | (cp >> 18));
		out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
}

template <size_t N>
u32 randomCodepoint(PcgRandom &rnd, const CodepointRange (&ranges)[N])
{
	// Ranges are picked uniformly, not weighted by size, so that short
	// ranges are well represented.
	const auto &range = ranges[rnd.range(0, static_cast<s32>(N) - 1)];
	return rnd.range(range.first, range.last);
}

// Random name of at most `max_cp` code points (so up to 4*max_cp bytes).
// This must stay below the 255 byte limit for a single name, and entire
// paths must stay below MAX_PATH.
//
// Forces at least one non-ASCII character to prevent accidental use of device
// names (con, nul, com1, ...)
std::string randomUnicodeName(PcgRandom &rnd, int max_cp)
{
	const int len = rnd.range(1, max_cp);
	const int forced = rnd.range(0, len - 1);
	std::string name;
	for (int i = 0; i < len; i++) {
		if (i == forced || rnd.range(0, 3) != 0)
			appendUtf8(name, randomCodepoint(rnd, unicode_ranges));
		else
			appendUtf8(name, randomCodepoint(rnd, ascii_ranges));
	}
	return name;
}

}

void TestFileSys::testUnicodePathsFuzz()
{
	constexpr int NUM_PATHS = 1000;

	// Fixed seed, so that a failure can be reproduced exactly.
	PcgRandom rnd(0x9E3779B9, 0x5BF03635);

	std::set<std::string> seen;

	// Unique random name with up to `max_cp` codepoints
	auto fresh = [&] (int max_cp) {
		std::string name;
		do {
			name = randomUnicodeName(rnd, max_cp);
		} while (!seen.insert(name).second);
		return name;
	};

	std::vector<std::string> names;
	names.reserve(NUM_PATHS);
	for (int i = 0; i < NUM_PATHS; i++)
		names.push_back(fresh(20));

	// The content contains the path it was written to, so an operation that
	// ends up on the wrong path is caught when reading it back.
	auto content_for = [] (const std::string &path) {
		return "unicode fuzz\n" + path;
	};

	// This could be a pure ascii path.
	const std::string base = getTestTempDirectory() + DIR_DELIM "unicodefuzz";

	rawstream << "-------- Creating scratch directories" << std::endl;
	const std::string flat = base + DIR_DELIM + fresh(8);
	const std::string pairs = base + DIR_DELIM + fresh(8);
	const std::string deep = base + DIR_DELIM + fresh(8);
	for (auto &it : {flat, pairs, deep}) {
		UASSERT(fs::CreateAllDirs(it));
		UASSERT(fs::IsDir(it));
	}

	// Fill `flat` using the generated names.
	// Even ones are files, odd ones are a directory holding a file.
	rawstream << "-------- Populating 'flat' scratch directory" << std::endl;
	std::map<std::string, bool> expect_dir;
	for (int i = 0; i < NUM_PATHS; i++) {
		const std::string path = flat + DIR_DELIM + names[i];
		if (i % 2 == 0) {
			expect_dir[names[i]] = false;
			// alternate between two ways of writing the file
			if (i % 4 == 0) {
				UASSERT(fs::safeWriteToFile(path, content_for(path)));
			} else {
				auto ofs = open_ofstream(path.c_str(), true);
				UASSERT(ofs.good());
				ofs << content_for(path);
				ofs.close();
				UASSERT(!ofs.fail());
			}
			UASSERT(fs::IsFile(path));
			UASSERT(!fs::IsDir(path));
		} else {
			expect_dir[names[i]] = true;
			UASSERT(fs::CreateDir(path));
			UASSERT(fs::IsDir(path));
			UASSERT(!fs::IsFile(path));
			// reuse the name of the file created in the previous iteration
			const std::string inner = path + DIR_DELIM + names[i - 1];
			auto ofs = open_ofstream(inner.c_str(), true);
			UASSERT(ofs.good());
			ofs << content_for(inner);
			ofs.close();
			UASSERT(!ofs.fail());
		}
		UASSERT(fs::PathExists(path));
	}
	UASSERTEQ(size_t, expect_dir.size(), NUM_PATHS);

	rawstream << "-------- Test ReadFile" << std::endl;
	for (int i = 0; i < NUM_PATHS; i++) {
		std::string path = flat + DIR_DELIM + names[i];
		if (i % 2 == 1)
			path += DIR_DELIM + names[i - 1];
		std::string actual;
		UASSERT(fs::ReadFile(path, actual, true));
		UASSERTEQ(auto, actual, content_for(path));
	}

	rawstream << "-------- Testing GetDirListing" << std::endl;
	{
		const auto listing = fs::GetDirListing(flat);
		UASSERTEQ(size_t, listing.size(), expect_dir.size());
		std::set<std::string> uniq;
		for (const auto &node : listing) {
			UTEST(expect_dir.count(node.name) == 1,
				"unexpected name in listing: %s", node.name.c_str());
			UASSERT(uniq.insert(node.name).second);
			UASSERTEQ(bool, node.dir, expect_dir[node.name]);
		}
	}

	rawstream << "-------- Testing GetRecursiveSubPaths" << std::endl;
	{
		std::vector<std::string> subpaths;
		fs::GetRecursiveSubPaths(flat, subpaths, true);
		// every entry, plus the file inside each of the directories
		UASSERTEQ(size_t, subpaths.size(), NUM_PATHS + NUM_PATHS / 2);
		const std::set<std::string> got(subpaths.begin(), subpaths.end());
		UASSERTEQ(size_t, got.size(), subpaths.size());
		for (int i = 0; i < NUM_PATHS; i++) {
			const std::string path = flat + DIR_DELIM + names[i];
			UASSERT(got.count(path) == 1);
			if (i % 2 == 1)
				UASSERT(got.count(path + DIR_DELIM + names[i - 1]) == 1);
		}
	}

	// Uses a different scratch directory, `pairs`.
	rawstream << "-------- Testing Rename, CopyFileContents, and "
		<< "DeleteSingleFileOrEmptyDirectory" << std::endl;
	for (int i = 0; i + 1 < NUM_PATHS; i += 2) {
		const std::string src = pairs + DIR_DELIM + names[i];
		const std::string dst = pairs + DIR_DELIM + names[i + 1];
		const std::string content = content_for(src);

		{
			auto ofs = open_ofstream(src.c_str(), true);
			UASSERT(ofs.good());
			ofs << content;
		}
		UASSERT(fs::IsFile(src));

		UASSERT(fs::Rename(src, dst));
		UASSERT(!fs::PathExists(src));
		UASSERT(fs::IsFile(dst));
		std::string actual;
		UASSERT(fs::ReadFile(dst, actual, true));
		UASSERTEQ(auto, actual, content);

		UASSERT(fs::CopyFileContents(dst, src));
		actual.clear();
		UASSERT(fs::ReadFile(src, actual, true));
		UASSERTEQ(auto, actual, content);

		UASSERT(fs::DeleteSingleFileOrEmptyDirectory(src, true));
		UASSERT(fs::DeleteSingleFileOrEmptyDirectory(dst, true));
	}
	UASSERT(fs::GetDirListing(pairs).empty());

	rawstream << "-------- Testing nested unicode paths" << std::endl;
	const std::string abs_deep = fs::AbsolutePath(deep);
	UASSERT(!abs_deep.empty());
	for (int i = 0; i < 20; i++) {
		const std::string n1 = fresh(6);
		const std::string n2 = fresh(6);
		const std::string n3 = fresh(6);
		const std::string top = deep + DIR_DELIM + fresh(6);
		const std::string dir = top + DIR_DELIM + n1 + DIR_DELIM + n2;
		const std::string file = dir + DIR_DELIM + n3;
		const std::string content = content_for(file);
		const std::string sub = n1 + DIR_DELIM + n2 + DIR_DELIM + n3;

		UASSERT(fs::CreateAllDirs(dir));
		UASSERT(fs::IsDir(dir));
		UASSERT(fs::safeWriteToFile(file, content));
		UASSERT(fs::IsFile(file));

		// Test PathStartsWith
		UASSERT(fs::PathStartsWith(fs::AbsolutePath(dir), abs_deep));

		// Test MakePathRelativeTo
		UASSERTEQ(auto, fs::MakePathRelativeTo(file, top), sub);

		// Test RemoveLastPathComponent
		std::string removed;
		UASSERTEQ(auto, fs::RemoveLastPathComponent(file, &removed), dir);
		UASSERTEQ(auto, removed, n3);

		// Test CopyDir
		const std::string copy = deep + DIR_DELIM + fresh(6);
		UASSERT(fs::CopyDir(top, copy));
		std::string actual;
		UASSERT(fs::ReadFile(copy + DIR_DELIM + sub, actual, true));
		UASSERTEQ(auto, actual, content);
		UASSERT(fs::IsFile(file)); // source untouched

		// Test MoveDir
		const std::string moved = deep + DIR_DELIM + fresh(6);
		UASSERT(fs::MoveDir(copy, moved));
		UASSERT(!fs::PathExists(copy));
		actual.clear();
		UASSERT(fs::ReadFile(moved + DIR_DELIM + sub, actual, true));
		UASSERTEQ(auto, actual, content);

		// Test RecursiveDelete, and clean up.
		UASSERT(fs::RecursiveDelete(top));
		UASSERT(!fs::PathExists(top));
		UASSERT(fs::RecursiveDelete(moved));
		UASSERT(!fs::PathExists(moved));
	}

	rawstream << "-------- Final cleanup with RecursiveDelete" << std::endl;
	UASSERT(fs::RecursiveDelete(base));
	UASSERT(!fs::PathExists(base));
}
