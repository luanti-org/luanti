// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "irrlichttypes_bloated.h"
#include "irrlicht.h" // createDevice
#include "irrlicht_changes/printing.h"
#include "benchmark/benchmark.h"
#include "chat_interface.h"
#include "debug.h"
#include "unittest/test.h"
#include "server.h"
#include "filesys.h"
#include "version.h"
#include "defaultsettings.h"
#include "migratesettings.h"
#include "gettext.h"
#include "log.h"
#include "log_internal.h"
#include "util/quicktune.h"
#include "httpfetch.h"
#include "gameparams.h"
#include "database/database.h"
#include "config.h"
#include "player.h"
#include "porting.h"
#include "serialization.h" // SER_FMT_VER_HIGHEST_*
#include "network/socket.h"
#include "mapblock.h"
#if USE_CURSES
	#include "terminal_chat_console.h"
#endif
#if CHECK_CLIENT_BUILD()
#include "gui/guiMainMenu.h"
#include "client/clientlauncher.h"
#include "gui/guiEngine.h"
#include "gui/mainmenumanager.h"
#endif

// for version information only
extern "C" {
#if USE_LUAJIT
	#include <luajit.h>
#else
	#include <lua.h>
#endif
}

#if !defined(__cpp_rtti) || !defined(__cpp_exceptions)
#error Luanti cannot be built without exceptions or RTTI
#endif

#if defined(__MINGW32__) && !defined(__clang__)
// see <https://github.com/luanti-org/luanti/issues/14140> or
// <https://github.com/luanti-org/luanti/issues/10137> for some of the issues we had
#error ==================================
#error MinGW gcc has a broken TLS implementation and is not supported for building \
	Luanti. Look at testTLS() in test_threading.cpp and see for yourself. \
	Please use a clang-based compiler or alternatively MSVC.
#error ==================================
#endif

// TODO: luanti.conf with migration
#define CONFIGFILE "minetest.conf"
#define DEBUGFILE "debug.txt"
#define DEFAULT_SERVER_PORT 30000

#define ENV_NO_COLOR "NO_COLOR"
#define ENV_CLICOLOR "CLICOLOR"
#define ENV_CLICOLOR_FORCE "CLICOLOR_FORCE"

typedef std::map<std::string, ValueSpec> OptionList;

/**********************************************************************
 * Private functions
 **********************************************************************/

static void get_env_opts(Settings &args);
static bool get_cmdline_opts(int argc, char *argv[], Settings *cmd_args);
static void set_allowed_options(OptionList *allowed_options);

static void print_help(const OptionList &allowed_options);
static void print_allowed_options(const OptionList &allowed_options);
static void print_version(std::ostream &os);
static void print_worldspecs(const std::vector<WorldSpec> &worldspecs,
	std::ostream &os, bool print_name = true, bool print_path = true);
static void print_modified_quicktune_values();

static void list_game_ids();
static void list_worlds(bool print_name, bool print_path);
static bool setup_log_params(const Settings &cmd_args);
static bool create_userdata_path();
static bool use_debugger(int argc, char *argv[]);
static bool init_common(const Settings &cmd_args, int argc, char *argv[]);
static void uninit_common();
static void startup_message();
static bool read_config_file(const Settings &cmd_args);
static void init_log_streams(const Settings &cmd_args);

static bool game_configure(GameParams *game_params, const Settings &cmd_args);
static void game_configure_port(GameParams *game_params, const Settings &cmd_args);

static bool game_configure_world(GameParams *game_params, const Settings &cmd_args);
static bool get_world_from_cmdline(GameParams *game_params, const Settings &cmd_args);
static bool get_world_from_config(GameParams *game_params, const Settings &cmd_args);
static bool auto_select_world(GameParams *game_params);
static std::string get_clean_world_path(const std::string &path);

static bool game_configure_subgame(GameParams *game_params, const Settings &cmd_args);
static bool get_game_from_cmdline(GameParams *game_params, const Settings &cmd_args);
static bool determine_subgame(GameParams *game_params);

static bool run_dedicated_server(const GameParams &game_params, const Settings &cmd_args);
static bool migrate_map_database(const GameParams &game_params, const Settings &cmd_args);
static bool recompress_map_database(const GameParams &game_params, const Settings &cmd_args);

/**********************************************************************/


static FileLogOutput file_log_output;

static OptionList allowed_options;

