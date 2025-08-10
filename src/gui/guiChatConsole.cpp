// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiChatConsole.h"
#include "chat.h"
#include "client/client.h"
#include "debug.h"
#include "gettime.h"
#include "client/keycode.h"
#include "settings.h"
#include "porting.h"
#include "client/texturesource.h"
#include "client/fontengine.h"
#include "log.h"
#include "gettext.h"
#include "irrlicht_changes/CGUITTFont.h"
#include "util/string.h"
#include "guiScrollBar.h"
#include <string>

inline u32 clamp_u8(s32 value)
{
	return (u32) MYMIN(MYMAX(value, 0), 255);
}

inline bool isInCtrlKeys(const EKEY_CODE& kc)
{
	return kc == KEY_LCONTROL || kc == KEY_RCONTROL || kc == KEY_CONTROL;
}

inline u32 getScrollbarSize(IGUIEnvironment* env)
{
	return env->getSkin()->getSize(gui::EGDS_SCROLLBAR_SIZE);
}

GUIChatConsole::GUIChatConsole(
		gui::IGUIEnvironment* env,
		gui::IGUIElement* parent,
		s32 id,
		ChatBackend* backend,
		Client* client,
		IMenuManager* menumgr
):
	IGUIElement(gui::EGUIET_ELEMENT, env, parent, id,
			core::rect<s32>(0,0,100,100)),
	m_chat_backend(backend),
	m_client(client),
	m_menumgr(menumgr),
	m_animate_time_old(porting::getTimeMs())
{
	// load background settings
	s32 console_alpha = g_settings->getS32("console_alpha");
	m_background_color.setAlpha(clamp_u8(console_alpha));

	// load the background texture depending on settings
	ITextureSource *tsrc = client->getTextureSource();
	if (tsrc->isKnownSourceImage("background_chat.jpg")) {
		m_background = tsrc->getTexture("background_chat.jpg");
		m_background_color.setRed(255);
		m_background_color.setGreen(255);
		m_background_color.setBlue(255);
	} else {
		v3f console_color = g_settings->getV3F("console_color").value_or(v3f());
		m_background_color.setRed(clamp_u8(myround(console_color.X)));
		m_background_color.setGreen(clamp_u8(myround(console_color.Y)));
		m_background_color.setBlue(clamp_u8(myround(console_color.Z)));
	}

	const u16 chat_font_size = g_settings->getU16("chat_font_size");
	m_font.grab(g_fontengine->getFont(chat_font_size != 0 ?
		rangelim(chat_font_size, 5, 72) : FONT_SIZE_UNSPECIFIED, FM_Mono));

	if (!m_font) {
		errorstream << "GUIChatConsole: Unable to load mono font" << std::endl;
	} else {
		core::dimension2d<u32> dim = m_font->getDimension(L"M");
		m_fontsize = v2u32(dim.Width, dim.Height);
	}
	m_fontsize.X = MYMAX(m_fontsize.X, 1);
	m_fontsize.Y = MYMAX(m_fontsize.Y, 1);

	// set default cursor options
	setCursor(true, true, 2.0, 0.1);

	// track ctrl keys for mouse event
	m_is_ctrl_down = false;
	m_cache_clickable_chat_weblinks = g_settings->getBool("clickable_chat_weblinks");

	m_scrollbar.reset(new GUIScrollBar(env, this, -1, core::rect<s32>(0, 0, 30, m_height), false, true, tsrc));
	m_scrollbar->setSubElement(true);
	m_scrollbar->setLargeStep(1);
	m_scrollbar->setSmallStep(1);
}

void GUIChatConsole::openConsole(f32 scale)
{
	if (m_open)
		return;

	assert(scale > 0.0f && scale <= 1.0f);

	m_open = true;

	m_desired_height_fraction = scale;
	m_desired_height = scale * m_screensize.Y;
	reformatConsole();
	m_animate_time_old = porting::getTimeMs();
	IGUIElement::setVisible(true);
	m_menumgr->createdMenu(this);
}

bool GUIChatConsole::isOpen() const
{
	return m_open;
}

bool GUIChatConsole::isOpenInhibited() const
{
	return m_open_inhibited > 0;
}

