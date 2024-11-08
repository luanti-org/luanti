// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "chat.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "config.h"
#include "debug.h"
#include "settings.h"
#include "util/strfnd.h"
#include "util/string.h"
#include "util/numeric.h"

ChatBuffer::ChatBuffer(u32 scrollback):
	m_scrollback(scrollback)
{
	if (m_scrollback == 0)
		m_scrollback = 1;
	m_empty_formatted_line.first = true;

	m_cache_clickable_chat_weblinks = false;
	// Curses mode cannot access g_settings here
	if (g_settings != nullptr) {
		m_cache_clickable_chat_weblinks = g_settings->getBool("clickable_chat_weblinks");
		if (m_cache_clickable_chat_weblinks) {
			std::string colorval = g_settings->get("chat_weblink_color");
			parseColorString(colorval, m_cache_chat_weblink_color, false, 255);
			m_cache_chat_weblink_color.setAlpha(255);
		}
	}
}

void ChatBuffer::addLine(const std::wstring &name, const std::wstring &text)
{
	m_lines_modified = true;

	ChatLine line(name, text);
	m_unformatted.push_back(line);

	if (m_rows > 0) {
		// m_formatted is valid and must be kept valid
		bool scrolled_at_bottom = (m_scroll == getBottomScrollPos());
		u32 num_added = formatChatLine(line, m_cols, m_formatted);
		if (scrolled_at_bottom)
			m_scroll += num_added;
	}

	// Limit number of lines by m_scrollback
	if (m_unformatted.size() > m_scrollback) {
		deleteOldest(m_unformatted.size() - m_scrollback);
	}
}

void ChatBuffer::clear()
{
	m_unformatted.clear();
	m_formatted.clear();
	m_scroll = 0;
	m_lines_modified = true;
}

u32 ChatBuffer::getLineCount() const
{
	return m_unformatted.size();
}

const ChatLine& ChatBuffer::getLine(u32 index) const
{
	assert(index < getLineCount());	// pre-condition
	return m_unformatted[index];
}

void ChatBuffer::step(f32 dtime)
{
	for (ChatLine &line : m_unformatted) {
		line.age += dtime;
	}
}

void ChatBuffer::deleteOldest(u32 count)
{
	bool at_bottom = (m_scroll == getBottomScrollPos());

	u32 del_unformatted = 0;
	u32 del_formatted = 0;

	while (count > 0 && del_unformatted < m_unformatted.size()) {
		++del_unformatted;

		// keep m_formatted in sync
		if (del_formatted < m_formatted.size()) {
			sanity_check(m_formatted[del_formatted].first);
			++del_formatted;
			while (del_formatted < m_formatted.size() &&
					!m_formatted[del_formatted].first)
				++del_formatted;
		}

		--count;
	}

	m_unformatted.erase(m_unformatted.begin(), m_unformatted.begin() + del_unformatted);
	m_formatted.erase(m_formatted.begin(), m_formatted.begin() + del_formatted);

	if (del_unformatted > 0)
		m_lines_modified = true;

	if (at_bottom)
		m_scroll = getBottomScrollPos();
	else
		scrollAbsolute(m_scroll - del_formatted);
}

void ChatBuffer::deleteByAge(f32 maxAge)
{
	u32 count = 0;
	while (count < m_unformatted.size() && m_unformatted[count].age > maxAge)
		++count;
	deleteOldest(count);
}

u32 ChatBuffer::getRows() const
{
	return m_rows;
}

void ChatBuffer::reformat(u32 cols, u32 rows)
{
	if (cols == 0 || rows == 0)
	{
		// Clear formatted buffer
		m_cols = 0;
		m_rows = 0;
		m_scroll = 0;
		m_formatted.clear();
	}
	else if (cols != m_cols || rows != m_rows)
	{
		// TODO: Avoid reformatting ALL lines (even invisible ones)
		// each time the console size changes.

		// Find out the scroll position in *unformatted* lines
		u32 restore_scroll_unformatted = 0;
		u32 restore_scroll_formatted = 0;
		bool at_bottom = (m_scroll == getBottomScrollPos());
		if (!at_bottom)
		{
			for (s32 i = 0; i < m_scroll; ++i)
			{
				if (m_formatted[i].first)
					++restore_scroll_unformatted;
			}
		}

		// If number of columns change, reformat everything
		if (cols != m_cols)
		{
			m_formatted.clear();
			for (u32 i = 0; i < m_unformatted.size(); ++i)
			{
				if (i == restore_scroll_unformatted)
					restore_scroll_formatted = m_formatted.size();
				formatChatLine(m_unformatted[i], cols, m_formatted);
			}
		}

		// Update the console size
		m_cols = cols;
		m_rows = rows;

		// Restore the scroll position
		if (at_bottom)
		{
			scrollBottom();
		}
		else
		{
			scrollAbsolute(restore_scroll_formatted);
		}
	}
}