int main(int argc, char *argv[])
{
	int retval;
	debug_set_exception_handler();

	g_logger.registerThread("Main");
	g_logger.addOutputMaxLevel(&stderr_output, LL_ACTION);

	porting::osSpecificInit();

	Settings cmd_args;
	get_env_opts(cmd_args);
	bool cmd_args_ok = get_cmdline_opts(argc, argv, &cmd_args);
	if (!cmd_args_ok
			|| cmd_args.getFlag("help")
			|| cmd_args.exists("nonopt1")) {
		porting::attachOrCreateConsole();
		print_help(allowed_options);
		return cmd_args_ok ? 0 : 1;
	}
	if (cmd_args.getFlag("console"))
		porting::attachOrCreateConsole();

	if (cmd_args.getFlag("version")) {
		porting::attachOrCreateConsole();
		print_version(std::cout);
		return 0;
	}

	// Debug handler
	BEGIN_DEBUG_EXCEPTION_HANDLER

	if (!setup_log_params(cmd_args))
		return 1;

	if (cmd_args.getFlag("debugger")) {
		if (!use_debugger(argc, argv))
			warningstream << "Continuing without debugger" << std::endl;
	}

	{
		auto exe_name = argc > 0 ? lowercase(fs::GetFilenameFromPath(argv[0])) : "";
		if (str_starts_with(exe_name, "minetest")) {
#if CHECK_CLIENT_BUILD()
			const char *new_ = PROJECT_NAME;
#else
			const char *new_ = PROJECT_NAME "server";
#endif
			warningstream << "The executable " << exe_name
				<< " is a deprecated alias, please use " << new_ << " instead." << std::endl;
		}
	}

	porting::signal_handler_init();
	porting::initializePaths();

	if (!create_userdata_path()) {
		errorstream << "Cannot create user data directory" << std::endl;
		return 1;
	}

	// List gameids if requested
	if (cmd_args.exists("gameid") && cmd_args.get("gameid") == "list") {
		list_game_ids();
		return 0;
	}

	// List worlds, world names, and world paths if requested
	if (cmd_args.exists("worldlist")) {
		if (cmd_args.get("worldlist") == "name") {
			list_worlds(true, false);
		} else if (cmd_args.get("worldlist") == "path") {
			list_worlds(false, true);
		} else if (cmd_args.get("worldlist") == "both") {
			list_worlds(true, true);
		} else {
			errorstream << "Invalid --worldlist value: "
				<< cmd_args.get("worldlist") << std::endl;
			return 1;
		}
		return 0;
	}

	if (!init_common(cmd_args, argc, argv))
		return 1;

	if (g_settings->getBool("enable_console"))
		porting::attachOrCreateConsole();

	// Run unit tests
	if (cmd_args.getFlag("run-unittests")) {
		porting::attachOrCreateConsole();
#if BUILD_UNITTESTS
		if (cmd_args.exists("test-module"))
			return run_tests(cmd_args.get("test-module")) ? 0 : 1;
		else
			return run_tests() ? 0 : 1;
#else
		errorstream << "Unittest support is not enabled in this binary. "
			<< "If you want to enable it, compile project with BUILD_UNITTESTS=1 flag."
			<< std::endl;
		return 1;
#endif
	}

	// Run benchmarks
	if (cmd_args.getFlag("run-benchmarks")) {
		porting::attachOrCreateConsole();
#if BUILD_BENCHMARKS
		if (cmd_args.exists("test-module"))
			return run_benchmarks(cmd_args.get("test-module").c_str()) ? 0 : 1;
		else
			return run_benchmarks() ? 0 : 1;
#else
		errorstream << "Benchmark support is not enabled in this binary. "
			<< "If you want to enable it, compile project with BUILD_BENCHMARKS=1 flag."
			<< std::endl;
		return 1;
#endif
	}

	GameStartData game_params;
#if !CHECK_CLIENT_BUILD()
	porting::attachOrCreateConsole();
	game_params.is_dedicated_server = true;
#else
	const bool isServer = cmd_args.getFlag("server");
	if (isServer)
		porting::attachOrCreateConsole();
	game_params.is_dedicated_server = isServer;
#endif

	if (!game_configure(&game_params, cmd_args))
		return 1;

	sanity_check(!game_params.world_path.empty());

	if (game_params.is_dedicated_server)
		return run_dedicated_server(game_params, cmd_args) ? 0 : 1;

#if CHECK_CLIENT_BUILD()
	retval = ClientLauncher().run(game_params, cmd_args) ? 0 : 1;
#else
	retval = 0;
#endif

	// Update configuration file
	if (!g_settings_path.empty())
		g_settings->updateConfigFile(g_settings_path.c_str());

	print_modified_quicktune_values();

	END_DEBUG_EXCEPTION_HANDLER

	return retval;
}


/*****************************************************************************
 * Startup / Init
 *****************************************************************************/


static void get_env_opts(Settings &args)
{
	// CLICOLOR is a de-facto standard option for colors <https://bixense.com/clicolors/>
	// CLICOLOR != 0: ANSI colors are supported (auto-detection, this is the default)
	// CLICOLOR == 0: ANSI colors are NOT supported
	const char *clicolor = std::getenv(ENV_CLICOLOR);
	if (clicolor && std::string(clicolor) == "0") {
		args.set("color", "never");
	}
	// NO_COLOR only specifies that no color is allowed.
	// Implemented according to <http://no-color.org/>
	const char *no_color = std::getenv(ENV_NO_COLOR);
	if (no_color && no_color[0]) {
		args.set("color", "never");
	}
	// CLICOLOR_FORCE is another option, which should turn on colors "no matter what".
	const char *clicolor_force = std::getenv(ENV_CLICOLOR_FORCE);
	if (clicolor_force && std::string(clicolor_force) != "0") {
		// should ALWAYS have colors, so we ignore tty (no "auto")
		args.set("color", "always");
	}
}

static bool get_cmdline_opts(int argc, char *argv[], Settings *cmd_args)
{
	set_allowed_options(&allowed_options);

	return cmd_args->parseCommandLine(argc, argv, allowed_options);
}