void GUIChatConsole::closeConsole()
{
	m_open = false;
	Environment->removeFocus(this);
	m_menumgr->deletingMenu(this);
	m_scrollbar->setVisible(false);
}

void GUIChatConsole::closeConsoleAtOnce()
{
	closeConsole();
	m_height = 0;
	recalculateConsolePosition();
}

void GUIChatConsole::replaceAndAddToHistory(const std::wstring &line)
{
	ChatPrompt& prompt = m_chat_backend->getPrompt();
	prompt.addToHistory(prompt.getLine());
	prompt.replace(line);
}


void GUIChatConsole::setCursor(
	bool visible, bool blinking, f32 blink_speed, f32 relative_height)
{
	if (visible)
	{
		if (blinking)
		{
			// leave m_cursor_blink unchanged
			m_cursor_blink_speed = blink_speed;
		}
		else
		{
			m_cursor_blink = 0x8000;  // on
			m_cursor_blink_speed = 0.0;
		}
	}
	else
	{
		m_cursor_blink = 0;  // off
		m_cursor_blink_speed = 0.0;
	}
	m_cursor_height = relative_height;
}

void GUIChatConsole::draw()
{
	if(!IsVisible)
		return;

	video::IVideoDriver* driver = Environment->getVideoDriver();

	// Check screen size
	v2u32 screensize = driver->getScreenSize();
	if (screensize != m_screensize)
	{
		// screen size has changed
		// scale current console height to new window size
		if (m_screensize.Y != 0)
			m_height = m_height * screensize.Y / m_screensize.Y;
		m_screensize = screensize;
		m_desired_height = m_desired_height_fraction * m_screensize.Y;
		reformatConsole();
	} else if (!m_scrollbar->getAbsolutePosition().isPointInside(core::vector2di(screensize.X, m_height))) {
		// the height of the chat window is no longer the height of the scrollbar
		// happens while opening/closing the window
		updateScrollbar(true);
	}

	// Animation
	u64 now = porting::getTimeMs();
	animate(now - m_animate_time_old);
	m_animate_time_old = now;

	// Draw console elements if visible
	if (m_height > 0)
	{
		drawBackground();
		drawText();
		drawPrompt();
	}

	gui::IGUIElement::draw();
}

void GUIChatConsole::reformatConsole()
{
	s32 cols = m_screensize.X / m_fontsize.X - 2; // make room for a margin (looks better)
	s32 rows = m_desired_height / m_fontsize.Y - 1; // make room for the input prompt
	if (cols <= 0 || rows <= 0)
		cols = rows = 0;

	updateScrollbar(true);

	recalculateConsolePosition();
	m_chat_backend->reformat(cols, rows);
}

void GUIChatConsole::recalculateConsolePosition()
{
	core::rect<s32> rect(0, 0, m_screensize.X, m_height);
	DesiredRect = rect;
	recalculateAbsolutePosition(false);
}

void GUIChatConsole::animate(u32 msec)
{
	// animate the console height
	s32 goal = m_open ? m_desired_height : 0;

	// Set invisible if close animation finished (reset by openConsole)
	// This function (animate()) is never called once its visibility becomes false so do not
	//		actually set visible to false before the inhibited period is over
	if (!m_open && m_height == 0 && m_open_inhibited == 0)
		IGUIElement::setVisible(false);

	if (m_height != goal)
	{
		s32 max_change = msec * m_screensize.Y * (m_height_speed / 1000.0);
		if (max_change == 0)
			max_change = 1;

		if (m_height < goal)
		{
			// increase height
			if (m_height + max_change < goal)
				m_height += max_change;
			else
				m_height = goal;
		}
		else
		{
			// decrease height
			if (m_height > goal + max_change)
				m_height -= max_change;
			else
				m_height = goal;
		}

		recalculateConsolePosition();
	}

	// blink the cursor
	if (m_cursor_blink_speed != 0.0)
	{
		u32 blink_increase = 0x10000 * msec * (m_cursor_blink_speed / 1000.0);
		if (blink_increase == 0)
			blink_increase = 1;
		m_cursor_blink = ((m_cursor_blink + blink_increase) & 0xffff);
	}

	// decrease open inhibit counter
	if (m_open_inhibited > msec)
		m_open_inhibited -= msec;
	else
		m_open_inhibited = 0;
}

