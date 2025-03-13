// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "log_internal.h"

#include "threading/mutex_auto_lock.h"
#include "debug.h"
#include "gettime.h"
#include "porting.h"
#include "settings.h"
#include "config.h"
#include "exceptions.h"
#include "util/numeric.h"
#include "filesys.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#if !defined(_WIN32)
#include <unistd.h> // isatty
#endif

#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>

class LevelTarget : public LogTarget {
public:
	LevelTarget(Logger &logger, LogLevel level, bool raw = false) :
		m_logger(logger),
		m_level(level),
		m_raw(raw)
	{}

	virtual bool hasOutput() override {
		return m_logger.hasOutput(m_level);
	}

	virtual void log(std::string_view buf) override {
		if (!m_raw) {
			m_logger.log(m_level, buf);
		} else {
			m_logger.logRaw(m_level, buf);
		}
	}

private:
	Logger &m_logger;
	LogLevel m_level;
	bool m_raw;
};

////
//// Globals
////

Logger g_logger;

#ifdef __ANDROID__
AndroidLogOutput stdout_output;
AndroidLogOutput stderr_output;
#else
StreamLogOutput stdout_output(std::cout);
StreamLogOutput stderr_output(std::cerr);
#endif

LevelTarget none_target_raw(g_logger, LL_NONE, true);
LevelTarget none_target(g_logger, LL_NONE);
LevelTarget error_target(g_logger, LL_ERROR);
LevelTarget warning_target(g_logger, LL_WARNING);
LevelTarget action_target(g_logger, LL_ACTION);
LevelTarget info_target(g_logger, LL_INFO);
LevelTarget verbose_target(g_logger, LL_VERBOSE);
LevelTarget trace_target(g_logger, LL_TRACE);

thread_local LogStream dstream(none_target);
thread_local LogStream rawstream(none_target_raw);
thread_local LogStream errorstream(error_target);
thread_local LogStream warningstream(warning_target);
thread_local LogStream actionstream(action_target);
thread_local LogStream infostream(info_target);
thread_local LogStream verbosestream(verbose_target);
thread_local LogStream tracestream(trace_target);
thread_local LogStream derr_con(verbose_target);
thread_local LogStream dout_con(trace_target);

// Android
#ifdef __ANDROID__

constexpr static unsigned int g_level_to_android[] = {
	ANDROID_LOG_INFO,     // LL_NONE
	ANDROID_LOG_ERROR,    // LL_ERROR
	ANDROID_LOG_WARN,     // LL_WARNING
	ANDROID_LOG_INFO,     // LL_ACTION
	ANDROID_LOG_DEBUG,    // LL_INFO
	ANDROID_LOG_VERBOSE,  // LL_VERBOSE
	ANDROID_LOG_VERBOSE,  // LL_TRACE
};

void AndroidLogOutput::logRaw(LogLevel lev, std::string_view line)
{
	static_assert(ARRLEN(g_level_to_android) == LL_MAX,
		"mismatch between android and internal loglevels");
	__android_log_print(g_level_to_android[lev], PROJECT_NAME_C, "%.*s",
		static_cast<int>(line.size()), line.data());
}
#endif

///////////////////////////////////////////////////////////////////////////////


////
//// Logger
////

LogLevel Logger::stringToLevel(std::string_view name)
{
	if (name == "none")
		return LL_NONE;
	else if (name == "error")
		return LL_ERROR;
	else if (name == "warning")
		return LL_WARNING;
	else if (name == "action")
		return LL_ACTION;
	else if (name == "info")
		return LL_INFO;
	else if (name == "verbose")
		return LL_VERBOSE;
	else if (name == "trace")
		return LL_TRACE;
	else
		return LL_MAX;
}

void Logger::addOutput(ILogOutput *out)
{
	addOutputMaxLevel(out, (LogLevel)(LL_MAX - 1));
}

void Logger::addOutput(ILogOutput *out, LogLevel lev)
{
	addOutputMasked(out, LOGLEVEL_TO_MASKLEVEL(lev));
}

void Logger::addOutputMasked(ILogOutput *out, LogLevelMask mask)
{
	MutexAutoLock lock(m_mutex);
	for (size_t i = 0; i < LL_MAX; i++) {
		if (mask & LOGLEVEL_TO_MASKLEVEL(i)) {
			m_outputs[i].push_back(out);
			m_has_outputs[i] = true;
		}
	}
}

void Logger::addOutputMaxLevel(ILogOutput *out, LogLevel lev)
{
	MutexAutoLock lock(m_mutex);
	assert(lev < LL_MAX);
	for (size_t i = 0; i <= lev; i++) {
		m_outputs[i].push_back(out);
		m_has_outputs[i] = true;
	}
}

LogLevelMask Logger::removeOutput(ILogOutput *out)
{
	MutexAutoLock lock(m_mutex);
	LogLevelMask ret_mask = 0;
	for (size_t i = 0; i < LL_MAX; i++) {
		auto it = std::find(m_outputs[i].begin(), m_outputs[i].end(), out);
		if (it != m_outputs[i].end()) {
			ret_mask |= LOGLEVEL_TO_MASKLEVEL(i);
			m_outputs[i].erase(it);
			m_has_outputs[i] = !m_outputs[i].empty();
		}
	}
	return ret_mask;
}