static void set_allowed_options(OptionList *allowed_options)
{
	assert(allowed_options);
	allowed_options->clear();

#if CHECK_CLIENT_BUILD()
#define SERVER_ONLY " (requires --server flag)"
#define LOCAL_GAME " (implies local game if used with option --go)"
#else
#define SERVER_ONLY ""
#define LOCAL_GAME ""
#endif

	allowed_options->insert(std::make_pair("help", ValueSpec(VALUETYPE_FLAG,
			_("Show allowed options"))));
	allowed_options->insert(std::make_pair("version", ValueSpec(VALUETYPE_FLAG,
			_("Show version information"))));
	allowed_options->insert(std::make_pair("config", ValueSpec(VALUETYPE_STRING,
			_("Load configuration from specified file"))));
	allowed_options->insert(std::make_pair("port", ValueSpec(VALUETYPE_STRING,
			_("Set network port (UDP)"))));
	allowed_options->insert(std::make_pair("run-unittests", ValueSpec(VALUETYPE_FLAG,
			_("Run unit tests and exit"))));
	allowed_options->insert(std::make_pair("run-benchmarks", ValueSpec(VALUETYPE_FLAG,
			_("Run benchmarks and exit"))));
	allowed_options->insert(std::make_pair("test-module", ValueSpec(VALUETYPE_STRING,
			_("Only run the specified test module or benchmark"))));
	allowed_options->insert(std::make_pair("map-dir", ValueSpec(VALUETYPE_STRING,
			_("Same as --world (deprecated)"))));
	allowed_options->insert(std::make_pair("world", ValueSpec(VALUETYPE_STRING,
			_("Set world path" LOCAL_GAME))));
	allowed_options->insert(std::make_pair("worldname", ValueSpec(VALUETYPE_STRING,
			_("Set world by name" LOCAL_GAME))));
	allowed_options->insert(std::make_pair("worldlist", ValueSpec(VALUETYPE_STRING,
			_("Get list of worlds ('path' lists paths, "
			"'name' lists names, 'both' lists both)"))));
	allowed_options->insert(std::make_pair("quiet", ValueSpec(VALUETYPE_FLAG,
			_("Print only errors to console"))));
	allowed_options->insert(std::make_pair("color", ValueSpec(VALUETYPE_STRING,
			_("Coloured logs ('always', 'never' or 'auto'), defaults to 'auto'"))));
	allowed_options->insert(std::make_pair("info", ValueSpec(VALUETYPE_FLAG,
			_("Print more information to console"))));
	allowed_options->insert(std::make_pair("verbose",  ValueSpec(VALUETYPE_FLAG,
			_("Print even more information to console"))));
	allowed_options->insert(std::make_pair("trace", ValueSpec(VALUETYPE_FLAG,
			_("Print enormous amounts of information to log and console"))));
	allowed_options->insert(std::make_pair("debugger", ValueSpec(VALUETYPE_FLAG,
			_("Try to automatically attach a debugger before starting (convenience option)"))));
	allowed_options->insert(std::make_pair("logfile", ValueSpec(VALUETYPE_STRING,
			_("Set log file path ('' = no logging)"))));
	allowed_options->insert(std::make_pair("gameid", ValueSpec(VALUETYPE_STRING,
			_("Set gameid (\"--gameid list\" prints available ones)"))));
	allowed_options->insert(std::make_pair("migrate", ValueSpec(VALUETYPE_STRING,
			_("Migrate from current map backend to another" SERVER_ONLY))));
	allowed_options->insert(std::make_pair("migrate-players", ValueSpec(VALUETYPE_STRING,
		_("Migrate from current players backend to another" SERVER_ONLY))));
	allowed_options->insert(std::make_pair("migrate-auth", ValueSpec(VALUETYPE_STRING,
		_("Migrate from current auth backend to another" SERVER_ONLY))));
	allowed_options->insert(std::make_pair("migrate-mod-storage", ValueSpec(VALUETYPE_STRING,
		_("Migrate from current mod storage backend to another" SERVER_ONLY))));
	allowed_options->insert(std::make_pair("terminal", ValueSpec(VALUETYPE_FLAG,
			_("Enable ncurses interactive terminal" SERVER_ONLY))));
	allowed_options->insert(std::make_pair("recompress", ValueSpec(VALUETYPE_FLAG,
			_("Recompress the blocks of the given map database" SERVER_ONLY))));
#if CHECK_CLIENT_BUILD()
	allowed_options->insert(std::make_pair("address", ValueSpec(VALUETYPE_STRING,
			_("Address to connect to ('' = local game)"))));
	allowed_options->insert(std::make_pair("random-input", ValueSpec(VALUETYPE_FLAG,
			_("Enable random user input (for testing)"))));
	allowed_options->insert(std::make_pair("server", ValueSpec(VALUETYPE_FLAG,
			_("Behave as dedicated server"))));
	allowed_options->insert(std::make_pair("name", ValueSpec(VALUETYPE_STRING,
			_("Set player name"))));
	allowed_options->insert(std::make_pair("password", ValueSpec(VALUETYPE_STRING,
			_("Set password"))));
	allowed_options->insert(std::make_pair("password-file", ValueSpec(VALUETYPE_STRING,
			_("Set password from contents of file"))));
	allowed_options->insert(std::make_pair("go", ValueSpec(VALUETYPE_FLAG,
			_("Skip main menu, go directly in-game"))));
	allowed_options->insert(std::make_pair("console", ValueSpec(VALUETYPE_FLAG,
			_("Start with the console open (Windows only)"))));
#endif

#undef SERVER_ONLY
#undef LOCAL_GAME
}

static void print_help(const OptionList &allowed_options)
{
	std::cout << _("Allowed options:") << std::endl;
	print_allowed_options(allowed_options);
}

static void print_allowed_options(const OptionList &allowed_options)
{
	for (const auto &allowed_option : allowed_options) {
		std::string opt = "  --" + allowed_option.first;
		if (allowed_option.second.type != VALUETYPE_FLAG)
			opt += _(" <value>");

		std::string opt_padded = padStringRight(opt, 30);
		std::cout << opt_padded;
		if (opt == opt_padded) // Line is too long to pad
			std::cout << std::endl << padStringRight("", 30);

		if (allowed_option.second.help)
			std::cout << allowed_option.second.help;

		std::cout << std::endl;
	}
}

static void print_version(std::ostream &os)
{
	os << PROJECT_NAME_C " " << g_version_hash
		<< " (" << porting::getPlatformName() << ")" << std::endl;
#if USE_LUAJIT
	os << "Using " << LUAJIT_VERSION
#ifdef OPENRESTY_LUAJIT
	<< " (OpenResty)"
#endif
	<< std::endl;
#else
	os << "Using " << LUA_RELEASE << std::endl;
#endif
#if defined(__clang__)
	os << "Built by Clang " << __clang_major__ << "." << __clang_minor__ << std::endl;
#elif defined(__GNUC__)
	os << "Built by GCC " << __GNUC__ << "." << __GNUC_MINOR__ << std::endl;
#elif defined(_MSC_VER)
	os << "Built by MSVC " << (_MSC_VER / 100) << "." << (_MSC_VER % 100) << std::endl;
#endif
	os << "Running on " << porting::get_sysinfo() << std::endl;
	os << g_build_info << std::endl;
}

