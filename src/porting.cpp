// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

/*
	Random portability stuff

	See comments in porting.h
*/

#include "porting.h"

#if defined(__FreeBSD__)  || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
	#include <sys/types.h>
	#include <sys/sysctl.h>
	extern char **environ;
#elif defined(_WIN32)
	#include <windows.h>
	#include <wincrypt.h>
	#include <algorithm>
	#include <shlwapi.h>
	#include <shellapi.h>
	#include <mmsystem.h>
#endif
#if !defined(_WIN32)
	#include <unistd.h>
	#include <sys/utsname.h>
	#if !defined(__ANDROID__)
		#include <spawn.h>
	#endif
#endif
#if defined(__hpux)
	#define _PSTAT64
	#include <sys/pstat.h>
#endif
#if defined(__ANDROID__)
	#include "porting_android.h"
	#include <android/api-level.h>
#endif
#if defined(__APPLE__)
	#include <mach-o/dyld.h>
	#include <CoreFoundation/CoreFoundation.h>
	// For _NSGetEnviron()
	// Related: https://gitlab.haskell.org/ghc/ghc/issues/2458
	#include <crt_externs.h>
#endif

#if defined(__HAIKU__)
	#include <FindDirectory.h>
#endif

#if HAVE_MALLOC_TRIM
	// glibc-only pretty much
	#include <malloc.h>
#endif

#include "debug.h"
#include "filesys.h"
#include "log.h"
#include "util/string.h"
#include "util/tracy_wrapper.h"
#include <vector>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <signal.h>
#include <atomic>

#if CHECK_CLIENT_BUILD() && defined(_WIN32)
// On Windows export some driver-specific variables to encourage Minetest to be
// executed on the discrete GPU in case of systems with two. Portability is fun.
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace porting
{

/*
	Signal handler (grabs Ctrl-C on POSIX systems)
*/

volatile static std::sig_atomic_t g_killed = false;

volatile std::sig_atomic_t *signal_handler_killstatus()
{
	return &g_killed;
}

#if !defined(_WIN32) // POSIX
#define STDERR_FILENO 2

static void signal_handler(int sig)
{
	if (!g_killed) {
		if (sig == SIGINT) {
			const char *dbg_text{"INFO: signal_handler(): "
				"Ctrl-C pressed, shutting down.\n"};
			write(STDERR_FILENO, dbg_text, strlen(dbg_text));
		} else if (sig == SIGTERM) {
			const char *dbg_text{"INFO: signal_handler(): "
				"got SIGTERM, shutting down.\n"};
			write(STDERR_FILENO, dbg_text, strlen(dbg_text));
		}
		g_killed = true;
	} else {
		(void)signal(sig, SIG_DFL);
	}
}

void signal_handler_init(void)
{
	(void)signal(SIGINT, signal_handler);
	(void)signal(SIGTERM, signal_handler);
}

#else // _WIN32

static BOOL WINAPI event_handler(DWORD sig)
{
	switch (sig) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (!g_killed) {
			dstream << "INFO: event_handler(): "
				<< "Ctrl+C, Close Event, Logoff Event or Shutdown Event,"
				" shutting down." << std::endl;
			g_killed = true;
		} else {
			(void)signal(SIGINT, SIG_DFL);
		}
		break;
	case CTRL_BREAK_EVENT:
		break;
	}

	return TRUE;
}

void signal_handler_init(void)
{
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)event_handler, TRUE);
}

#endif


/*
	Path mangler
*/

// Nobody should be reading these before initializePaths() is called
std::string path_share = "UNINITIALIZED";
std::string path_user = "UNINITIALIZED";
std::string path_locale = "UNINITIALIZED";
std::string path_cache = "UNINITIALIZED";


std::string getDataPath(const char *subpath)
{
	return path_share + DIR_DELIM + subpath;
}

[[maybe_unused]] static void pathRemoveFile(char *path, char delim)
{
	// Remove filename and path delimiter
	int i;
	for(i = strlen(path)-1; i>=0; i--)
	{
		if(path[i] == delim)
			break;
	}
	path[i] = 0;
}

bool detectMSVCBuildDir(const std::string &path)
{
	const char *ends[] = {
		"bin\\Release",
		"bin\\MinSizeRel",
		"bin\\RelWithDebInfo",
		"bin\\Debug",
		"bin\\Build",
		NULL
	};
	return (!removeStringEnd(path, ends).empty());
}

// Note that the system info is sent in every HTTP request, so keep it reasonably
// privacy-conserving while ideally still being meaningful.