const ChatFormattedLine& ChatBuffer::getFormattedLine(u32 row) const
{
	s32 index = m_scroll + (s32) row;
	if (index >= 0 && index < (s32) m_formatted.size())
		return m_formatted[index];

	return m_empty_formatted_line;
}

void ChatBuffer::scroll(s32 rows)
{
	scrollAbsolute(m_scroll + rows);
}

void ChatBuffer::scrollAbsolute(s32 scroll)
{
	s32 top = getTopScrollPos();
	s32 bottom = getBottomScrollPos();

	m_scroll = scroll;
	if (m_scroll < top)
		m_scroll = top;
	if (m_scroll > bottom)
		m_scroll = bottom;
}

void ChatBuffer::scrollBottom()
{
	m_scroll = getBottomScrollPos();
}

u32 ChatBuffer::formatChatLine(const ChatLine &line, u32 cols,
		std::vector<ChatFormattedLine> &destination) const
{
	u32 num_added = 0;
	std::vector<ChatFormattedFragment> next_frags;
	ChatFormattedLine next_line;
	ChatFormattedFragment temp_frag;
	u32 out_column = 0;
	u32 in_pos = 0;
	u32 hanging_indentation = 0;

	// Format the sender name and produce fragments
	if (!line.name.empty()) {
		temp_frag.text = L"<";
		temp_frag.column = 0;
		//temp_frag.bold = 0;
		next_frags.push_back(temp_frag);
		temp_frag.text = line.name;
		temp_frag.column = 0;
		//temp_frag.bold = 1;
		next_frags.push_back(temp_frag);
		temp_frag.text = L"> ";
		temp_frag.column = 0;
		//temp_frag.bold = 0;
		next_frags.push_back(temp_frag);
	}

	std::wstring name_sanitized = line.name.c_str();

	// Choose an indentation level
	if (line.name.empty()) {
		// Server messages
		hanging_indentation = 0;
	} else if (name_sanitized.size() + 3 <= cols/2) {
		// Names shorter than about half the console width
		hanging_indentation = line.name.size() + 3;
	} else {
		// Very long names
		hanging_indentation = 2;
	}
	// If there are no columns remaining after the indentation (window is very
	// narrow), we can't write anything
	if (hanging_indentation >= cols)
		return 0;

	next_line.first = true;
	// Set/use forced newline after the last frag in each line
	bool mark_newline = false;

	// Produce fragments and layout them into lines
	while (!next_frags.empty() || in_pos < line.text.size()) {
		mark_newline = false; // now using this to USE line-end frag

		// Layout fragments into lines
		while (!next_frags.empty()) {
			ChatFormattedFragment& frag = next_frags[0];

			// Force newline after this frag, if marked
			if (frag.column == INT_MAX)
				mark_newline = true;

			if (frag.text.size() <= cols - out_column) {
				// Fragment fits into current line
				frag.column = out_column;
				next_line.fragments.push_back(frag);
				out_column += frag.text.size();
				next_frags.erase(next_frags.begin());
			} else {
				// Fragment does not fit into current line
				// So split it up
				temp_frag.text = frag.text.substr(0, cols - out_column);
				temp_frag.column = out_column;
				temp_frag.weblink = frag.weblink;

				next_line.fragments.push_back(temp_frag);
				frag.text = frag.text.substr(cols - out_column);
				frag.column = 0;
				out_column = cols;
			}

			if (out_column == cols || mark_newline) {
				// End the current line
				destination.push_back(next_line);
				num_added++;
				next_line.fragments.clear();
				next_line.first = false;

				out_column = hanging_indentation;
				mark_newline = false;
			}
		}

		// Produce fragment(s) for next formatted line
		if (!(in_pos < line.text.size()))
			continue;

		const std::wstring &linestring = line.text.getString();
		u32 remaining_in_output = cols - out_column;
		size_t http_pos = std::wstring::npos;
		mark_newline = false;  // now using this to SET line-end frag

		// Construct all frags for next output line
		while (!mark_newline) {
			// Determine a fragment length <= the minimum of
			// remaining_in_{in,out}put. Try to end the fragment
			// on a word boundary.
			u32 frag_length = 0, space_pos = 0;
			u32 remaining_in_input = line.text.size() - in_pos;

			if (m_cache_clickable_chat_weblinks) {
				// Note: unsigned(-1) on fail
				http_pos = linestring.find(L"https://", in_pos);
				if (http_pos == std::wstring::npos)
					http_pos = linestring.find(L"http://", in_pos);
				if (http_pos != std::wstring::npos)
					http_pos -= in_pos;
			}

			while (frag_length < remaining_in_input &&
					frag_length < remaining_in_output) {
				if (iswspace(linestring[in_pos + frag_length]))
					space_pos = frag_length;
				++frag_length;
			}

			if (http_pos >= remaining_in_output) {
				// Http not in range, grab until space or EOL, halt as normal.
				// Note this works because (http_pos = npos) is unsigned(-1)

				mark_newline = true;
			} else if (http_pos == 0) {
				// At http, grab ALL until FIRST whitespace or end marker. loop.
				// If at end of string, next loop will be empty string to mark end of weblink.

				frag_length = 6;  // Frag is at least "http://"

				// Chars to mark end of weblink
				// TODO? replace this with a safer (slower) regex whitelist?
				static const std::wstring delim_chars = L"\'\";";
				wchar_t tempchar = linestring[in_pos+frag_length];
				while (frag_length < remaining_in_input &&
						!iswspace(tempchar) &&
						delim_chars.find(tempchar) == std::wstring::npos) {
					++frag_length;
					tempchar = linestring[in_pos+frag_length];
				}

				// Remove tailing punctuation characters
				static const std::wstring tailing_chars = L",.";
				tempchar = linestring[in_pos+frag_length - 1];
				if (tailing_chars.find(tempchar) != std::wstring::npos) {
					frag_length--;
				}

				space_pos = frag_length - 1;
				// This frag may need to be force-split. That's ok, urls aren't "words"
				if (frag_length >= remaining_in_output) {
					mark_newline = true;
				}
			} else {
				// Http in range, grab until http, loop

				space_pos = http_pos - 1;
				frag_length = http_pos;
			}

			// Include trailing space in current frag
			if (space_pos != 0 && frag_length < remaining_in_input)
				frag_length = space_pos + 1;

			temp_frag.text = line.text.substr(in_pos, frag_length);
			// A hack so this frag remembers mark_newline for the layout phase
			temp_frag.column = mark_newline ? INT_MAX : 0;

			if (http_pos == 0) {
				// Discard color stuff from the source frag
				temp_frag.text = EnrichedString(temp_frag.text.getString());
				temp_frag.text.setDefaultColor(m_cache_chat_weblink_color);
				// Set weblink in the frag meta
				temp_frag.weblink = wide_to_utf8(temp_frag.text.getString());
			} else {
				temp_frag.weblink.clear();
			}
			next_frags.push_back(temp_frag);
			in_pos += frag_length;
			remaining_in_output -= std::min(frag_length, remaining_in_output);
		}
	}

	// End the last line
	if (num_added == 0 || !next_line.fragments.empty()) {
		destination.push_back(next_line);
		num_added++;
	}

	return num_added;
}