static void list_game_ids()
{
	std::set<std::string> gameids = getAvailableGameIds();
	for (const std::string &gameid : gameids)
		rawstream << gameid <<std::endl;
}

static void list_worlds(bool print_name, bool print_path)
{
	std::cout << _("Available worlds:") << std::endl;
	std::vector<WorldSpec> worldspecs = getAvailableWorlds();
	print_worldspecs(worldspecs, std::cout, print_name, print_path);
}

static void print_worldspecs(const std::vector<WorldSpec> &worldspecs,
	std::ostream &os, bool print_name, bool print_path)
{
	for (const WorldSpec &worldspec : worldspecs) {
		const auto &name = worldspec.name;
		const auto &path = worldspec.path;
		if (print_name && print_path) {
			os << "\t" << name << "\t\t" << path << std::endl;
		} else if (print_name) {
			os << "\t" << name << std::endl;
		} else if (print_path) {
			os << "\t" << path << std::endl;
		}
	}
}

static void print_modified_quicktune_values()
{
	bool header_printed = false;
	std::vector<std::string> names = getQuicktuneNames();

	for (const std::string &name : names) {
		QuicktuneValue val = getQuicktuneValue(name);
		if (!val.modified)
			continue;
		if (!header_printed) {
			dstream << "Modified quicktune values:" << std::endl;
			header_printed = true;
		}
		dstream << name << " = " << val.getString() << std::endl;
	}
}

static bool setup_log_params(const Settings &cmd_args)
{
	// Quiet mode, print errors only
	if (cmd_args.getFlag("quiet")) {
		g_logger.removeOutput(&stderr_output);
		g_logger.addOutputMaxLevel(&stderr_output, LL_ERROR);
	}

	// Coloured log messages (see log.h)
	std::string color_mode;
	if (cmd_args.exists("color")) {
		color_mode = cmd_args.get("color");
	}
	if (!color_mode.empty()) {
		if (color_mode == "auto") {
			Logger::color_mode = LOG_COLOR_AUTO;
		} else if (color_mode == "always") {
			Logger::color_mode = LOG_COLOR_ALWAYS;
		} else if (color_mode == "never") {
			Logger::color_mode = LOG_COLOR_NEVER;
		} else {
			errorstream << "Invalid color mode: " << color_mode << std::endl;
			return false;
		}
	}

	// In certain cases, output info level on stderr
	if (cmd_args.getFlag("info") || cmd_args.getFlag("verbose") ||
			cmd_args.getFlag("trace") || cmd_args.getFlag("speedtests"))
		g_logger.addOutput(&stderr_output, LL_INFO);

	// In certain cases, output verbose level on stderr
	if (cmd_args.getFlag("verbose") || cmd_args.getFlag("trace"))
		g_logger.addOutput(&stderr_output, LL_VERBOSE);

	if (cmd_args.getFlag("trace")) {
		dstream << _("Enabling trace level debug output") << std::endl;
		g_logger.addOutput(&stderr_output, LL_TRACE);
	}

	return true;
}

static bool create_userdata_path()
{
	bool success;

#ifdef __ANDROID__
	if (!fs::PathExists(porting::path_user)) {
		success = fs::CreateDir(porting::path_user);
	} else {
		success = true;
	}
#else
	// Create user data directory
	success = fs::CreateAllDirs(porting::path_user);
#endif

	return success;
}

namespace {
	[[maybe_unused]] std::string findProgram(const char *name) {
		char *path_c = getenv("PATH");
		if (!path_c)
			return "";
		std::istringstream iss(path_c);
		std::string checkpath;
		while (!iss.eof()) {
			std::getline(iss, checkpath, PATH_DELIM[0]);
			if (!checkpath.empty() && checkpath.back() != DIR_DELIM_CHAR)
				checkpath.push_back(DIR_DELIM_CHAR);
			checkpath.append(name);
			if (fs::IsExecutable(checkpath))
				return checkpath;
		}
		return "";
	}

#ifdef _WIN32
	const char *debuggerNames[] = {"gdb.exe", "lldb.exe"};
#else
	[[maybe_unused]] const char *debuggerNames[] = {"gdb", "lldb"};
#endif

	template <class T>
	void getDebuggerArgs(T &out, int i) {
		if (i == 0) {
			for (auto s : {"-q", "--batch", "-iex", "set confirm off",
				"-ex", "run", "-ex", "bt", "--args"})
				out.push_back(s);
		} else if (i == 1) {
			for (auto s : {"-Q", "-b", "-o", "run", "-k", "bt\nq", "--"})
				out.push_back(s);
		}
	}
}

static bool use_debugger(int argc, char *argv[])
{
#if defined(__ANDROID__)
	return false;
#else
#ifdef _WIN32
	if (IsDebuggerPresent()) {
		warningstream << "Process is already being debugged." << std::endl;
		return false;
	}
#endif

	char exec_path[1024];
	if (!porting::getCurrentExecPath(exec_path, sizeof(exec_path)))
		return false;

	int debugger = -1;
	std::string debugger_path;
	for (u32 i = 0; i < ARRLEN(debuggerNames); i++) {
		debugger_path = findProgram(debuggerNames[i]);
		if (!debugger_path.empty()) {
			debugger = i;
			break;
		}
	}
	if (debugger == -1) {
		warningstream << "Couldn't find a debugger to use. Try installing gdb or lldb." << std::endl;
		return false;
	}
	verbosestream << "Found debugger " << debugger_path << std::endl;

	// Try to be helpful
#ifdef NDEBUG
	if (strcmp(BUILD_TYPE, "RelWithDebInfo") != 0) {
		warningstream << "It looks like your " PROJECT_NAME_C " executable was built without "
			"debug symbols (BUILD_TYPE=" BUILD_TYPE "), so you won't get useful backtraces."
			<< std::endl;
	}
#endif

	std::vector<const char*> new_args;
	new_args.push_back(debugger_path.c_str());
	getDebuggerArgs(new_args, debugger);
	// Copy the existing arguments
	new_args.push_back(exec_path);
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--debugger"))
			continue;
		new_args.push_back(argv[i]);
	}
	new_args.push_back("--console");
	new_args.push_back(nullptr);