static std::string detectSystemInfo()
{
#ifdef _WIN32
	std::ostringstream oss;
	LPSTR filePath = new char[MAX_PATH];
	UINT blockSize;
	VS_FIXEDFILEINFO *fixedFileInfo;

	GetSystemDirectoryA(filePath, MAX_PATH);
	PathAppendA(filePath, "kernel32.dll");

	DWORD dwVersionSize = GetFileVersionInfoSizeA(filePath, NULL);
	LPBYTE lpVersionInfo = new BYTE[dwVersionSize];

	GetFileVersionInfoA(filePath, 0, dwVersionSize, lpVersionInfo);
	VerQueryValueA(lpVersionInfo, "\\", (LPVOID *)&fixedFileInfo, &blockSize);

	oss << "Windows/"
		<< HIWORD(fixedFileInfo->dwProductVersionMS) << '.' // Major
		<< LOWORD(fixedFileInfo->dwProductVersionMS) << '.' // Minor
		<< HIWORD(fixedFileInfo->dwProductVersionLS) << ' '; // Build

	SYSTEM_INFO info;
	GetNativeSystemInfo(&info);
	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		oss << "x86_64";
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		oss << "arm";
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		oss << "arm64";
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		oss << "x86";
		break;
	default:
		oss << "unknown";
		break;
	}

	delete[] lpVersionInfo;
	delete[] filePath;

	return oss.str();
#elif defined(__ANDROID__)
	std::ostringstream oss;
	struct utsname osinfo;
	uname(&osinfo);
	int api = android_get_device_api_level();

	oss << "Android/" << api << " " << osinfo.machine;
	return oss.str();
#else /* POSIX */
	struct utsname osinfo;
	uname(&osinfo);

	std::string_view release(osinfo.release);
	// cut off anything but the primary version number
	release = release.substr(0, release.find_first_not_of("0123456789."));

	std::string ret = osinfo.sysname;
	ret.append("/").append(release).append(" ").append(osinfo.machine);
	return ret;
#endif
}

const std::string &get_sysinfo()
{
	static std::string ret = detectSystemInfo();
	return ret;
}


[[maybe_unused]] static bool getCurrentWorkingDir(char *buf, size_t len)
{
#ifdef _WIN32
	DWORD ret = GetCurrentDirectory(len, buf);
	return (ret != 0) && (ret <= len);
#else
	return getcwd(buf, len);
#endif
}


static bool getExecPathFromProcfs(char *buf, size_t buflen)
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
	buflen--;

	ssize_t len;
	if ((len = readlink("/proc/self/exe",     buf, buflen)) == -1 &&
		(len = readlink("/proc/curproc/file", buf, buflen)) == -1 &&
		(len = readlink("/proc/curproc/exe",  buf, buflen)) == -1)
		return false;

	buf[len] = '\0';
	return true;
#else
	return false;
#endif
}

//// Windows
#if defined(_WIN32)

bool getCurrentExecPath(char *buf, size_t len)
{
	DWORD written = GetModuleFileNameA(NULL, buf, len);
	if (written == 0 || written == len)
		return false;

	return true;
}


//// Linux
#elif defined(__linux__)

bool getCurrentExecPath(char *buf, size_t len)
{
	if (!getExecPathFromProcfs(buf, len))
		return false;

	return true;
}


//// Mac OS X, Darwin
#elif defined(__APPLE__)

bool getCurrentExecPath(char *buf, size_t len)
{
	uint32_t lenb = (uint32_t)len;
	if (_NSGetExecutablePath(buf, &lenb) == -1)
		return false;

	return true;
}


//// FreeBSD, NetBSD, DragonFlyBSD
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)

bool getCurrentExecPath(char *buf, size_t len)
{
	// Try getting path from procfs first, since valgrind
	// doesn't work with the latter
	if (getExecPathFromProcfs(buf, len))
		return true;

	int mib[4];

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = -1;

	if (sysctl(mib, 4, buf, &len, NULL, 0) == -1)
		return false;

	return true;
}

#elif defined(__HAIKU__)

bool getCurrentExecPath(char *buf, size_t len)
{
	return find_path(B_APP_IMAGE_SYMBOL, B_FIND_PATH_IMAGE_PATH, NULL, buf, len) == B_OK;
}

//// Solaris
#elif defined(__sun) || defined(sun)

bool getCurrentExecPath(char *buf, size_t len)
{
	const char *exec = getexecname();
	if (exec == NULL)
		return false;

	if (strlcpy(buf, exec, len) >= len)
		return false;

	return true;
}


