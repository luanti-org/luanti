// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <sstream>

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
	void testRemoveLastPathComponent();
	void testRemoveLastPathComponentWithTrailingDelimiter();
	void testRemoveRelativePathComponent();
	void testAbsolutePath();
	void testSafeWriteToFile();
	void testCopyFileContents();
	void testNonExist();
	void testRecursiveDelete();
};

static TestFileSys g_test_instance;

void TestFileSys::runTests(IGameDef *gamedef)
{
	TEST(testIsDirDelimiter);
	TEST(testPathStartsWith);
	TEST(testRemoveLastPathComponent);
	TEST(testRemoveLastPathComponentWithTrailingDelimiter);
	TEST(testRemoveRelativePathComponent);
	TEST(testAbsolutePath);
	TEST(testSafeWriteToFile);
	TEST(testCopyFileContents);
	TEST(testNonExist);
	TEST(testRecursiveDelete);
}

////////////////////////////////////////////////////////////////////////////////

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
		path = "C:" + path;
	#endif

	return path;
}


void TestFileSys::testIsDirDelimiter()
{
	UASSERT(fs::IsDirDelimiter('/') == true);
	UASSERT(fs::IsDirDelimiter('A') == false);
	UASSERT(fs::IsDirDelimiter(0) == false);
#ifdef _WIN32
	UASSERT(fs::IsDirDelimiter('\\') == true);
#else
	UASSERT(fs::IsDirDelimiter('\\') == false);
#endif
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
		/*verbosestream<<"testing fs::PathStartsWith(\""
			<<paths[i]<<"\", \""
			<<paths[j]<<"\")"<<std::endl;*/
		bool starts = fs::PathStartsWith(paths[i], paths[j]);
		int expected = expected_results[i][j];
		if(expected == 0){
			UASSERT(starts == false);
		}
		else if(expected == 1){
			UASSERT(starts == true);
		}
		#ifdef _WIN32
		else if(expected == 2){
			UASSERT(starts == false);
		}
		else if(expected == 3){
			UASSERT(starts == true);
		}
		#else
		else if(expected == 2){
			UASSERT(starts == true);
		}
		else if(expected == 3){
			UASSERT(starts == false);
		}
		#endif
		else if(expected == 4){
			UASSERT(starts == (bool)FILESYS_CASE_INSENSITIVE);
		}
	}
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
#ifdef _WIN32
	UASSERT(result == "C:");
#else
	UASSERT(result == "");
#endif
	UASSERT(removed == p("home/user/minetest/bin/../worlds/world1"));
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
#ifdef _WIN32
	UASSERT(result == "C:");
#else
	UASSERT(result == "");
#endif
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
	const std::string dest_path = getTestTempFile();
	const std::string test_data("hello\0world", 11);
	fs::safeWriteToFile(dest_path, test_data);
	UASSERT(fs::PathExists(dest_path));
	std::string contents_actual;
	UASSERT(fs::ReadFile(dest_path, contents_actual));
	UASSERTEQ(auto, contents_actual, test_data);
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
}