#ifdef _WIN32
	// Special treatment for Windows
	std::string cmdline;
	for (int i = 1; new_args[i]; i++) {
		if (i > 1)
			cmdline += ' ';
		cmdline += porting::QuoteArgv(new_args[i]);
	}

	STARTUPINFO startup_info = {};
	PROCESS_INFORMATION process_info = {};
	bool ok = CreateProcess(new_args[0], cmdline.empty() ? nullptr : &cmdline[0],
		nullptr, nullptr, false, CREATE_UNICODE_ENVIRONMENT,
		nullptr, nullptr, &startup_info, &process_info);
	if (!ok) {
		warningstream << "CreateProcess: " << GetLastError() << std::endl;
		return false;
	}
	DWORD exitcode = 0;
	WaitForSingleObject(process_info.hProcess, INFINITE);
	GetExitCodeProcess(process_info.hProcess, &exitcode);
	exit(exitcode);
	// not reached
#else
	errno = 0;
	execv(new_args[0], const_cast<char**>(new_args.data()));
	warningstream << "execv: " << strerror(errno) << std::endl;
	return false;
#endif
#endif
}

static bool init_common(const Settings &cmd_args, int argc, char *argv[])
{
	startup_message();

	sockets_init();

	// Initialize g_settings
	set_default_settings();
	Settings::createLayer(SL_GLOBAL);

	// Set cleanup callback(s) to run at process exit
	atexit(uninit_common);

	if (!read_config_file(cmd_args))
		return false;

	migrate_settings();

	init_log_streams(cmd_args);

	// Initialize random seed
	u64 seed;
	if (!porting::secure_rand_fill_buf(&seed, sizeof(seed))) {
		infostream << "Secure randomness not available to seed global RNG!" << std::endl;
		std::ostringstream oss;
		// stuff that's somewhat unpredictable:
		oss << time(nullptr) << porting::getTimeUs() << argc
			<< g_settings_path << reinterpret_cast<intptr_t>(argv);
		print_version(oss);
		std::string data = oss.str();
		seed = murmur_hash_64_ua(data.c_str(), data.size(), 0xc0ffee);
	}
	srand(seed);
	mysrand(seed);

	// Initialize HTTP fetcher
	httpfetch_init(g_settings->getS32("curl_parallel_limit"));

	init_gettext(porting::path_locale.c_str(),
		g_settings->get("language"), argc, argv);

	return true;
}

static void uninit_common()
{
	httpfetch_cleanup();

	sockets_cleanup();

	// It'd actually be okay to leak these but we want to please valgrind...
	for (int i = 0; i < (int)SL_TOTAL_COUNT; i++)
		delete Settings::getLayer((SettingsLayer)i);
}

static void startup_message()
{
	print_version(infostream);
	infostream << "SER_FMT_VER_HIGHEST_READ=" << (int)SER_FMT_VER_HIGHEST_READ
		<< " LATEST_PROTOCOL_VERSION=" << (int)LATEST_PROTOCOL_VERSION
		<< std::endl;
}

static bool read_config_file(const Settings &cmd_args)
{
	// Path of configuration file in use
	sanity_check(g_settings_path.empty());	// Sanity check

	if (cmd_args.exists("config")) {
		bool r = g_settings->readConfigFile(cmd_args.get("config").c_str());
		if (!r) {
			errorstream << "Could not read configuration from \""
			            << cmd_args.get("config") << "\"" << std::endl;
			return false;
		}
		g_settings_path = cmd_args.get("config");
	} else {
		std::vector<std::string> filenames;
		filenames.push_back(porting::path_user + DIR_DELIM + CONFIGFILE);
		// Legacy configuration file location
		filenames.push_back(porting::path_user +
				DIR_DELIM + ".." + DIR_DELIM + CONFIGFILE);

#if RUN_IN_PLACE
		// Try also from a lower level (to aid having the same configuration
		// for many RUN_IN_PLACE installs)
		filenames.push_back(porting::path_user +
				DIR_DELIM + ".." + DIR_DELIM + ".." + DIR_DELIM + CONFIGFILE);
#endif

		for (const std::string &filename : filenames) {
			bool r = g_settings->readConfigFile(filename.c_str());
			if (r) {
				g_settings_path = filename;
				break;
			}
		}

		// If no path found, use the first one (menu creates the file)
		if (g_settings_path.empty()) {
			g_settings_path = filenames[0];
			g_first_run = true;
		}
	}
	infostream << "Global configuration file: " << g_settings_path
		<< (g_first_run ? " (first run)" : "") << std::endl;

	return true;
}