// HP-UX
#elif defined(__hpux)

bool getCurrentExecPath(char *buf, size_t len)
{
	struct pst_status psts;

	if (pstat_getproc(&psts, sizeof(psts), 0, getpid()) == -1)
		return false;

	if (pstat_getpathname(buf, len, &psts.pst_fid_text) == -1)
		return false;

	return true;
}


#else

bool getCurrentExecPath(char *buf, size_t len)
{
	return false;
}

#endif


[[maybe_unused]] static inline const char *getHomeOrFail()
{
	const char *home = getenv("HOME");
	// In rare cases the HOME environment variable may be unset
	FATAL_ERROR_IF(!home,
		"Required environment variable HOME is not set");
	return home;
}


//// Windows
#if defined(_WIN32)

bool setSystemPaths()
{
	char buf[BUFSIZ];

	// Find path of executable and set path_share relative to it
	FATAL_ERROR_IF(!getCurrentExecPath(buf, sizeof(buf)),
		"Failed to get current executable path");
	pathRemoveFile(buf, '\\');

	std::string exepath(buf);

	// Use ".\bin\.."
	path_share = exepath + "\\..";
	if (detectMSVCBuildDir(exepath)) {
		// The msvc build dir schould normaly not be present if properly installed,
		// but its useful for debugging.
		path_share += DIR_DELIM "..";
	}

	// Use %MINETEST_USER_PATH%
	DWORD len = GetEnvironmentVariable("MINETEST_USER_PATH", buf, sizeof(buf));
	FATAL_ERROR_IF(len > sizeof(buf), "Failed to get MINETEST_USER_PATH (too large for buffer)");
	if (len == 0) {
		// Use "C:\Users\<user>\AppData\Roaming\<PROJECT_NAME_C>"
		len = GetEnvironmentVariable("APPDATA", buf, sizeof(buf));
		FATAL_ERROR_IF(len == 0 || len > sizeof(buf), "Failed to get APPDATA");
		// TODO: Luanti with migration
		path_user = std::string(buf) + DIR_DELIM + "Minetest";
	} else {
		path_user = std::string(buf);
	}

	return true;
}


//// Android

#elif defined(__ANDROID__)

extern bool setSystemPaths(); // defined in porting_android.cpp


//// Linux
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)

bool setSystemPaths()
{
	char buf[BUFSIZ];

	if (!getCurrentExecPath(buf, sizeof(buf))) {
		FATAL_ERROR("Unable to read bindir");
		return false;
	}

	pathRemoveFile(buf, '/');
	std::string bindir(buf);

	// Find share directory from these.
	// It is identified by containing the subdirectory "builtin".
	std::vector<std::string> trylist;
	std::string static_sharedir = STATIC_SHAREDIR;
	if (!static_sharedir.empty() && static_sharedir != ".")
		trylist.push_back(static_sharedir);

	trylist.push_back(bindir + DIR_DELIM ".." DIR_DELIM "share"
		DIR_DELIM + PROJECT_NAME);
	trylist.push_back(bindir + DIR_DELIM "..");

	for (auto i = trylist.begin(); i != trylist.end(); ++i) {
		const std::string &trypath = *i;
		if (!fs::PathExists(trypath) ||
			!fs::PathExists(trypath + DIR_DELIM + "builtin")) {
			warningstream << "system-wide share not found at \""
					<< trypath << "\""<< std::endl;
			continue;
		}

		// Warn if was not the first alternative
		if (i != trylist.begin()) {
			warningstream << "system-wide share found at \""
					<< trypath << "\"" << std::endl;
		}

		path_share = trypath;
		break;
	}

	const char *const minetest_user_path = getenv("MINETEST_USER_PATH");
	if (minetest_user_path && minetest_user_path[0] != '\0') {
		path_user = std::string(minetest_user_path);
	} else {
		// TODO: luanti with migration
		path_user = std::string(getHomeOrFail()) + DIR_DELIM "."
			+ "minetest";
	}

	return true;
}


//// Mac OS X
#elif defined(__APPLE__)

bool setSystemPaths()
{
	CFBundleRef main_bundle = CFBundleGetMainBundle();
	CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
	char path[PATH_MAX];
	if (CFURLGetFileSystemRepresentation(resources_url,
			TRUE, (UInt8 *)path, PATH_MAX)) {
		path_share = std::string(path);
	} else {
		warningstream << "Could not determine bundle resource path" << std::endl;
	}
	CFRelease(resources_url);

	const char *const minetest_user_path = getenv("MINETEST_USER_PATH");
	if (minetest_user_path && minetest_user_path[0] != '\0') {
		path_user = std::string(minetest_user_path);
	} else {
		// TODO: luanti with migration
		path_user = std::string(getHomeOrFail())
			+ "/Library/Application Support/"
			+ "minetest";
	}
	return true;
}