s32 ChatBuffer::getTopScrollPos() const
{
	s32 formatted_count = (s32) m_formatted.size();
	s32 rows = (s32) m_rows;
	if (rows == 0)
		return 0;

	if (formatted_count <= rows)
		return formatted_count - rows;

	return 0;
}

s32 ChatBuffer::getBottomScrollPos() const
{
	s32 formatted_count = (s32) m_formatted.size();
	s32 rows = (s32) m_rows;
	if (rows == 0)
		return 0;

	return formatted_count - rows;
}

void ChatBuffer::resize(u32 scrollback)
{
	m_scrollback = scrollback;
	if (m_unformatted.size() > m_scrollback)
		deleteOldest(m_unformatted.size() - m_scrollback);
}


ChatPrompt::ChatPrompt(const std::wstring &prompt, u32 history_limit):
	m_prompt(prompt),
	m_history_limit(history_limit)
{
}

const std::wstring &ChatPrompt::getLineRef() const
{
	return m_history_index >= m_history.size() ? m_line : m_history[m_history_index].line;
}

std::wstring &ChatPrompt::makeLineRef()
{
	if (m_history_index >= m_history.size()) {
		return m_line;
	} else {
		if (!m_history[m_history_index].saved)
			m_history[m_history_index].saved = m_history[m_history_index].line;
		return m_history[m_history_index].line;
	}
}