void GUIChatConsole::drawBackground()
{
	video::IVideoDriver* driver = Environment->getVideoDriver();
	if (m_background != NULL)
	{
		core::rect<s32> sourcerect(0, -m_height, m_screensize.X, 0);
		driver->draw2DImage(
			m_background,
			v2s32(0, 0),
			sourcerect,
			&AbsoluteClippingRect,
			m_background_color,
			false);
	}
	else
	{
		driver->draw2DRectangle(
			m_background_color,
			core::rect<s32>(0, 0, m_screensize.X, m_height),
			&AbsoluteClippingRect);
	}
}

void GUIChatConsole::drawText()
{
	if (!m_font)
		return;

	ChatBuffer& buf = m_chat_backend->getConsoleBuffer();

	core::recti rect;
	if (m_scrollbar->isVisible())
		rect = core::rect<s32> (0, 0, m_screensize.X - getScrollbarSize(Environment), m_height);
	else
		rect = AbsoluteClippingRect;

	for (u32 row = 0; row < buf.getRows(); ++row)
	{
		const ChatFormattedLine& line = buf.getFormattedLine(row);
		if (line.fragments.empty())
			continue;

		s32 line_height = m_fontsize.Y;
		s32 y = row * line_height + m_height - m_desired_height;
		if (y + line_height < 0)
			continue;

		for (const ChatFormattedFragment &fragment : line.fragments) {
			s32 x = (fragment.column + 1) * m_fontsize.X;
			core::rect<s32> destrect(
				x, y, x + m_fontsize.X * fragment.text.size(), y + m_fontsize.Y);

			if (m_font->getType() == gui::EGFT_CUSTOM) {
				// Draw colored text if possible
				auto *tmp = static_cast<gui::CGUITTFont*>(m_font.get());
				tmp->draw(
					fragment.text,
					destrect,
					false,
					false,
					&rect);
			} else {
				// Otherwise use standard text
				m_font->draw(
					fragment.text.c_str(),
					destrect,
					video::SColor(255, 255, 255, 255),
					false,
					false,
					&rect);
			}
		}
	}

	updateScrollbar();
}

void GUIChatConsole::drawPrompt()
{
	if (!m_font)
		return;

	ChatPrompt& prompt = m_chat_backend->getPrompt();
	std::wstring prompt_text = prompt.getVisiblePortion();

	u32 font_width  = m_fontsize.X;
	u32 font_height = m_fontsize.Y;

	core::dimension2d<u32> size = m_font->getDimension(prompt_text.c_str());
	u32 text_width = size.Width;
	if (size.Height > font_height)
		font_height = size.Height;

	u32 row = m_chat_backend->getConsoleBuffer().getRows();
	s32 y = row * font_height + m_height - m_desired_height;

	core::rect<s32> destrect(
		font_width, y, font_width + text_width, y + font_height);
	m_font->draw(
		prompt_text.c_str(),
		destrect,
		video::SColor(255, 255, 255, 255),
		false,
		false,
		&AbsoluteClippingRect);

	// Draw the cursor during on periods
	if ((m_cursor_blink & 0x8000) != 0)
	{
		s32 cursor_pos = prompt.getVisibleCursorPosition();

		if (cursor_pos >= 0)
		{

			u32 text_to_cursor_pos_width = m_font->getDimension(prompt_text.substr(0, cursor_pos).c_str()).Width;

			s32 cursor_len = prompt.getCursorLength();
			video::IVideoDriver* driver = Environment->getVideoDriver();
			s32 x = font_width + text_to_cursor_pos_width;
			core::rect<s32> destrect(
				x,
				y + font_height * (1.0 - m_cursor_height),
				x + font_width * MYMAX(cursor_len, 1),
				y + font_height * (cursor_len ? m_cursor_height+1 : 1)
			);
			video::SColor cursor_color(255,255,255,255);
			driver->draw2DRectangle(
				cursor_color,
				destrect,
				&AbsoluteClippingRect);
		}
	}

}