static void init_log_streams(const Settings &cmd_args)
{
	std::string log_filename = porting::path_user + DIR_DELIM + DEBUGFILE;

	if (cmd_args.exists("logfile"))
		log_filename = cmd_args.get("logfile");

	g_logger.removeOutput(&file_log_output);
	std::string conf_loglev = g_settings->get("debug_log_level");

	if (log_filename.empty() || conf_loglev.empty())  // No logging
		return;

	// Old integer format
	if (std::isdigit(conf_loglev[0])) {
		warningstream << "Deprecated use of debug_log_level with an "
			"integer value; please update your configuration." << std::endl;
		static const char *lev_name[] =
			{"", "error", "action", "info", "verbose", "trace"};
		int lev_i = atoi(conf_loglev.c_str());
		if (lev_i < 0 || lev_i >= (int)ARRLEN(lev_name)) {
			warningstream << "Supplied invalid debug_log_level!"
				"  Assuming action level." << std::endl;
			lev_i = 2;
		}
		conf_loglev = lev_name[lev_i];
	}

	LogLevel log_level = Logger::stringToLevel(conf_loglev);
	if (log_level == LL_MAX) {
		warningstream << "Supplied unrecognized debug_log_level; "
			"using maximum." << std::endl;
	}

	infostream << "Logging to " << log_filename << std::endl;

	file_log_output.setFile(log_filename,
		g_settings->getU64("debug_log_size_max") * 1000000);
	g_logger.addOutputMaxLevel(&file_log_output, log_level);
}

static bool game_configure(GameParams *game_params, const Settings &cmd_args)
{
	game_configure_port(game_params, cmd_args);

	if (!game_configure_world(game_params, cmd_args)) {
		errorstream << "No world path specified or found." << std::endl;
		return false;
	}

	return game_configure_subgame(game_params, cmd_args);
}

static void game_configure_port(GameParams *game_params, const Settings &cmd_args)
{
	if (cmd_args.exists("port")) {
		game_params->socket_port = cmd_args.getU16("port");
	} else {
		if (game_params->is_dedicated_server)
			game_params->socket_port = g_settings->getU16("port");
		else
			game_params->socket_port = g_settings->getU16("remote_port");
	}

	if (game_params->socket_port == 0)
		game_params->socket_port = DEFAULT_SERVER_PORT;
}

static bool game_configure_world(GameParams *game_params, const Settings &cmd_args)
{
	if (get_world_from_cmdline(game_params, cmd_args))
		return true;

	if (get_world_from_config(game_params, cmd_args))
		return true;

	return auto_select_world(game_params);
}

static bool get_world_from_cmdline(GameParams *game_params, const Settings &cmd_args)
{
	std::string commanded_world;

	// World name
	std::string commanded_worldname;
	if (cmd_args.exists("worldname"))
		commanded_worldname = cmd_args.get("worldname");

	// If a world name was specified, convert it to a path
	if (!commanded_worldname.empty()) {
		// Get information about available worlds
		std::vector<WorldSpec> worldspecs = getAvailableWorlds();
		bool found = false;
		for (const WorldSpec &worldspec : worldspecs) {
			std::string name = worldspec.name;
			if (name == commanded_worldname) {
				dstream << "Using world specified by --worldname on the "
					"command line" << std::endl;
				commanded_world = worldspec.path;
				found = true;
				break;
			}
		}
		if (!found) {
			dstream << "World '" << commanded_worldname
			        << "' not available. Available worlds:" << std::endl;
			print_worldspecs(worldspecs, dstream);
			return false;
		}

		game_params->world_path = get_clean_world_path(commanded_world);
		return !commanded_world.empty();
	}

	if (cmd_args.exists("world"))
		commanded_world = cmd_args.get("world");
	else if (cmd_args.exists("map-dir"))
		commanded_world = cmd_args.get("map-dir");
	else if (cmd_args.exists("nonopt0")) // First nameless argument
		commanded_world = cmd_args.get("nonopt0");

	game_params->world_path = get_clean_world_path(commanded_world);
	return !commanded_world.empty();
}

static bool get_world_from_config(GameParams *game_params, const Settings &cmd_args)
{
	// World directory
	std::string commanded_world;

	if (g_settings->exists("map-dir"))
		commanded_world = g_settings->get("map-dir");

	game_params->world_path = get_clean_world_path(commanded_world);

	return !commanded_world.empty();
}

static bool auto_select_world(GameParams *game_params)
{
	// No world was specified; try to select it automatically
	// Get information about available worlds

	std::vector<WorldSpec> worldspecs = getAvailableWorlds();
	std::string world_path;

	// If there is only a single world, use it
	if (worldspecs.size() == 1) {
		world_path = worldspecs[0].path;
		dstream << "Automatically selecting world at ["
		        << world_path << "]" << std::endl;
	// If there are multiple worlds, list them
	} else if (worldspecs.size() > 1 && game_params->is_dedicated_server) {
		rawstream << "Multiple worlds are available.\n"
			<< "Please select one using --worldname <name> or --world <path>"
			<< std::endl;
		print_worldspecs(worldspecs, rawstream);
		return false;
	// If there are no worlds, automatically create a new one
	} else {
		// This is the ultimate default world path
		world_path = porting::path_user + DIR_DELIM + "worlds" +
				DIR_DELIM + "world";
		infostream << "Using default world at ["
		           << world_path << "]" << std::endl;
	}

	assert(!world_path.empty());	// Post-condition
	game_params->world_path = world_path;
	return true;
}

static std::string get_clean_world_path(const std::string &path)
{
	const std::string worldmt = "world.mt";
	std::string clean_path;

	if (path.size() > worldmt.size()
			&& path.substr(path.size() - worldmt.size()) == worldmt) {
		dstream << _("Supplied world.mt file - stripping it off.") << std::endl;
		clean_path = path.substr(0, path.size() - worldmt.size());
	} else {
		clean_path = path;
	}
	return path;
}


static bool game_configure_subgame(GameParams *game_params, const Settings &cmd_args)
{
	bool success;

	success = get_game_from_cmdline(game_params, cmd_args);
	if (!success)
		success = determine_subgame(game_params);

	return success;
}

