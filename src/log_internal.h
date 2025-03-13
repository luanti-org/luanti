// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <atomic>
#include <map>
#include <queue>
#include <string_view>
#include <fstream>
#include <thread>
#include <mutex>
#include "threading/mutex_auto_lock.h"
#include "util/basic_macros.h"
#include "util/stream.h"
#include "irrlichttypes.h"
#include "log.h"

class ILogOutput;

enum LogLevel {
	LL_NONE, // Special level that is always printed
	LL_ERROR,
	LL_WARNING,
	LL_ACTION,  // In-game actions
	LL_INFO,
	LL_VERBOSE,
	LL_TRACE,
	LL_MAX,
};

enum LogColor {
	LOG_COLOR_NEVER,
	LOG_COLOR_ALWAYS,
	LOG_COLOR_AUTO,
};

typedef u8 LogLevelMask;
#define LOGLEVEL_TO_MASKLEVEL(x) (1 << x)

class Logger {
public:
	void addOutput(ILogOutput *out);
	void addOutput(ILogOutput *out, LogLevel lev);
	void addOutputMasked(ILogOutput *out, LogLevelMask mask);
	void addOutputMaxLevel(ILogOutput *out, LogLevel lev);
	LogLevelMask removeOutput(ILogOutput *out);
	void setLevelSilenced(LogLevel lev, bool silenced);

	void registerThread(std::string_view name);
	void deregisterThread();

	void log(LogLevel lev, std::string_view text);
	// Logs without a prefix
	void logRaw(LogLevel lev, std::string_view text);

	static LogLevel stringToLevel(std::string_view name);
	static const char *getLevelLabel(LogLevel lev);

	bool hasOutput(LogLevel level) {
		return m_has_outputs[level].load(std::memory_order_relaxed);
	}

	bool isLevelSilenced(LogLevel level) {
		return m_silenced_levels[level].load(std::memory_order_relaxed);
	}

	static LogColor color_mode;

private:
	void logToOutputsRaw(LogLevel, std::string_view line);
	void logToOutputs(LogLevel, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		std::string_view payload_text);

	const std::string &getThreadName();

	std::vector<ILogOutput *> m_outputs[LL_MAX];
	std::atomic<bool> m_has_outputs[LL_MAX];
	std::atomic<bool> m_silenced_levels[LL_MAX];
	std::map<std::thread::id, std::string> m_thread_names;
	mutable std::mutex m_mutex;
};

class ILogOutput {
public:
	virtual void logRaw(LogLevel, std::string_view line) = 0;
	virtual void log(LogLevel, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		std::string_view payload_text) = 0;
};

class ICombinedLogOutput : public ILogOutput {
public:
	void log(LogLevel lev, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		std::string_view payload_text)
	{
		logRaw(lev, combined);
	}
};

class StreamLogOutput : public ICombinedLogOutput {
public:
	StreamLogOutput(std::ostream &stream);

	void logRaw(LogLevel lev, std::string_view line);

private:
	std::ostream &m_stream;
	bool is_tty = false;
};

class FileLogOutput : public ICombinedLogOutput {
public:
	void setFile(const std::string &filename, s64 file_size_max);

	void logRaw(LogLevel lev, std::string_view line)
	{
		m_stream << line << std::endl;
	}

private:
	std::ofstream m_stream;
};

struct LogEntry {
	LogLevel level;
	std::string timestamp;
	std::string thread_name;
	std::string text;
	// "timestamp: level[thread_name]: text"
	std::string combined;
};

class CaptureLogOutput : public ILogOutput {
public:
	CaptureLogOutput() = delete;
	DISABLE_CLASS_COPY(CaptureLogOutput);

	CaptureLogOutput(Logger &logger) : m_logger(logger)
	{
		m_logger.addOutput(this);
	}

	~CaptureLogOutput()
	{
		m_logger.removeOutput(this);
	}

	void setLogLevel(LogLevel level)
	{
		m_logger.removeOutput(this);
		m_logger.addOutputMaxLevel(this, level);
	}

	void logRaw(LogLevel lev, std::string_view line) override
	{
		MutexAutoLock lock(m_mutex);
		m_entries.emplace_back(LogEntry{lev, "", "", std::string(line), std::string(line)});
	}

	void log(LogLevel lev, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		std::string_view payload_text) override
	{
		MutexAutoLock lock(m_mutex);
		m_entries.emplace_back(LogEntry{lev, time, thread_name, std::string(payload_text), combined});
	}

	// Take the log entries currently stored, clearing the buffer.
	std::vector<LogEntry> take()
	{
		std::vector<LogEntry> entries;
		MutexAutoLock lock(m_mutex);
		std::swap(m_entries, entries);
		return entries;
	}

private:
	Logger &m_logger;
	// g_logger serializes calls to log/logRaw with a mutex, but that
	// doesn't prevent take() from being called on top of it.
	// This mutex prevents that.
	std::mutex m_mutex;
	std::vector<LogEntry> m_entries;
};


#ifdef __ANDROID__
class AndroidLogOutput : public ICombinedLogOutput {
public:
	void logRaw(LogLevel lev, std::string_view line);
};
#endif

#ifdef __ANDROID__
extern AndroidLogOutput stdout_output;
extern AndroidLogOutput stderr_output;
#else
extern StreamLogOutput stdout_output;
extern StreamLogOutput stderr_output;
#endif

extern Logger g_logger;
