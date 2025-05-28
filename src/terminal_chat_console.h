// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 est31 <MTest31@outlook.com>

#pragma once

#include "chat.h"
#include "threading/thread.h"
#include "util/container.h"
#include "log.h"
#include "log_internal.h"

#include <csignal>
#include <set>
#include <sstream>


struct ChatInterface;

class TermLogOutput : public ILogOutput {
public:

	void logRaw(LogLevel lev, std::string_view line)
	{
		queue.push_back(std::make_pair(lev, std::string(line)));
	}

	virtual void log(LogLevel lev, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		std::string_view payload_text)
	{
		std::ostringstream os(std::ios_base::binary);
		os << time << ": [" << thread_name << "] " << payload_text;

		queue.push_back(std::make_pair(lev, os.str()));
	}

	MutexedQueue<std::pair<LogLevel, std::string> > queue;
};

class TerminalChatConsole : public Thread {
public:

	TerminalChatConsole() :
		Thread("TerminalThread")
	{}

	void setup(
		ChatInterface *iface,
		volatile std::sig_atomic_t *kill_requested,
		const std::string &nick)
	{
		m_nick = nick;
		m_kill_requested = kill_requested;
		m_chat_interface = iface;
	}

	virtual void *run();

	// Highly required!
	void clearKillStatus() { m_kill_requested = nullptr; }

	void stopAndWaitforThread();

private:
	// these have stupid names so that nobody missclassifies them
	// as curses functions. Oh, curses has stupid names too?
	// Well, at least it was worth a try...
	void initOfCurses();
	void deInitOfCurses();

	void draw_text();

	void typeChatMessage(const std::wstring &m);

	void handleInput(int ch, bool &complete_redraw_needed);

	void step(int ch);

	// Used to ensure the deinitialisation is always called.
	struct CursesInitHelper {
		TerminalChatConsole *cons;
		CursesInitHelper(TerminalChatConsole * a_console)
			: cons(a_console)
		{ cons->initOfCurses(); }
		~CursesInitHelper() { cons->deInitOfCurses(); }
	};

	int m_log_level = LL_ACTION;
	std::string m_nick;

	u8 m_utf8_bytes_to_wait = 0;
	std::string m_pending_utf8_bytes;

	std::set<std::string> m_nicks;

	int m_cols;
	int m_rows;
	bool m_can_draw_text;

	volatile std::sig_atomic_t *m_kill_requested = nullptr;
	ChatBackend                 m_chat_backend;
	ChatInterface              *m_chat_interface;

	TermLogOutput m_log_output;

	bool m_esc_mode = false;

	u64 m_game_time = 0;
	u32 m_time_of_day = 0;
};

extern TerminalChatConsole g_term_console;