static bool get_game_from_cmdline(GameParams *game_params, const Settings &cmd_args)
{
	SubgameSpec commanded_gamespec;

	if (cmd_args.exists("gameid")) {
		std::string gameid = cmd_args.get("gameid");
		commanded_gamespec = findSubgame(gameid);
		if (!commanded_gamespec.isValid()) {
			errorstream << "Game \"" << gameid << "\" not found" << std::endl;
			return false;
		}
		dstream << _("Using game specified by --gameid on the command line")
		        << std::endl;
		game_params->game_spec = commanded_gamespec;
		return true;
	}

	return false;
}

static bool determine_subgame(GameParams *game_params)
{
	SubgameSpec gamespec;

	assert(!game_params->world_path.empty());	// Pre-condition

	// If world doesn't exist
	if (!game_params->world_path.empty()
		&& !getWorldExists(game_params->world_path)) {
		// Try to take gamespec from command line
		if (game_params->game_spec.isValid()) {
			gamespec = game_params->game_spec;
			infostream << "Using commanded gameid [" << gamespec.id << "]" << std::endl;
		} else if (game_params->is_dedicated_server) {
			auto games = getAvailableGameIds();
			// If there's exactly one obvious choice then do the right thing
			if (games.size() > 1)
				games.erase("devtest");
			if (games.size() == 1) {
				gamespec = findSubgame(*games.begin());
				infostream << "Automatically selecting gameid [" << gamespec.id << "]" << std::endl;
			} else {
				// Else, force the user to choose
				auto &url = g_settings->get("contentdb_url");

				errorstream << "To run a " PROJECT_NAME_C " server, you need to select a game using the '--gameid' argument." << std::endl;
				if (games.empty())
					errorstream << "Check out " << url << " for a selection of games to pick from and download." << std::endl;
				else
					errorstream << "Use '--gameid list' to print a list of all installed games." << std::endl;
				return false;
			}
		}
	} else { // World exists
		std::string world_gameid = getWorldGameId(game_params->world_path, false);
		// If commanded to use a gameid, do so
		if (game_params->game_spec.isValid()) {
			gamespec = game_params->game_spec;
			if (game_params->game_spec.id != world_gameid) {
				warningstream << "Using commanded gameid ["
				            << gamespec.id << "]" << " instead of world gameid ["
				            << world_gameid << "]" << std::endl;
			}
		} else {
			// If world contains an embedded game, use it;
			// Otherwise find world from local system.
			gamespec = findWorldSubgame(game_params->world_path);
			infostream << "Using world gameid [" << gamespec.id << "]" << std::endl;
		}
	}

	if (!gamespec.isValid()) {
		if (!game_params->is_dedicated_server)
			return true; // not an error, this would prevent the main menu from running
		errorstream << "Game [" << gamespec.id << "] could not be found."
		            << std::endl;
		return false;
	}

	game_params->game_spec = gamespec;
	return true;
}


/*****************************************************************************
 * Dedicated server
 *****************************************************************************/
static bool run_dedicated_server(const GameParams &game_params, const Settings &cmd_args)
{
	verbosestream << _("Using world path") << " ["
	              << game_params.world_path << "]" << std::endl;
	verbosestream << _("Using gameid") << " ["
	              << game_params.game_spec.id << "]" << std::endl;

	// Database migration/compression
	if (cmd_args.exists("migrate"))
		return migrate_map_database(game_params, cmd_args);

	if (cmd_args.exists("migrate-players"))
		return ServerEnvironment::migratePlayersDatabase(game_params, cmd_args);

	if (cmd_args.exists("migrate-auth"))
		return ServerEnvironment::migrateAuthDatabase(game_params, cmd_args);

	if (cmd_args.exists("migrate-mod-storage"))
		return Server::migrateModStorageDatabase(game_params, cmd_args);

	if (cmd_args.getFlag("recompress"))
		return recompress_map_database(game_params, cmd_args);

	// Bind address
	std::string bind_str = g_settings->get("bind_address");
	Address bind_addr(0, 0, 0, 0, game_params.socket_port);

	if (g_settings->getBool("ipv6_server"))
		bind_addr.setAddress(static_cast<IPv6AddressBytes*>(nullptr));
	try {
		bind_addr.Resolve(bind_str.c_str());
	} catch (const ResolveError &e) {
		warningstream << "Resolving bind address \"" << bind_str
			<< "\" failed: " << e.what()
			<< " -- Listening on all addresses." << std::endl;
	}
	if (bind_addr.isIPv6() && !g_settings->getBool("enable_ipv6")) {
		errorstream << "Unable to listen on "
		            << bind_addr.serializeString()
		            << " because IPv6 is disabled" << std::endl;
		return false;
	}

	if (cmd_args.exists("terminal")) {
#if USE_CURSES
		std::string admin_nick = g_settings->get("name");

		if (!is_valid_player_name(admin_nick)) {
			if (admin_nick.empty()) {
				errorstream << "No name given for admin. "
					<< "Please check your configuration that it "
					<< "contains a 'name = ...' for your main admin account."
					<< std::endl;
			} else {
				errorstream << "Name for admin '"
					<< admin_nick << "' is not valid. "
					<< "Please check that it only contains allowed characters "
					<< "and that it is at most 20 characters long. "
					<< "Valid characters are: " << PLAYERNAME_ALLOWED_CHARS_USER_EXPL
					<< std::endl;
			}
			return false;
		}
		ChatInterface iface;
		volatile auto &kill = *porting::signal_handler_killstatus();

		try {
			// Create server
			Server server(game_params.world_path, game_params.game_spec,
					false, bind_addr, true, &iface);

			g_term_console.setup(&iface, &kill, admin_nick);

			g_term_console.start();

			server.start();
			// Run server
			dedicated_server_loop(server, kill);
		} catch (const ModError &e) {
			g_term_console.stopAndWaitforThread();
			errorstream << "ModError: " << e.what() << std::endl;
			return false;
		} catch (const ServerError &e) {
			g_term_console.stopAndWaitforThread();
			errorstream << "ServerError: " << e.what() << std::endl;
			return false;
		}

		// Tell the console to stop, and wait for it to finish,
		// only then leave context and free iface
		g_term_console.stop();
		g_term_console.wait();

		g_term_console.clearKillStatus();
	} else {
#else
		errorstream << "Cmd arg --terminal passed, but "
			<< "compiled without ncurses. Ignoring." << std::endl;
	} {
#endif
		try {
			// Create server
			Server server(game_params.world_path, game_params.game_spec, false,
				bind_addr, true);
			server.start();

			// Run server
			volatile auto &kill = *porting::signal_handler_killstatus();
			dedicated_server_loop(server, kill);

		} catch (const ModError &e) {
			errorstream << "ModError: " << e.what() << std::endl;
			return false;
		} catch (const ServerError &e) {
			errorstream << "ServerError: " << e.what() << std::endl;
			return false;
		}
	}

	return true;
}

