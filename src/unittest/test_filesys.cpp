// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <sstream>
#include <algorithm>

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

#if CHECK_CLIENT_BUILD()
	void testWriteZipFile();
#endif
};

static TestFileSys g_test_instance;

void TestFileSys::runTests(IGameDef *gamedef)
{
	TEST(testIsDirDelimiter);
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

#if CHECK_CLIENT_BUILD()
	TEST(testWriteZipFile);
#endif
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
	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '/') {
			path.replace(i, 1, DIR_DELIM);
			i += strlen(DIR_DELIM) - 1; // generally a no-op
		}
	}

#ifdef _WIN32
	if (path[0] == '\\')
		path.insert(0, "C:");
#endif

	return path;
}


void TestFileSys::testIsDirDelimiter()
{
	UASSERT(fs::IsDirDelimiter('/') == true);
	UASSERT(fs::IsDirDelimiter('A') == false);
	UASSERT(fs::IsDirDelimiter(0) == false);
	UASSERT(fs::IsDirDelimiter('\\') == win32);
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
		4 = returns true if and only if
			FILESYS_CASE_INSENSITIVE is true
	*/
	int expected_results[numpaths][numpaths] = {
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
	UASSERT(result == path);
	path = p("/home/user/minetest/bin/../worlds/world1");
	result = fs::RemoveRelativePathComponents(path);
	UASSERT(result == p("/home/user/minetest/worlds/world1"));
	path = p("/home/user/minetest/bin/../worlds/world1/");
	result = fs::RemoveRelativePathComponents(path);
	UASSERT(result == p("/home/user/minetest/worlds/world1"));
	path = p(".");
	result = fs::RemoveRelativePathComponents(path);
	UASSERT(result == "");
	path = p("../a");
	result = fs::RemoveRelativePathComponents(path);
	UASSERT(result == "");
	path = p("./subdir/../..");
	result = fs::RemoveRelativePathComponents(path);
	UASSERT(result == "");
	path = p("/a/b/c/.././../d/../e/f/g/../h/i/j/../../../..");
	result = fs::RemoveRelativePathComponents(path);
	UASSERT(result == p("/a/e"));
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
		UASSERTCMP(auto, !=, fs::AbsolutePath(dir_path2), ""); // now it does
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

#if CHECK_CLIENT_BUILD()
void TestFileSys::testWriteZipFile()
{
	const auto dir_path = getTestTempDirectory() + DIR_DELIM "ziptest";
	const auto nested_dir = dir_path + DIR_DELIM "sub";
	UASSERT(fs::CreateAllDirs(nested_dir));

	const std::string top_level_data = "hello";
	const std::string nested_data = "nested data";

	const auto top_level_file = dir_path + DIR_DELIM "file.txt";
	{
		auto os = open_ofstream(top_level_file.c_str(), false);
		UASSERT(os.good());
		os.write(top_level_data.data(), top_level_data.size());
		UASSERT(os.good());
	}

	const auto nested_file = nested_dir + DIR_DELIM "nested.txt";
	{
		auto os = open_ofstream(nested_file.c_str(), false);
		UASSERT(os.good());
		os.write(nested_data.data(), nested_data.size());
		UASSERT(os.good());
	}

	const auto zip_path = getTestTempFile();
	UASSERT(fs::createZipFile(dir_path, zip_path));
	UASSERT(fs::IsFile(zip_path));

	std::string contents;
	UASSERT(fs::ReadFile(zip_path, contents));

	size_t position = 0;
	auto read_u16 = [&]() -> u16 {
		UASSERT(position + 2 <= contents.size());
		u16 value = static_cast<u8>(contents[position]) |
				(static_cast<u16>(static_cast<u8>(contents[position + 1])) << 8);
		position += 2;
		return value;
	};
	auto read_u32 = [&]() -> u32 {
		UASSERT(position + 4 <= contents.size());
		u32 value = static_cast<u8>(contents[position]) |
				(static_cast<u32>(static_cast<u8>(contents[position + 1])) << 8) |
				(static_cast<u32>(static_cast<u8>(contents[position + 2])) << 16) |
				(static_cast<u32>(static_cast<u8>(contents[position + 3])) << 24);
		position += 4;
		return value;
	};
	auto read_string = [&](size_t length) {
		UASSERT(position + length <= contents.size());
		std::string value = contents.substr(position, length);
		position += length;
		return value;
	};

	auto expect_local_file = [&](const std::string &archive_path,
			const std::string &file_data, u32 crc32, size_t expected_offset) {
		UASSERTEQ(size_t, position, expected_offset);
		UASSERTEQ(u32, read_u32(), 0x04034b50);
		UASSERTEQ(u16, read_u16(), 20);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0x0021);
		UASSERTEQ(u32, read_u32(), crc32);
		UASSERTEQ(u32, read_u32(), static_cast<u32>(file_data.size()));
		UASSERTEQ(u32, read_u32(), static_cast<u32>(file_data.size()));
		UASSERTEQ(u16, read_u16(), static_cast<u16>(archive_path.size()));
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(auto, read_string(archive_path.size()), archive_path);
		UASSERTEQ(auto, read_string(file_data.size()), file_data);
	};

	auto expect_central_directory_entry = [&](const std::string &archive_path,
			const std::string &file_data, u32 crc32, u32 local_header_offset) {
		UASSERTEQ(u32, read_u32(), 0x02014b50);
		UASSERTEQ(u16, read_u16(), 20);
		UASSERTEQ(u16, read_u16(), 20);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0x0021);
		UASSERTEQ(u32, read_u32(), crc32);
		UASSERTEQ(u32, read_u32(), static_cast<u32>(file_data.size()));
		UASSERTEQ(u32, read_u32(), static_cast<u32>(file_data.size()));
		UASSERTEQ(u16, read_u16(), static_cast<u16>(archive_path.size()));
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u16, read_u16(), 0);
		UASSERTEQ(u32, read_u32(), 0);
		UASSERTEQ(u32, read_u32(), local_header_offset);
		UASSERTEQ(auto, read_string(archive_path.size()), archive_path);
	};

	// createZipFile sorts entries by archive path before writing.
	expect_local_file("file.txt", top_level_data, 0x3610a686, 0);
	expect_local_file("sub/nested.txt", nested_data, 0xb8428ee9, 43);

	const size_t central_directory_offset = position;
	expect_central_directory_entry("file.txt", top_level_data, 0x3610a686, 0);
	expect_central_directory_entry("sub/nested.txt", nested_data, 0xb8428ee9, 43);
	const u32 central_directory_size = static_cast<u32>(position - central_directory_offset);

	UASSERTEQ(u32, read_u32(), 0x06054b50);
	UASSERTEQ(u16, read_u16(), 0);
	UASSERTEQ(u16, read_u16(), 0);
	UASSERTEQ(u16, read_u16(), 2);
	UASSERTEQ(u16, read_u16(), 2);
	UASSERTEQ(u32, read_u32(), central_directory_size);
	UASSERTEQ(u32, read_u32(), static_cast<u32>(central_directory_offset));
	UASSERTEQ(u16, read_u16(), 0);
	UASSERTEQ(size_t, position, contents.size());

	fs::DeleteSingleFileOrEmptyDirectory(zip_path);
	fs::RecursiveDelete(dir_path);
}
#endif

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