#else

bool setSystemPaths()
{
	path_share = STATIC_SHAREDIR;
	const char *const minetest_user_path = getenv("MINETEST_USER_PATH");
	if (minetest_user_path && minetest_user_path[0] != '\0') {
		path_user = std::string(minetest_user_path);
	} else {
		// TODO: luanti with migration
		path_user  = std::string(getHomeOrFail()) + DIR_DELIM "."
			+ "minetest";
	}
	return true;
}


#endif

// Move cache folder from path_user to system cache location if possible.
[[maybe_unused]] static void migrateCachePath()
{
	const std::string local_cache_path = path_user + DIR_DELIM + "cache";

	// Delete tmp folder if it exists (it only ever contained
	// a temporary ogg file, which is no longer used).
	if (fs::PathExists(local_cache_path + DIR_DELIM + "tmp"))
		fs::RecursiveDelete(local_cache_path + DIR_DELIM + "tmp");

	// Bail if migration impossible
	if (path_cache == local_cache_path || !fs::PathExists(local_cache_path)
			|| fs::PathExists(path_cache)) {
		return;
	}
	if (!fs::Rename(local_cache_path, path_cache)) {
		errorstream << "Failed to migrate local cache path "
			"to system path!" << std::endl;
	}
}

// Create tag in cache folder according to <https://bford.info/cachedir/> spec
static void createCacheDirTag()
{
	const auto path = path_cache + DIR_DELIM + "CACHEDIR.TAG";

	if (fs::PathExists(path))
		return;
	fs::CreateAllDirs(path_cache);
	auto ofs = open_ofstream(path.c_str(), false);
	if (!ofs.good())
		return;
	ofs << "Signature: 8a477f597d28d172789f06886806bc55\n"
		"# This file is a cache directory tag automatically created by "
		PROJECT_NAME_C ".\n"
		"# For information about cache directory tags, see: "
		"https://bford.info/cachedir/\n";
}

void initializePaths()
{
#if RUN_IN_PLACE
	infostream << "Using relative paths (RUN_IN_PLACE)" << std::endl;

	char buf[BUFSIZ];
	bool success =
		getCurrentExecPath(buf, sizeof(buf)) ||
		getExecPathFromProcfs(buf, sizeof(buf));

	if (success) {
		pathRemoveFile(buf, DIR_DELIM_CHAR);
		std::string execpath(buf);

		path_share = execpath + DIR_DELIM "..";
		path_user  = execpath + DIR_DELIM "..";

		if (detectMSVCBuildDir(execpath)) {
			path_share += DIR_DELIM "..";
			path_user  += DIR_DELIM "..";
		}
	} else {
		errorstream << "Failed to get paths by executable location, "
			"trying cwd" << std::endl;

		if (!getCurrentWorkingDir(buf, sizeof(buf)))
			FATAL_ERROR("Ran out of methods to get paths");

		size_t cwdlen = strlen(buf);
		if (cwdlen >= 1 && buf[cwdlen - 1] == DIR_DELIM_CHAR) {
			cwdlen--;
			buf[cwdlen] = '\0';
		}

		if (cwdlen >= 4 && !strcmp(buf + cwdlen - 4, DIR_DELIM "bin"))
			pathRemoveFile(buf, DIR_DELIM_CHAR);

		std::string execpath(buf);

		path_share = execpath;
		path_user  = execpath;
	}
	path_cache = path_user + DIR_DELIM + "cache";

#else
	infostream << "Using system-wide paths (NOT RUN_IN_PLACE)" << std::endl;

	if (!setSystemPaths())
		errorstream << "Failed to get one or more system-wide path" << std::endl;

#  ifdef __ANDROID__
	sanity_check(!path_cache.empty());
#  elif defined(_WIN32)
	path_cache = path_user + DIR_DELIM + "cache";
#  else
	// First try $XDG_CACHE_HOME/PROJECT_NAME
	const char *cache_dir = getenv("XDG_CACHE_HOME");
	const char *home_dir = getenv("HOME");
	if (cache_dir && cache_dir[0] != '\0') {
		// TODO: luanti with migration
		path_cache = std::string(cache_dir) + DIR_DELIM + "minetest";
	} else if (home_dir) {
		// Then try $HOME/.cache/PROJECT_NAME
		// TODO: luanti with migration
		path_cache = std::string(home_dir) + DIR_DELIM + ".cache"
			+ DIR_DELIM + "minetest";
	} else {
		// If neither works, use $PATH_USER/cache
		path_cache = path_user + DIR_DELIM + "cache";
	}
#  endif // _WIN32

	// Migrate cache folder to new location if possible
	migrateCachePath();

#endif // RUN_IN_PLACE

	assert(!path_share.empty());
	assert(!path_user.empty());
	assert(!path_cache.empty());

	infostream << "Detected share path: " << path_share << std::endl;
	infostream << "Detected user path: " << path_user << std::endl;
	infostream << "Detected cache path: " << path_cache << std::endl;

	createCacheDirTag();

#if USE_GETTEXT
	bool found_localedir = false;
#  ifdef STATIC_LOCALEDIR
	/* STATIC_LOCALEDIR may be a generalized path such as /usr/share/locale that
	 * doesn't necessarily contain our locale files, so check data path first. */
	path_locale = getDataPath("locale");
	if (fs::PathExists(path_locale)) {
		found_localedir = true;
		infostream << "Using in-place locale directory " << path_locale
			<< " even though a static one was provided." << std::endl;
	} else if (STATIC_LOCALEDIR[0] && fs::PathExists(STATIC_LOCALEDIR)) {
		found_localedir = true;
		path_locale = STATIC_LOCALEDIR;
		infostream << "Using static locale directory " << STATIC_LOCALEDIR
			<< std::endl;
	}
#  else
	path_locale = getDataPath("locale");
	if (fs::PathExists(path_locale)) {
		found_localedir = true;
	}
#  endif
	if (!found_localedir) {
		warningstream << "Couldn't find a locale directory!" << std::endl;
	}
#endif  // USE_GETTEXT
}