static bool migrate_map_database(const GameParams &game_params, const Settings &cmd_args)
{
	std::string migrate_to = cmd_args.get("migrate");
	Settings world_mt;
	std::string world_mt_path = game_params.world_path + DIR_DELIM + "world.mt";
	if (!world_mt.readConfigFile(world_mt_path.c_str())) {
		errorstream << "Cannot read world.mt!" << std::endl;
		return false;
	}

	if (!world_mt.exists("backend")) {
		errorstream << "Please specify your current backend in world.mt:"
			<< std::endl
			<< "	backend = {sqlite3|leveldb|redis|dummy|postgresql}"
			<< std::endl;
		return false;
	}

	std::string backend = world_mt.get("backend");
	if (backend == migrate_to) {
		errorstream << "Cannot migrate: new backend is same"
			<< " as the old one" << std::endl;
		return false;
	}

	MapDatabase *old_db = ServerMap::createDatabase(backend, game_params.world_path, world_mt),
		*new_db = ServerMap::createDatabase(migrate_to, game_params.world_path, world_mt);

	u32 count = 0;
	u64 last_update_time = 0;
	volatile auto &kill = *porting::signal_handler_killstatus();

	std::vector<v3s16> blocks;
	old_db->listAllLoadableBlocks(blocks);
	new_db->beginSave();
	for (auto it = blocks.begin(); it != blocks.end(); ++it) {
		if (kill) return false;

		std::string data;
		old_db->loadBlock(*it, &data);
		if (!data.empty()) {
			new_db->saveBlock(*it, data);
			count++;
		} else {
			errorstream << "Failed to load block " << *it << ", skipping it." << std::endl;
		}
		if (porting::getTimeS() - last_update_time >= 1) {
			std::cerr << " Migrated " << count << " blocks, "
				<< (100.0 * count / blocks.size()) << "% completed.\r" << std::flush;
			new_db->endSave();
			new_db->beginSave();
			last_update_time = porting::getTimeS();
		}
	}
	std::cerr << std::endl;
	new_db->endSave();
	delete old_db;
	delete new_db;

	actionstream << "Successfully migrated " << count << " blocks" << std::endl;
	world_mt.set("backend", migrate_to);
	if (!world_mt.updateConfigFile(world_mt_path.c_str()))
		errorstream << "Failed to update world.mt!" << std::endl;
	else
		actionstream << "world.mt updated" << std::endl;

	return true;
}

static bool recompress_map_database(const GameParams &game_params, const Settings &cmd_args)
{
	Settings world_mt;
	const std::string world_mt_path = game_params.world_path + DIR_DELIM + "world.mt";

	if (!world_mt.readConfigFile(world_mt_path.c_str())) {
		errorstream << "Cannot read world.mt at " << world_mt_path << std::endl;
		return false;
	}
	const std::string &backend = world_mt.get("backend");
	Server server(game_params.world_path, game_params.game_spec, false, Address(), false);
	MapDatabase *db = ServerMap::createDatabase(backend, game_params.world_path, world_mt);

	u32 count = 0;
	u64 last_update_time = 0;
	volatile auto &kill = *porting::signal_handler_killstatus();
	const u8 serialize_as_ver = SER_FMT_VER_HIGHEST_WRITE;
	const s16 map_compression_level = rangelim(g_settings->getS16("map_compression_level_disk"), -1, 9);

	// This is ok because the server doesn't actually run
	std::vector<v3s16> blocks;
	db->listAllLoadableBlocks(blocks);
	db->beginSave();
	std::istringstream iss(std::ios_base::binary);
	std::ostringstream oss(std::ios_base::binary);
	for (auto it = blocks.begin(); it != blocks.end(); ++it) {
		if (kill) return false;

		std::string data;
		db->loadBlock(*it, &data);
		if (data.empty()) {
			errorstream << "Failed to load block " << *it << std::endl;
			return false;
		}

		iss.str(data);
		iss.clear();

		{
			MapBlock mb(v3s16(0,0,0), &server);
			ServerMap::deSerializeBlock(&mb, iss);

			oss.str("");
			oss.clear();
			writeU8(oss, serialize_as_ver);
			mb.serialize(oss, serialize_as_ver, true, map_compression_level);
		}

		db->saveBlock(*it, oss.str());
		count++;

		if (porting::getTimeS() - last_update_time >= 1) {
			std::cerr << " Recompressed " << count << " blocks, "
				<< (100.0f * count / blocks.size()) << "% completed.\r" << std::flush;
			db->endSave();
			db->beginSave();
			last_update_time = porting::getTimeS();
		}
	}
	std::cerr << std::endl;
	db->endSave();

	actionstream << "Done, " << count << " blocks were recompressed." << std::endl;
	return true;
}