bool ChatPrompt::HistoryEntry::operator==(const ChatPrompt::HistoryEntry &other)
{
	if (line != other.line)
		return false;
	if (saved == other.saved)
		return true;
	if ((!saved || saved == line) && (!other.saved || other.saved == other.line))
		return true;
	return false;
}

void ChatPrompt::input(wchar_t ch)
{
	makeLineRef().insert(m_cursor, 1, ch);
	m_cursor++;
	clampView();
	m_nick_completion_start = 0;
	m_nick_completion_end = 0;
}

void ChatPrompt::input(const std::wstring &str)
{
	makeLineRef().insert(m_cursor, str);
	m_cursor += str.size();
	clampView();
	m_nick_completion_start = 0;
	m_nick_completion_end = 0;
}

void ChatPrompt::addToHistory(const std::wstring &line)
{
	std::wstring old_line = getLine();
	if (m_history_index < m_history.size()) {
		auto entry = m_history.begin() + m_history_index;
		if (entry->saved && entry->line == line) {
			entry->line = *entry->saved;
			entry->saved = std::nullopt;
			// Remove potential duplicates
			auto dup_before = std::find(m_history.begin(), entry, *entry);
			if (dup_before != entry)
				m_history.erase(dup_before);
			else if (std::find(entry + 1, m_history.end(), *entry) != m_history.end())
				m_history.erase(entry);
		}
	}
	if (!line.empty() &&
			(m_history.size() == 0 || m_history.back().line != line)) {
		HistoryEntry entry(line);
		// Remove all duplicates
		m_history.erase(std::remove(m_history.begin(), m_history.end(), entry),
				m_history.end());
		// Push unique line
		m_history.push_back(std::move(entry));
	}
	if (m_history.size() > m_history_limit)
		m_history.erase(m_history.begin());
	m_history_index = m_history.size();
	m_line = std::move(old_line);
}

void ChatPrompt::clear()
{
	makeLineRef().clear();
	m_view = 0;
	m_cursor = 0;
	m_nick_completion_start = 0;
	m_nick_completion_end = 0;
}

std::wstring ChatPrompt::replace(const std::wstring &line)
{
	std::wstring old_line = getLine();
	makeLineRef() = line;
	m_view = m_cursor = line.size();
	clampView();
	m_nick_completion_start = 0;
	m_nick_completion_end = 0;
	return old_line;
}

void ChatPrompt::historyPrev()
{
	if (m_history_index != 0) {
		--m_history_index;
		m_view = m_cursor = getLineRef().size();
		clampView();
		m_nick_completion_start = 0;
		m_nick_completion_end = 0;
	}
}

void ChatPrompt::historyNext()
{
	if (m_history_index < m_history.size()) {
		m_history_index++;
		m_view = m_cursor = getLineRef().size();
		clampView();
		m_nick_completion_start = 0;
		m_nick_completion_end = 0;
	}
}