void Logger::setLevelSilenced(LogLevel lev, bool silenced)
{
	m_silenced_levels[lev] = silenced;
}

void Logger::registerThread(std::string_view name)
{
	std::thread::id id = std::this_thread::get_id();
	MutexAutoLock lock(m_mutex);
	m_thread_names[id] = name;
}

void Logger::deregisterThread()
{
	std::thread::id id = std::this_thread::get_id();
	MutexAutoLock lock(m_mutex);
	m_thread_names.erase(id);
}

const char *Logger::getLevelLabel(LogLevel lev)
{
	static const char *names[] = {
		"",
		"ERROR",
		"WARNING",
		"ACTION",
		"INFO",
		"VERBOSE",
		"TRACE",
	};
	static_assert(ARRLEN(names) == LL_MAX,
		"mismatch between loglevel names and enum");
	assert(lev < LL_MAX && lev >= 0);
	return names[lev];
}

LogColor Logger::color_mode = LOG_COLOR_AUTO;

const std::string &Logger::getThreadName()
{
	std::thread::id id = std::this_thread::get_id();

	auto it = m_thread_names.find(id);
	if (it != m_thread_names.end())
		return it->second;

	thread_local std::string fallback_name;
	if (fallback_name.empty()) {
		std::ostringstream os;
		os << "#0x" << std::hex << id;
		fallback_name = os.str();
	}
	return fallback_name;
}

void Logger::log(LogLevel lev, std::string_view text)
{
	if (isLevelSilenced(lev))
		return;

	const std::string &thread_name = getThreadName();
	const char *label = getLevelLabel(lev);
	const std::string timestamp = getTimestamp();

	std::string line = timestamp;
	line.append(": ").append(label).append("[").append(thread_name)
		.append("]: ").append(text);

	logToOutputs(lev, line, timestamp, thread_name, text);
}

void Logger::logRaw(LogLevel lev, std::string_view text)
{
	if (isLevelSilenced(lev))
		return;

	logToOutputsRaw(lev, text);
}

void Logger::logToOutputsRaw(LogLevel lev, std::string_view line)
{
	MutexAutoLock lock(m_mutex);
	for (size_t i = 0; i != m_outputs[lev].size(); i++)
		m_outputs[lev][i]->logRaw(lev, line);
}

void Logger::logToOutputs(LogLevel lev, const std::string &combined,
	const std::string &time, const std::string &thread_name,
	std::string_view payload_text)
{
	MutexAutoLock lock(m_mutex);
	for (size_t i = 0; i != m_outputs[lev].size(); i++)
		m_outputs[lev][i]->log(lev, combined, time, thread_name, payload_text);
}

////
//// *LogOutput methods
////

void FileLogOutput::setFile(const std::string &filename, s64 file_size_max)
{
	// Only move debug.txt if there is a valid maximum file size
	bool is_too_large = false;
	if (file_size_max > 0) {
		std::ifstream ifile(filename, std::ios::binary | std::ios::ate);
		if (ifile.good())
			is_too_large = ifile.tellg() > file_size_max;
	}
	if (is_too_large) {
		std::string filename_secondary = filename + ".1";
		actionstream << "The log file grew too big; it is moved to " <<
			filename_secondary << std::endl;
		fs::DeleteSingleFileOrEmptyDirectory(filename_secondary);
		fs::Rename(filename, filename_secondary);
	}

	// Intentionally not using open_ofstream() to keep the text mode
	if (!fs::OpenStream(*m_stream.rdbuf(), filename.c_str(), std::ios::out | std::ios::app, true, false))
		throw FileNotGoodException("Failed to open log file");

	m_stream << "\n\n"
		"-------------\n" <<
		"  Separator\n" <<
		"-------------\n" << std::endl;
}

StreamLogOutput::StreamLogOutput(std::ostream &stream) :
	m_stream(stream)
{
#if !defined(_WIN32)
	if (&stream == &std::cout)
		is_tty = isatty(STDOUT_FILENO);
	else if (&stream == &std::cerr)
		is_tty = isatty(STDERR_FILENO);
#endif
}

void StreamLogOutput::logRaw(LogLevel lev, std::string_view line)
{
	bool colored_message = (Logger::color_mode == LOG_COLOR_ALWAYS) ||
		(Logger::color_mode == LOG_COLOR_AUTO && is_tty);
	if (colored_message) {
		switch (lev) {
		case LL_ERROR:
			// error is red
			m_stream << "\033[91m";
			break;
		case LL_WARNING:
			// warning is yellow
			m_stream << "\033[93m";
			break;
		case LL_INFO:
			// info is a bit dark
			m_stream << "\033[37m";
			break;
		case LL_VERBOSE:
		case LL_TRACE:
			// verbose is darker than info
			m_stream << "\033[2m";
			break;
		default:
			// action is white
			colored_message = false;
		}
	}

	m_stream << line << std::endl;

	if (colored_message) {
		// reset to white color
		m_stream << "\033[0m";
	}
}

void StreamProxy::fix_stream_state(std::ostream &os)
{
	std::ios::iostate state = os.rdstate();
	// clear error state so the stream works again
	os.clear();
	if (state & std::ios::eofbit)
		os << "(ostream:eofbit)";
	if (state & std::ios::badbit)
		os << "(ostream:badbit)";
	if (state & std::ios::failbit)
		os << "(ostream:failbit)";
}
