// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 est31 <MTest31@outlook.com>

#include <inttypes.h>
#include "config.h"
#if USE_CURSES
#include "version.h"
#include "terminal_chat_console.h"
#include "porting.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/string.h"
#include "chat_interface.h"

TerminalChatConsole g_term_console;

// include this last to avoid any conflicts
// (likes to set macros to common names, conflicting various stuff)
#if CURSES_HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif CURSES_HAVE_NCURSESW_CURSES_H
#include <ncursesw/curses.h>
#elif CURSES_HAVE_CURSES_H
#include <curses.h>
#elif CURSES_HAVE_NCURSES_H
#include <ncurses.h>
#elif CURSES_HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif CURSES_HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#endif

// Some functions to make drawing etc position independent
static bool reformat_backend(ChatBackend *backend, int rows, int cols)
{
	if (rows < 2)
		return false;
	backend->reformat(cols, rows - 2);
	return true;
}

static void move_for_backend(int row, int col)
{
	move(row + 1, col);
}

void TerminalChatConsole::initOfCurses()
{
	initscr();
	cbreak(); //raw();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	timeout(100);

	// To make esc not delay up to one second. According to the internet,
	// this is the value vim uses, too.
	set_escdelay(25);

	getmaxyx(stdscr, m_rows, m_cols);
	m_can_draw_text = reformat_backend(&m_chat_backend, m_rows, m_cols);
}

void TerminalChatConsole::deInitOfCurses()
{
	endwin();
}

void *TerminalChatConsole::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	std::cout << "========================" << std::endl;
	std::cout << "Begin log output over terminal"
		<< " (no stdout/stderr backlog during that)" << std::endl;
	// Make the loggers to stdout/stderr shut up.
	// Go over our own loggers instead.
	LogLevelMask err_mask = g_logger.removeOutput(&stderr_output);
	LogLevelMask out_mask = g_logger.removeOutput(&stdout_output);

	g_logger.addOutput(&m_log_output);

	// Inform the server of our nick
	m_chat_interface->command_queue.push_back(
		new ChatEventNick(CET_NICK_ADD, m_nick));

	{
		// Ensures that curses is deinitialized even on an exception being thrown
		CursesInitHelper helper(this);

		while (!stopRequested()) {

			int ch = getch();
			if (stopRequested())
				break;

			step(ch);
		}
	}

	if (m_kill_requested)
		*m_kill_requested = true;

	g_logger.removeOutput(&m_log_output);
	g_logger.addOutputMasked(&stderr_output, err_mask);
	g_logger.addOutputMasked(&stdout_output, out_mask);

	std::cout << "End log output over terminal"
		<< " (no stdout/stderr backlog during that)" << std::endl;
	std::cout << "========================" << std::endl;

	END_DEBUG_EXCEPTION_HANDLER

	return NULL;
}

void TerminalChatConsole::typeChatMessage(const std::wstring &msg)
{
	// Discard empty line
	if (msg.empty())
		return;

	// Send to server
	m_chat_interface->command_queue.push_back(
		new ChatEventChat(m_nick, msg));

	// Print if its a command (gets eaten by server otherwise)
	if (msg[0] == L'/') {
		m_chat_backend.addMessage(L"", (std::wstring)L"Issued command: " + msg);
	}
}

void TerminalChatConsole::handleInput(int ch, bool &complete_redraw_needed)
{
	ChatPrompt &prompt = m_chat_backend.getPrompt();
	// Helpful if you want to collect key codes that aren't documented
	/*if (ch != ERR) {
		m_chat_backend.addMessage(L"",
			(std::wstring)L"Pressed key " + utf8_to_wide(
			std::string(keyname(ch)) + " (code " + itos(ch) + ")"));
		complete_redraw_needed = true;
	}//*/

	// All the key codes below are compatible to xterm
	// Only add new ones if you have tried them there,
	// to ensure compatibility with not just xterm but the wide
	// range of terminals that are compatible to xterm.

	switch (ch) {
		case ERR: // no input
			break;
		case 27: // ESC
			// Toggle ESC mode
			m_esc_mode = !m_esc_mode;
			break;
		case KEY_PPAGE:
			m_chat_backend.scrollPageUp();
			complete_redraw_needed = true;
			break;
		case KEY_NPAGE:
			m_chat_backend.scrollPageDown();
			complete_redraw_needed = true;
			break;
		case KEY_ENTER:
		case '\r':
		case '\n': {
			prompt.addToHistory(prompt.getLine());
			typeChatMessage(prompt.replace(L""));
			break;
		}
		case KEY_UP:
			prompt.historyPrev();
			break;
		case KEY_DOWN:
			prompt.historyNext();
			break;
		case KEY_LEFT:
			// Left pressed
			// move character to the left
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_CHARACTER);
			break;
		case 545:
			// Ctrl-Left pressed
			// move word to the left
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_WORD);
			break;
		case KEY_RIGHT:
			// Right pressed
			// move character to the right
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_CHARACTER);
			break;
		case 560:
			// Ctrl-Right pressed
			// move word to the right
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_WORD);
			break;
		case KEY_HOME:
			// Home pressed
			// move to beginning of line
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			break;
		case KEY_END:
			// End pressed
			// move to end of line
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			break;
		case KEY_BACKSPACE:
		case '\b':
		case 127:
			// Backspace pressed
			// delete character to the left
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_CHARACTER);
			break;
		case KEY_DC:
			// Delete pressed
			// delete character to the right
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_CHARACTER);
			break;
		case 519:
			// Ctrl-Delete pressed
			// delete word to the right
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_WORD);
			break;
		case 21:
			// Ctrl-U pressed
			// kill line to left end
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			break;
		case 11:
			// Ctrl-K pressed
			// kill line to right end
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			break;
		case '\t':
			// Tab pressed
			// Nick completion
			prompt.nickCompletion(m_nicks, false);
			break;
		default:
			// Add character to the prompt,
			// assuming UTF-8.
			if (IS_UTF8_MULTB_START(ch)) {
				m_pending_utf8_bytes.append(1, (char)ch);
				m_utf8_bytes_to_wait += UTF8_MULTB_START_LEN(ch) - 1;
			} else if (m_utf8_bytes_to_wait != 0) {
				m_pending_utf8_bytes.append(1, (char)ch);
				m_utf8_bytes_to_wait--;
				if (m_utf8_bytes_to_wait == 0) {
					std::wstring w = utf8_to_wide(m_pending_utf8_bytes);
					m_pending_utf8_bytes = "";
					// hopefully only one char in the wstring...
					for (size_t i = 0; i < w.size(); i++) {
						prompt.input(w.c_str()[i]);
					}
				}
			} else if (IS_ASCII_PRINTABLE_CHAR(ch)) {
				prompt.input(ch);
			} else {
				// Silently ignore characters we don't handle

				//warningstream << "Pressed invalid character '"
				//	<< keyname(ch) << "' (code " << itos(ch) << ")" << std::endl;
			}
			break;
	}
}

