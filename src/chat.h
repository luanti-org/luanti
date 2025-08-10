// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <set>
#include <string>
#include <vector>
#include <list>
#include <optional>

#include "irrlichttypes.h"
#include "util/enriched_string.h"

// Chat console related classes

struct ChatLine
{
	// age in seconds
	f32 age = 0.0f;
	// name of sending player, or empty if sent by server
	EnrichedString name;
	// message text
	EnrichedString text;

	ChatLine(const std::wstring &a_name, const std::wstring &a_text):
		name(a_name),
		text(a_text)
	{
	}

	ChatLine(const EnrichedString &a_name, const EnrichedString &a_text):
		name(a_name),
		text(a_text)
	{
	}
};

struct ChatFormattedFragment
{
	// text string
	EnrichedString text;
	// starting column
	u32 column;
	// web link is empty for most frags
	std::string weblink;
	// formatting
	//u8 bold:1;
};

struct ChatFormattedLine
{
	// Array of text fragments
	std::vector<ChatFormattedFragment> fragments;
	// true if first line of one formatted ChatLine
	bool first;
};

class ChatBuffer
{
public:
	ChatBuffer(u32 scrollback);
	~ChatBuffer() = default;

	// Append chat line
	// Removes oldest chat line if scrollback size is reached
	void addLine(const std::wstring &name, const std::wstring &text);

	// Remove all chat lines
	void clear();

	// Get number of lines currently in buffer.
	u32 getLineCount() const;
	// Get reference to i-th chat line.
	const ChatLine& getLine(u32 index) const;

	// Increase each chat line's age by dtime.
	void step(f32 dtime);
	// Delete oldest N chat lines.
	void deleteOldest(u32 count);
	// Delete lines older than maxAge.
	void deleteByAge(f32 maxAge);

	// Get number of rows, 0 if reformat has not been called yet.
	u32 getRows() const;
	// Update console size and reformat all formatted lines.
	void reformat(u32 cols, u32 rows);
	// Get formatted line for a given row (0 is top of screen).
	// Only valid after reformat has been called at least once
	const ChatFormattedLine& getFormattedLine(u32 row) const;
	// Scrolling in formatted buffer (relative)
	// positive rows == scroll up, negative rows == scroll down
	void scroll(s32 rows);
	// Scrolling in formatted buffer (absolute)
	void scrollAbsolute(s32 scroll);
	// Scroll to bottom of buffer (newest)
	void scrollBottom();

	// Functions for keeping track of whether the lines were modified by any
	// preceding operations
	// If they were not changed, getLineCount() and getLine() output the same as
	// before
	bool getLinesModified() const { return m_lines_modified; }
	void resetLinesModified() { m_lines_modified = false; }

	// Format a chat line for the given number of columns.
	// Appends the formatted lines to the destination array and
	// returns the number of formatted lines.
	u32 formatChatLine(const ChatLine& line, u32 cols,
			std::vector<ChatFormattedLine>& destination) const;

	void resize(u32 scrollback);

	// Get the current scroll position
	s32 getScrollPosition() const { return m_scroll; }
	s32 getTopScrollPos() const;
	s32 getBottomScrollPos() const;

private:
	// Scrollback size
	u32 m_scrollback;
	// Array of unformatted chat lines
	std::vector<ChatLine> m_unformatted;

	// Number of character columns in console
	u32 m_cols = 0;
	// Number of character rows in console
	u32 m_rows = 0;
	// Scroll position (console's top line index into m_formatted)
	s32 m_scroll = 0;
	// Array of formatted lines
	std::vector<ChatFormattedLine> m_formatted;
	// Empty formatted line, for error returns
	ChatFormattedLine m_empty_formatted_line;

	// Enable clickable chat weblinks
	bool m_cache_clickable_chat_weblinks;
	// Color of clickable chat weblinks
	video::SColor m_cache_chat_weblink_color;

	// Whether the lines were modified since last markLinesUnchanged()
	// Is always set to true when m_unformatted is modified, because that's what
	// determines the output of getLineCount() and getLine()
	bool m_lines_modified = true;
};

class ChatPrompt
{
public:
	ChatPrompt(const std::wstring &prompt, u32 history_limit);
	~ChatPrompt() = default;

	// Input character or string
	void input(wchar_t ch);
	void input(const std::wstring &str);

	// Add a string to the history
	void addToHistory(const std::wstring &line);

	// Get current line
	std::wstring getLine() const { return getLineRef(); }