void ChatPrompt::nickCompletion(const std::set<std::string> &names, bool backwards)
{
	const std::wstring_view line(getLineRef());
	// Two cases:
	// (a) m_nick_completion_start == m_nick_completion_end == 0
	//     Then no previous nick completion is active.
	//     Get the word around the cursor and replace with any nick
	//     that has that word as a prefix.
	// (b) else, continue a previous nick completion.
	//     m_nick_completion_start..m_nick_completion_end are the
	//     interval where the originally used prefix was. Cycle
	//     through the list of completions of that prefix.
	u32 prefix_start = m_nick_completion_start;
	u32 prefix_end = m_nick_completion_end;
	bool initial = (prefix_end == 0);
	if (initial)
	{
		// no previous nick completion is active
		prefix_start = prefix_end = m_cursor;
		while (prefix_start > 0 && !iswspace(line[prefix_start-1]))
			--prefix_start;
		while (prefix_end < line.size() && !iswspace(line[prefix_end]))
			++prefix_end;
		if (prefix_start == prefix_end)
			return;
	}
	auto prefix = line.substr(prefix_start, prefix_end - prefix_start);

	// find all names that start with the selected prefix
	std::vector<std::wstring> completions;
	for (const std::string &name : names) {
		std::wstring completion = utf8_to_wide(name);
		if (str_starts_with(completion, prefix, true)) {
			if (prefix_start == 0)
				completion += L": ";
			completions.push_back(completion);
		}
	}

	if (completions.empty())
		return;

	// find a replacement string and the word that will be replaced
	u32 word_end = prefix_end;
	u32 replacement_index = 0;
	if (!initial)
	{
		while (word_end < line.size() && !iswspace(line[word_end]))
			++word_end;
		auto word = line.substr(prefix_start, word_end - prefix_start);

		// cycle through completions
		for (u32 i = 0; i < completions.size(); ++i)
		{
			if (str_equal(word, completions[i], true))
			{
				if (backwards)
					replacement_index = i + completions.size() - 1;
				else
					replacement_index = i + 1;
				replacement_index %= completions.size();
				break;
			}
		}
	}
	const auto &replacement = completions[replacement_index];
	if (word_end < line.size() && iswspace(line[word_end]))
		++word_end;

	// replace existing word with replacement word,
	// place the cursor at the end and record the completion prefix
	makeLineRef().replace(prefix_start, word_end - prefix_start, replacement);
	m_cursor = prefix_start + replacement.size();
	clampView();
	m_nick_completion_start = prefix_start;
	m_nick_completion_end = prefix_end;
}

void ChatPrompt::reformat(u32 cols)
{
	if (cols <= m_prompt.size())
	{
		m_cols = 0;
		m_view = m_cursor;
	}
	else
	{
		s32 length = getLineRef().size();
		bool was_at_end = (m_view + m_cols >= length + 1);
		m_cols = cols - m_prompt.size();
		if (was_at_end)
			m_view = length;
		clampView();
	}
}

std::wstring ChatPrompt::getVisiblePortion() const
{
	const std::wstring &line_ref = getLineRef();
	if ((size_t)m_view >= line_ref.size())
		return m_prompt;
	else
		return m_prompt + line_ref.substr(m_view, m_cols);
}

s32 ChatPrompt::getVisibleCursorPosition() const
{
	return m_cursor - m_view + m_prompt.size();
}

void ChatPrompt::cursorOperation(CursorOp op, CursorOpDir dir, CursorOpScope scope)
{
	s32 old_cursor = m_cursor;
	s32 new_cursor = m_cursor;

	const std::wstring &line = getLineRef();
	s32 length = line.size();
	s32 increment = (dir == CURSOROP_DIR_RIGHT) ? 1 : -1;

	switch (scope) {
	case CURSOROP_SCOPE_CHARACTER:
		new_cursor += increment;
		break;
	case CURSOROP_SCOPE_WORD:
		if (dir == CURSOROP_DIR_RIGHT) {
			// skip one word to the right
			while (new_cursor < length && iswspace(line[new_cursor]))
				new_cursor++;
			while (new_cursor < length && !iswspace(line[new_cursor]))
				new_cursor++;
			while (new_cursor < length && iswspace(line[new_cursor]))
				new_cursor++;
		} else {
			// skip one word to the left
			while (new_cursor >= 1 && iswspace(line[new_cursor - 1]))
				new_cursor--;
			while (new_cursor >= 1 && !iswspace(line[new_cursor - 1]))
				new_cursor--;
		}
		break;
	case CURSOROP_SCOPE_LINE:
		new_cursor += increment * length;
		break;
	case CURSOROP_SCOPE_SELECTION:
		break;
	}

	new_cursor = MYMAX(MYMIN(new_cursor, length), 0);

	switch (op) {
	case CURSOROP_MOVE:
		m_cursor = new_cursor;
		m_cursor_len = 0;
		break;
	case CURSOROP_DELETE:
		if (m_cursor_len > 0) { // Delete selected text first
			makeLineRef().erase(m_cursor, m_cursor_len);
		} else {
			m_cursor = MYMIN(new_cursor, old_cursor);
			makeLineRef().erase(m_cursor, abs(new_cursor - old_cursor));
		}
		m_cursor_len = 0;
		break;
	case CURSOROP_SELECT:
		if (scope == CURSOROP_SCOPE_LINE) {
			m_cursor = 0;
			m_cursor_len = length;
		} else {
			m_cursor = MYMIN(new_cursor, old_cursor);
			m_cursor_len += abs(new_cursor - old_cursor);
			m_cursor_len = MYMIN(m_cursor_len, length - m_cursor);
		}
		break;
	}

	clampView();

	m_nick_completion_start = 0;
	m_nick_completion_end = 0;
}