void TerminalChatConsole::step(int ch)
{
	bool complete_redraw_needed = false;

	// empty queues
	while (!m_chat_interface->outgoing_queue.empty()) {
		ChatEvent *evt = m_chat_interface->outgoing_queue.pop_frontNoEx();
		switch (evt->type) {
			case CET_NICK_REMOVE:
				m_nicks.erase(((ChatEventNick *)evt)->nick);
				break;
			case CET_NICK_ADD:
				m_nicks.insert(((ChatEventNick *)evt)->nick);
				break;
			case CET_CHAT:
				complete_redraw_needed = true;
				// This is only used for direct replies from commands
				// or for lua's print() functionality
				m_chat_backend.addMessage(L"", ((ChatEventChat *)evt)->evt_msg);
				break;
			case CET_TIME_INFO:
				ChatEventTimeInfo *tevt = (ChatEventTimeInfo *)evt;
				m_game_time = tevt->game_time;
				m_time_of_day = tevt->time;
		};
		delete evt;
	}
	while (!m_log_output.queue.empty()) {
		complete_redraw_needed = true;
		std::pair<LogLevel, std::string> p = m_log_output.queue.pop_frontNoEx();
		if (p.first > m_log_level)
			continue;

		auto label = std::string("\x1b(c@red)") + Logger::getLevelLabel(p.first)
			+ "\x1b(c@white)";
		m_chat_backend.addMessage(utf8_to_wide(label), utf8_to_wide(p.second));
	}

	// handle input
	if (!m_esc_mode) {
		handleInput(ch, complete_redraw_needed);
	} else {
		switch (ch) {
			case ERR: // no input
				break;
			case 27: // ESC
				// Toggle ESC mode
				m_esc_mode = !m_esc_mode;
				break;
			case 'L':
				m_log_level--;
				m_log_level = MYMAX(m_log_level, LL_NONE + 1); // LL_NONE isn't accessible
				break;
			case 'l':
				m_log_level++;
				m_log_level = MYMIN(m_log_level, LL_MAX - 1);
				break;
		}
	}

	// was there a resize?
	int xn, yn;
	getmaxyx(stdscr, yn, xn);
	if (xn != m_cols || yn != m_rows) {
		m_cols = xn;
		m_rows = yn;
		m_can_draw_text = reformat_backend(&m_chat_backend, m_rows, m_cols);
		complete_redraw_needed = true;
	}

	// draw title
	move(0, 0);
	clrtoeol();
	addstr(PROJECT_NAME_C);
	addstr(" ");
	addstr(g_version_hash);

	u32 minutes = m_time_of_day % 1000;
	u32 hours = m_time_of_day / 1000;
	minutes = (float)minutes / 1000 * 60;

	if (m_game_time)
		printw(" | Game %" PRIu64 " Time of day %02d:%02d ",
			m_game_time, hours, minutes);

	// draw text
	if (complete_redraw_needed && m_can_draw_text)
		draw_text();

	// draw prompt
	if (!m_esc_mode) {
		// normal prompt
		ChatPrompt& prompt = m_chat_backend.getPrompt();
		std::string prompt_text = wide_to_utf8(prompt.getVisiblePortion());
		move(m_rows - 1, 0);
		clrtoeol();
		addstr(prompt_text.c_str());
		// Draw cursor
		s32 cursor_pos = prompt.getVisibleCursorPosition();
		if (cursor_pos >= 0) {
			move(m_rows - 1, cursor_pos);
		}
	} else {
		// esc prompt
		move(m_rows - 1, 0);
		clrtoeol();
		printw("[ESC] Toggle ESC mode |"
			" [CTRL+C] Shut down |"
			" (L) in-, (l) decrease loglevel %s",
			Logger::getLevelLabel((LogLevel) m_log_level));
	}

	refresh();
}

void TerminalChatConsole::draw_text()
{
	ChatBuffer& buf = m_chat_backend.getConsoleBuffer();
	for (u32 row = 0; row < buf.getRows(); row++) {
		move_for_backend(row, 0);
		clrtoeol();
		const ChatFormattedLine& line = buf.getFormattedLine(row);
		if (line.fragments.empty())
			continue;
		for (const ChatFormattedFragment &fragment : line.fragments) {
			addstr(wide_to_utf8(fragment.text.getString()).c_str());
		}
	}
}

void TerminalChatConsole::stopAndWaitforThread()
{
	clearKillStatus();
	stop();
	wait();
}

#endif