	// Get section of line that is currently selected
	std::wstring getSelection() const { return getLineRef().substr(m_cursor, m_cursor_len); }

	// Clear the current line
	void clear();

	// Replace the current line with the given text
	std::wstring replace(const std::wstring &line);

	// Select previous command from history
	void historyPrev();
	// Select next command from history
	void historyNext();

	// Nick completion
	void nickCompletion(const std::set<std::string> &names, bool backwards);

	// Update console size and reformat the visible portion of the prompt
	void reformat(u32 cols);
	// Get visible portion of the prompt.
	std::wstring getVisiblePortion() const;
	// Get cursor position (relative to visible portion). -1 if invalid
	s32 getVisibleCursorPosition() const;
	// Get length of cursor selection
	s32 getCursorLength() const { return m_cursor_len; }

	// Cursor operations
	enum CursorOp {
		CURSOROP_MOVE,
		CURSOROP_SELECT,
		CURSOROP_DELETE
	};

	// Cursor operation direction
	enum CursorOpDir {
		CURSOROP_DIR_LEFT,
		CURSOROP_DIR_RIGHT
	};

	// Cursor operation scope
	enum CursorOpScope {
		CURSOROP_SCOPE_CHARACTER,
		CURSOROP_SCOPE_WORD,
		CURSOROP_SCOPE_LINE,
		CURSOROP_SCOPE_SELECTION
	};

	// Cursor operation
	// op specifies whether it's a move or delete operation
	// dir specifies whether the operation goes left or right
	// scope specifies how far the operation will reach (char/word/line)
	// Examples:
	//   cursorOperation(CURSOROP_MOVE, CURSOROP_DIR_RIGHT, CURSOROP_SCOPE_LINE)
	//     moves the cursor to the end of the line.
	//   cursorOperation(CURSOROP_DELETE, CURSOROP_DIR_LEFT, CURSOROP_SCOPE_WORD)
	//     deletes the word to the left of the cursor.
	void cursorOperation(CursorOp op, CursorOpDir dir, CursorOpScope scope);

protected:
	const std::wstring &getLineRef() const;

	std::wstring &makeLineRef();

	// set m_view to ensure that 0 <= m_view <= m_cursor < m_view + m_cols
	// if line can be fully shown, set m_view to zero
	// else, also ensure m_view <= m_line.size() + 1 - m_cols
	void clampView();

private:
	struct HistoryEntry {
		std::wstring line;
		// If line is edited, saved holds the unedited version.
		std::optional<std::wstring> saved;

		HistoryEntry(const std::wstring &line): line(line) {}

		bool operator==(const HistoryEntry &other);
		bool operator!=(const HistoryEntry &other) { return !(*this == other); }
	};

	// Prompt prefix
	std::wstring m_prompt = L"";
	// Non-historical edited line
	std::wstring m_line = L"";
	// History buffer
	std::vector<HistoryEntry> m_history;
	// History index (0 <= m_history_index <= m_history.size())
	u32 m_history_index = 0;
	// Maximum number of history entries
	u32 m_history_limit;

	// Number of columns excluding columns reserved for the prompt
	s32 m_cols = 0;
	// Start of visible portion (index into m_line)
	s32 m_view = 0;
	// Cursor (index into m_line)
	s32 m_cursor = 0;
	// Cursor length (length of selected portion of line)
	s32 m_cursor_len = 0;

	// Last nick completion start (index into m_line)
	s32 m_nick_completion_start = 0;
	// Last nick completion start (index into m_line)
	s32 m_nick_completion_end = 0;
};

class ChatBackend
{
public:
	ChatBackend();
	~ChatBackend() = default;

	// Add chat message
	void addMessage(const std::wstring &name, std::wstring text);
	// Parse and add unparsed chat message
	void addUnparsedMessage(std::wstring line);

	// Get the console buffer
	ChatBuffer& getConsoleBuffer();
	// Get the recent messages buffer
	ChatBuffer& getRecentBuffer();
	// Concatenate all recent messages
	EnrichedString getRecentChat() const;
	// Get the console prompt
	ChatPrompt& getPrompt();

	// Reformat all buffers
	void reformat(u32 cols, u32 rows);

	// Clear all recent messages
	void clearRecentChat();

	// Age recent messages
	void step(float dtime);

	// Scrolling
	void scroll(s32 rows);
	void scrollPageDown();
	void scrollPageUp();

	// Resize recent buffer based on settings
	void applySettings();

private:
	ChatBuffer m_console_buffer;
	ChatBuffer m_recent_buffer;
	ChatPrompt m_prompt;
};