bool GUIChatConsole::OnEvent(const SEvent& event)
{

	ChatPrompt &prompt = m_chat_backend->getPrompt();

	if (event.EventType == EET_KEY_INPUT_EVENT && !event.KeyInput.PressedDown)
	{
		// CTRL up
		if (isInCtrlKeys(event.KeyInput.Key))
		{
			m_is_ctrl_down = false;
		}
	}
	else if(event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown)
	{
		// CTRL down
		if (isInCtrlKeys(event.KeyInput.Key)) {
			m_is_ctrl_down = true;
		}

		// Key input
		if (KeyPress(event.KeyInput) == getKeySetting("keymap_console")) {
			closeConsole();

			// inhibit open so the_game doesn't reopen immediately
			m_open_inhibited = 50;
			m_close_on_enter = false;
			return true;
		}

		// Mac OS sends private use characters along with some keys.
		bool has_char = event.KeyInput.Char && !event.KeyInput.Control &&
				!iswcntrl(event.KeyInput.Char) && !IS_PRIVATE_USE_CHAR(event.KeyInput.Char);

		if (event.KeyInput.Key == KEY_ESCAPE) {
			closeConsoleAtOnce();
			m_close_on_enter = false;
			// inhibit open so the_game doesn't reopen immediately
			m_open_inhibited = 1; // so the ESCAPE button doesn't open the "pause menu"
			return true;
		}
		else if(event.KeyInput.Key == KEY_PRIOR)
		{
			if (!has_char) { // no num lock
				m_chat_backend->scrollPageUp();
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_NEXT)
		{
			if (!has_char) { // no num lock
				m_chat_backend->scrollPageDown();
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_RETURN)
		{
			prompt.addToHistory(prompt.getLine());
			std::wstring text = prompt.replace(L"");
			m_client->typeChatMessage(text);
			if (m_close_on_enter) {
				closeConsoleAtOnce();
				m_close_on_enter = false;
			}
			return true;
		}
		else if(event.KeyInput.Key == KEY_UP)
		{
			if (!has_char) { // no num lock
				// Up pressed
				// Move back in history
				prompt.historyPrev();
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_DOWN)
		{
			if (!has_char) { // no num lock
				// Down pressed
				// Move forward in history
				prompt.historyNext();
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_LEFT || event.KeyInput.Key == KEY_RIGHT)
		{
			if (!has_char) { // no num lock
				// Left/right pressed
				// Move/select character/word to the left depending on control and shift keys
				ChatPrompt::CursorOp op = event.KeyInput.Shift ?
					ChatPrompt::CURSOROP_SELECT :
					ChatPrompt::CURSOROP_MOVE;
				ChatPrompt::CursorOpDir dir = event.KeyInput.Key == KEY_LEFT ?
					ChatPrompt::CURSOROP_DIR_LEFT :
					ChatPrompt::CURSOROP_DIR_RIGHT;
				ChatPrompt::CursorOpScope scope = event.KeyInput.Control ?
					ChatPrompt::CURSOROP_SCOPE_WORD :
					ChatPrompt::CURSOROP_SCOPE_CHARACTER;
				prompt.cursorOperation(op, dir, scope);

				if (op == ChatPrompt::CURSOROP_SELECT)
					updatePrimarySelection();
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_HOME)
		{
			if (!has_char) { // no num lock
				// Home pressed
				// move to beginning of line
				prompt.cursorOperation(
					ChatPrompt::CURSOROP_MOVE,
					ChatPrompt::CURSOROP_DIR_LEFT,
					ChatPrompt::CURSOROP_SCOPE_LINE);
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_END)
		{
			if (!has_char) { // no num lock
				// End pressed
				// move to end of line
				prompt.cursorOperation(
					ChatPrompt::CURSOROP_MOVE,
					ChatPrompt::CURSOROP_DIR_RIGHT,
					ChatPrompt::CURSOROP_SCOPE_LINE);
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_BACK)
		{
			// Backspace or Ctrl-Backspace pressed
			// delete character / word to the left
			ChatPrompt::CursorOpScope scope =
				event.KeyInput.Control ?
				ChatPrompt::CURSOROP_SCOPE_WORD :
				ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				scope);
			return true;
		}
		else if(event.KeyInput.Key == KEY_DELETE)
		{
			if (!has_char) { // no num lock
				// Delete or Ctrl-Delete pressed
				// delete character / word to the right
				ChatPrompt::CursorOpScope scope =
					event.KeyInput.Control ?
					ChatPrompt::CURSOROP_SCOPE_WORD :
					ChatPrompt::CURSOROP_SCOPE_CHARACTER;
				prompt.cursorOperation(
					ChatPrompt::CURSOROP_DELETE,
					ChatPrompt::CURSOROP_DIR_RIGHT,
					scope);
				return true;
			}
		}
		else if(event.KeyInput.Key == KEY_KEY_A && event.KeyInput.Control)
		{
			// Ctrl-A pressed
			// Select all text
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_SELECT,
				ChatPrompt::CURSOROP_DIR_LEFT, // Ignored
				ChatPrompt::CURSOROP_SCOPE_LINE);

			updatePrimarySelection();
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_C && event.KeyInput.Control)
		{
			// Ctrl-C pressed
			// Copy text to clipboard
			if (prompt.getCursorLength() <= 0)
				return true;
			std::wstring wselected = prompt.getSelection();
			std::string selected = wide_to_utf8(wselected);
			Environment->getOSOperator()->copyToClipboard(selected.c_str());
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_V && event.KeyInput.Control)
		{
			// Ctrl-V pressed
			// paste text from clipboard
			if (prompt.getCursorLength() > 0) {
				// Delete selected section of text
				prompt.cursorOperation(
					ChatPrompt::CURSOROP_DELETE,
					ChatPrompt::CURSOROP_DIR_LEFT, // Ignored
					ChatPrompt::CURSOROP_SCOPE_SELECTION);
			}
			IOSOperator *os_operator = Environment->getOSOperator();
			const c8 *text = os_operator->getTextFromClipboard();
			if (!text)
				return true;
			prompt.input(utf8_to_wide(text));
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_X && event.KeyInput.Control)
		{
			// Ctrl-X pressed
			// Cut text to clipboard
			if (prompt.getCursorLength() <= 0)
				return true;
			std::wstring wselected = prompt.getSelection();
			std::string selected = wide_to_utf8(wselected);
			Environment->getOSOperator()->copyToClipboard(selected.c_str());
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT, // Ignored
				ChatPrompt::CURSOROP_SCOPE_SELECTION);
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_U && event.KeyInput.Control)
		{
			// Ctrl-U pressed
			// kill line to left end
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(event.KeyInput.Key == KEY_KEY_K && event.KeyInput.Control)
		{
			// Ctrl-K pressed
			// kill line to right end
			prompt.cursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(event.KeyInput.Key == KEY_TAB)
		{
			// Tab or Shift-Tab pressed
			// Nick completion
			auto names = m_client->getConnectedPlayerNames();
			bool backwards = event.KeyInput.Shift;
			prompt.nickCompletion(names, backwards);
			return true;
		}

		if (has_char) {
			prompt.input(event.KeyInput.Char);
			return true;
		}
	}
	else if(event.EventType == EET_MOUSE_INPUT_EVENT)
	{
		if (event.MouseInput.Event == EMIE_MOUSE_WHEEL)
		{
			s32 rows = myround(-3.0 * event.MouseInput.Wheel);
			m_chat_backend->scroll(rows);
		}
		// Middle click or ctrl-click opens weblink, if enabled in config
		// Otherwise, middle click pastes primary selection
		else if (event.MouseInput.Event == EMIE_MMOUSE_PRESSED_DOWN ||
				(event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN && m_is_ctrl_down))
		{
			// If clicked within console output region
			if (event.MouseInput.Y / m_fontsize.Y < (m_height / m_fontsize.Y) - 1 )
			{
				// Translate pixel position to font position
				bool was_url_pressed = m_cache_clickable_chat_weblinks &&
						weblinkClick(event.MouseInput.X / m_fontsize.X,
								event.MouseInput.Y / m_fontsize.Y);

				if (!was_url_pressed
						&& event.MouseInput.Event == EMIE_MMOUSE_PRESSED_DOWN) {
					// Paste primary selection at cursor pos
					const c8 *text = Environment->getOSOperator()
							->getTextFromPrimarySelection();
					if (text)
						prompt.input(utf8_to_wide(text));
				}
			}
		}
	}
	else if(event.EventType == EET_STRING_INPUT_EVENT)
	{
		prompt.input(std::wstring(event.StringInput.Str->c_str()));
		return true;
	}
	else if (event.EventType == EET_GUI_EVENT && event.GUIEvent.EventType == EGET_SCROLL_BAR_CHANGED &&
			(void*) event.GUIEvent.Caller == (void*) m_scrollbar.get())
	{
		m_chat_backend->getConsoleBuffer().scrollAbsolute(m_scrollbar->getPos());
	}

	return Parent ? Parent->OnEvent(event) : false;
}

void GUIChatConsole::setVisible(bool visible)
{
	m_open = visible;
	IGUIElement::setVisible(visible);
	if (!visible) {
		m_height = 0;
		recalculateConsolePosition();
	}
	m_scrollbar->setVisible(visible);
}

bool GUIChatConsole::weblinkClick(s32 col, s32 row)
{
	// Prevent accidental rapid clicking
	static u64 s_oldtime = 0;
	u64 newtime = porting::getTimeMs();

	// 0.6 seconds should suffice
	if (newtime - s_oldtime < 600)
		return false;
	s_oldtime = newtime;

	const std::vector<ChatFormattedFragment> &
			frags = m_chat_backend->getConsoleBuffer().getFormattedLine(row).fragments;
	std::string weblink = ""; // from frag meta

	// Identify targetted fragment, if exists
	int indx = frags.size() - 1;
	if (indx < 0) {
		// Invalid row, frags is empty
		return false;
	}
	// Scan from right to left, offset by 1 font space because left margin
	while (indx > -1 && (u32)col < frags[indx].column + 1) {
		--indx;
	}
	if (indx > -1) {
		weblink = frags[indx].weblink;
		// Note if(indx < 0) then a frag somehow had a corrupt column field
	}

	/*
	// Debug help. Please keep this in case adjustments are made later.
	std::string ws;
	ws = "Middleclick: (" + std::to_string(col) + ',' + std::to_string(row) + ')' + " frags:";
	// show all frags <position>(<length>) for the clicked row
	for (u32 i=0;i<frags.size();++i) {
		if (indx == int(i))
			// tag the actual clicked frag
			ws += '*';
		ws += std::to_string(frags.at(i).column) + '('
			+ std::to_string(frags.at(i).text.size()) + "),";
	}
	actionstream << ws << std::endl;
	*/

	// User notification
	if (weblink.size() != 0) {
		std::ostringstream msg;
		msg << " * ";
		if (porting::open_url(weblink)) {
			msg << gettext("Opening webpage");
		}
		else {
			msg << gettext("Failed to open webpage");
		}
		msg << " '" << weblink << "'";
		m_chat_backend->addUnparsedMessage(utf8_to_wide(msg.str()));
		return true;
	}

	return false;
}

void GUIChatConsole::updatePrimarySelection()
{
	std::wstring wselected = m_chat_backend->getPrompt().getSelection();
	std::string selected = wide_to_utf8(wselected);
	Environment->getOSOperator()->copyToPrimarySelection(selected.c_str());
}

void GUIChatConsole::updateScrollbar(bool update_size)
{
	ChatBuffer &buf = m_chat_backend->getConsoleBuffer();
	m_scrollbar->setMin(buf.getTopScrollPos());
	m_scrollbar->setMax(buf.getBottomScrollPos());
	m_scrollbar->setPos(buf.getScrollPosition());
	m_scrollbar->setPageSize(m_fontsize.Y * buf.getLineCount());
	m_scrollbar->setVisible(m_scrollbar->getMin() != m_scrollbar->getMax());

	if (update_size) {
		const core::rect<s32> rect (m_screensize.X - getScrollbarSize(Environment), 0, m_screensize.X, m_height);
		m_scrollbar->setRelativePosition(rect);
	}
}