////
//// OS-specific Secure Random
////

#ifdef WIN32

bool secure_rand_fill_buf(void *buf, size_t len)
{
	HCRYPTPROV wctx;

	if (!CryptAcquireContext(&wctx, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		return false;

	CryptGenRandom(wctx, len, (BYTE *)buf);
	CryptReleaseContext(wctx, 0);
	return true;
}

#else

bool secure_rand_fill_buf(void *buf, size_t len)
{
	// N.B.  This function checks *only* for /dev/urandom, because on most
	// common OSes it is non-blocking, whereas /dev/random is blocking, and it
	// is exceptionally uncommon for there to be a situation where /dev/random
	// exists but /dev/urandom does not.  This guesswork is necessary since
	// random devices are not covered by any POSIX standard...
	FILE *fp = fopen("/dev/urandom", "rb");
	if (!fp)
		return false;

	bool success = fread(buf, len, 1, fp) == 1;

	fclose(fp);
	return success;
}

#endif

#ifndef __ANDROID__

void osSpecificInit()
{
#ifdef _WIN32
	// hardening options
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
	SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE |
		BASE_SEARCH_PATH_PERMANENT);
	SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
#endif
}

#endif

void attachOrCreateConsole()
{
#ifdef _WIN32
	static bool once = false;
	const bool redirected = _fileno(stdout) >= 0; // If output is redirected to e.g a file
	if (!once && !redirected) {
		if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);
		}
		once = true;
	}
#endif
}

#ifdef _WIN32
std::string QuoteArgv(const std::string &arg)
{
	// Quoting rules on Windows are batshit insane, can differ between applications
	// and there isn't even a stdlib function to deal with it.
	// Ref: https://learn.microsoft.com/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
	if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos)
		return arg;

	std::string ret;
	ret.reserve(arg.size()+2);
	ret.push_back('"');
	for (auto it = arg.begin(); it != arg.end(); ++it) {
		u32 back = 0;
		while (it != arg.end() && *it == '\\')
			++back, ++it;

		if (it == arg.end()) {
			ret.append(2 * back, '\\');
			break;
		} else if (*it == '"') {
			ret.append(2 * back + 1, '\\');
		} else {
			ret.append(back, '\\');
		}
		ret.push_back(*it);
	}
	ret.push_back('"');
	return ret;
}

std::string ConvertError(DWORD error_code)
{
	wchar_t buffer[320];

	auto r = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, error_code, 0, buffer, ARRLEN(buffer) - 1, nullptr);
	if (!r)
		return std::to_string(error_code);

	if (!buffer[0]) // should not happen normally
		return "?";
	return wide_to_utf8(buffer);
}
#endif