void ChatPrompt::clampView()
{
	s32 length = getLineRef().size();
	if (length + 1 <= m_cols)
	{
		m_view = 0;
	}
	else
	{
		m_view = MYMIN(m_view, length + 1 - m_cols);
		m_view = MYMIN(m_view, m_cursor);
		m_view = MYMAX(m_view, m_cursor - m_cols + 1);
		m_view = MYMAX(m_view, 0);
	}
}



ChatBackend::ChatBackend():
	m_console_buffer(1500),
	m_recent_buffer(6),
	m_prompt(L"]", 1500)
{
}

void ChatBackend::addMessage(const std::wstring &name, std::wstring text)
{
	// Note: A message may consist of multiple lines, for example the MOTD.
	text = translate_string(text);
	WStrfnd fnd(text);
	while (!fnd.at_end())
	{
		std::wstring line = fnd.next(L"\n");
		m_console_buffer.addLine(name, line);
		m_recent_buffer.addLine(name, line);
	}
}

void ChatBackend::addUnparsedMessage(std::wstring message)
{
	// TODO: Remove the need to parse chat messages client-side, by sending
	// separate name and text fields in TOCLIENT_CHAT_MESSAGE.

	if (message.size() >= 2 && message[0] == L'<')
	{
		std::size_t closing = message.find_first_of(L'>', 1);
		if (closing != std::wstring::npos &&
				closing + 2 <= message.size() &&
				message[closing+1] == L' ')
		{
			std::wstring name = message.substr(1, closing - 1);
			std::wstring text = message.substr(closing + 2);
			addMessage(name, text);
			return;
		}
	}

	// Unable to parse, probably a server message.
	addMessage(L"", message);
}

ChatBuffer& ChatBackend::getConsoleBuffer()
{
	return m_console_buffer;
}

ChatBuffer& ChatBackend::getRecentBuffer()
{
	return m_recent_buffer;
}

EnrichedString ChatBackend::getRecentChat() const
{
	EnrichedString result;
	for (u32 i = 0; i < m_recent_buffer.getLineCount(); ++i) {
		const ChatLine& line = m_recent_buffer.getLine(i);
		if (i != 0)
			result += L"\n";
		if (!line.name.empty()) {
			result += L"<";
			result += line.name;
			result += L"> ";
		}
		result += line.text;
	}
	return result;
}

ChatPrompt& ChatBackend::getPrompt()
{
	return m_prompt;
}

void ChatBackend::reformat(u32 cols, u32 rows)
{
	m_console_buffer.reformat(cols, rows);

	// no need to reformat m_recent_buffer, its formatted lines
	// are not used

	m_prompt.reformat(cols);
}

void ChatBackend::clearRecentChat()
{
	m_recent_buffer.clear();
}


void ChatBackend::applySettings()
{
	u32 recent_lines = g_settings->getU32("recent_chat_messages");
	recent_lines = rangelim(recent_lines, 2, 20);
	m_recent_buffer.resize(recent_lines);
}

void ChatBackend::step(float dtime)
{
	m_recent_buffer.step(dtime);
	m_recent_buffer.deleteByAge(60.0);

	// no need to age messages in anything but m_recent_buffer
}

void ChatBackend::scroll(s32 rows)
{
	m_console_buffer.scroll(rows);
}

void ChatBackend::scrollPageDown()
{
	m_console_buffer.scroll(m_console_buffer.getRows());
}

void ChatBackend::scrollPageUp()
{
	m_console_buffer.scroll(-(s32)m_console_buffer.getRows());
}