int mt_snprintf(char *buf, const size_t buf_size, const char *fmt, ...)
{
	// https://msdn.microsoft.com/en-us/library/bt7tawza.aspx
	//  Many of the MSVC / Windows printf-style functions do not support positional
	//  arguments (eg. "%1$s"). We just forward the call to vsnprintf for sane
	//  platforms, but defer to _vsprintf_p on MSVC / Windows.
	// https://github.com/FFmpeg/FFmpeg/blob/5ae9fa13f5ac640bec113120d540f70971aa635d/compat/msvcrt/snprintf.c#L46
	//  _vsprintf_p has to be shimmed with _vscprintf_p on -1 (for an example see
	//  above FFmpeg link).
	va_list args;
	va_start(args, fmt);
#ifndef _MSC_VER
	int c = vsnprintf(buf, buf_size, fmt, args);
#else  // _MSC_VER
	int c = _vsprintf_p(buf, buf_size, fmt, args);
	if (c == -1)
		c = _vscprintf_p(fmt, args);
#endif // _MSC_VER
	va_end(args);
	return c;
}

#ifdef __ANDROID__
// defined in porting_android.cpp
extern void openURIAndroid(const char *url);
#endif

static bool open_uri(const std::string &uri)
{
	if (uri.find_first_of("\r\n") != std::string::npos) {
		errorstream << "Unable to open URI as it is invalid, contains new line: " << uri << std::endl;
		return false;
	}

#if defined(_WIN32)
	return (intptr_t)ShellExecuteA(NULL, NULL, uri.c_str(), NULL, NULL, SW_SHOWNORMAL) > 32;
#elif defined(__ANDROID__)
	openURIAndroid(uri.c_str());
	return true;
#elif defined(__APPLE__)
	const char *argv[] = {"open", uri.c_str(), NULL};
	return posix_spawnp(NULL, "open", NULL, NULL, (char**)argv,
		(*_NSGetEnviron())) == 0;
#else
	const char *argv[] = {"xdg-open", uri.c_str(), NULL};
	return posix_spawnp(NULL, "xdg-open", NULL, NULL, (char**)argv, environ) == 0;
#endif
}

bool open_url(const std::string &url)
{
	if (!str_starts_with(url, "http://") && !str_starts_with(url, "https://")) {
		errorstream << "Unable to open browser as URL is missing schema: " << url << std::endl;
		return false;
	}

	return open_uri(url);
}

bool open_directory(const std::string &path)
{
	if (!fs::IsDir(path)) {
		errorstream << "Unable to open directory as it does not exist: " << path << std::endl;
		return false;
	}

	return open_uri(path);
}

// Load performance counter frequency only once at startup
#ifdef _WIN32

inline double get_perf_freq()
{
	// Also use this opportunity to enable high-res timers
	timeBeginPeriod(1);

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	return freq.QuadPart;
}

double perf_freq = get_perf_freq();

#endif

#if HAVE_MALLOC_TRIM

/*
 * On Linux/glibc we found that after deallocating bigger chunks of data (esp. MapBlocks)
 * the memory would not be given back to the OS and would stay at peak usage.
 * This appears to be a combination of unfortunate allocation order/fragmentation
 * and the fact that glibc does not call madvise(MADV_DONTNEED) on its own.
 * Some other allocators were also affected, jemalloc and musl libc were not.
 * read more: <https://forum.luanti.org/viewtopic.php?t=30509>
 *
 * As a workaround we track freed memory coarsely and call malloc_trim() once a
 * certain amount is reached.
 *
 * Because trimming can take more than 10ms and would cause jitter if done
 * uncontrolled we have a separate function, which is called from background threads.
 */

static std::atomic<size_t> memory_freed;

constexpr size_t MEMORY_TRIM_THRESHOLD = 256 * 1024 * 1024;

void TrackFreedMemory(size_t amount)
{
	memory_freed.fetch_add(amount, std::memory_order_relaxed);
}

void TriggerMemoryTrim()
{
	ZoneScoped;

	constexpr auto MO = std::memory_order_relaxed;
	if (memory_freed.load(MO) >= MEMORY_TRIM_THRESHOLD) {
		// Synchronize call
		if (memory_freed.exchange(0, MO) < MEMORY_TRIM_THRESHOLD)
			return;
		// Leave some headroom for future allocations
		malloc_trim(8 * 1024 * 1024);
	}
}

#endif

} //namespace porting
