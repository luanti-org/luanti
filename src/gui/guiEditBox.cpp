// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Minetest

#include "guiEditBox.h"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"

#include "porting.h"
#include "util/string.h"

GUIEditBox::~GUIEditBox()
{
	if (m_override_font)
		m_override_font->drop();

	if (m_operator)
		m_operator->drop();

	if (m_vscrollbar)
		m_vscrollbar->drop();
}

void GUIEditBox::setOverrideFont(IGUIFont *font)
{
	if (m_override_font == font)
		return;

	if (m_override_font)
		m_override_font->drop();

	m_override_font = font;

	if (m_override_font)
		m_override_font->grab();

	breakText();
}

//! Get the font which is used right now for drawing
IGUIFont *GUIEditBox::getActiveFont() const
{
	if (m_override_font)
		return m_override_font;
	IGUISkin *skin = Environment->getSkin();
	if (skin)
		return skin->getFont();
	return 0;
}

//! Sets another color for the text.
void GUIEditBox::setOverrideColor(video::SColor color)
{
	m_override_color = color;
	m_override_color_enabled = true;
}

video::SColor GUIEditBox::getOverrideColor() const
{
	return m_override_color;
}

//! Sets if the text should use the overide color or the color in the gui skin.
void GUIEditBox::enableOverrideColor(bool enable)
{
	m_override_color_enabled = enable;
}

//! Enables or disables word wrap
void GUIEditBox::setWordWrap(bool enable)
{
	m_word_wrap = enable;
	breakText();
}

//! Enables or disables newlines.
void GUIEditBox::setMultiLine(bool enable)
{
	m_multiline = enable;
}

//! Enables or disables automatic scrolling with cursor position
//! \param enable: If set to true, the text will move around with the cursor position
void GUIEditBox::setAutoScroll(bool enable)
{
	m_autoscroll = enable;
}

void GUIEditBox::setPasswordBox(bool password_box, wchar_t password_char)
{
	m_passwordbox = password_box;
	if (m_passwordbox) {
		m_passwordchar = password_char;
		setMultiLine(false);
		setWordWrap(false);
		m_broken_text.clear();
	}
}

//! Sets text justification
void GUIEditBox::setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical)
{
	m_halign = horizontal;
	m_valign = vertical;
}

//! Sets the new caption of this element.
void GUIEditBox::setText(const wchar_t *text)
{
	Text = text;
	if (u32(m_cursor_pos) > Text.size())
		m_cursor_pos = Text.size();
	m_hscroll_pos = 0;
	breakText();
}

//! Sets the maximum amount of characters which may be entered in the box.
//! \param max: Maximum amount of characters. If 0, the character amount is
//! infinity.
void GUIEditBox::setMax(u32 max)
{
	m_max = max;

	if (Text.size() > m_max && m_max != 0)
		Text = Text.subString(0, m_max);
}

//! Gets the area of the text in the edit box
//! \return Returns the size in pixels of the text
core::dimension2du GUIEditBox::getTextDimension()
{
	core::rect<s32> ret;

	setTextRect(0);
	ret = m_current_text_rect;

	for (u32 i = 1; i < m_broken_text.size(); ++i) {
		setTextRect(i);
		ret.addInternalPoint(m_current_text_rect.UpperLeftCorner);
		ret.addInternalPoint(m_current_text_rect.LowerRightCorner);
	}

	return core::dimension2du(ret.getSize());
}

//! Turns the border on or off
void GUIEditBox::setDrawBorder(bool border)
{
	m_border = border;
}

void GUIEditBox::setWritable(bool can_write_text)
{
	m_writable = can_write_text;
}

//! set text markers
void GUIEditBox::setTextMarkers(s32 begin, s32 end)
{
	if (begin != m_mark_begin || end != m_mark_end) {
		m_mark_begin = begin;
		m_mark_end = end;

		if (!m_passwordbox && m_operator && m_mark_begin != m_mark_end) {
			// copy to primary selection
			const s32 realmbgn = m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
			const s32 realmend = m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;

			std::string s = stringw_to_utf8(Text.subString(realmbgn, realmend - realmbgn));
			m_operator->copyToPrimarySelection(s.c_str());
		}

		sendGuiEvent(EGET_EDITBOX_MARKING_CHANGED);
	}
}

//! send some gui event to parent
void GUIEditBox::sendGuiEvent(EGUI_EVENT_TYPE type)
{
	if (Parent) {
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = this;
		e.GUIEvent.Element = 0;
		e.GUIEvent.EventType = type;

		Parent->OnEvent(e);
	}
}

//! called if an event happened.
bool GUIEditBox::OnEvent(const SEvent &event)
{
	if (isEnabled()) {
		switch (event.EventType) {
		case EET_GUI_EVENT:
			if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST) {
				if (event.GUIEvent.Caller == this) {
					m_mouse_marking = false;
					setTextMarkers(0, 0);
				}
			}
			break;
		case EET_KEY_INPUT_EVENT:
			if (processKey(event))
				return true;
			break;
		case EET_MOUSE_INPUT_EVENT:
			if (processMouse(event))
				return true;
			break;
		case EET_STRING_INPUT_EVENT:
			inputString(*event.StringInput.Str);
			return true;
		default:
			break;
		}
	}

	return IGUIElement::OnEvent(event);
}

bool GUIEditBox::processKey(const SEvent &event)
{
	if (!event.KeyInput.PressedDown)
		return false;

	bool text_changed = false;
	s32 new_mark_begin = m_mark_begin;
	s32 new_mark_end = m_mark_end;

	// control shortcut handling
	if (event.KeyInput.Control) {

		// german backlash '\' entered with control + '?'
		if (event.KeyInput.Char == '\\') {
			inputChar(event.KeyInput.Char);
			return true;
		}

		switch (event.KeyInput.Key) {
		case KEY_KEY_A:
			// select all
			new_mark_begin = 0;
			new_mark_end = Text.size();
			break;
		case KEY_KEY_C:
			onKeyControlC(event);
			break;
		case KEY_KEY_X:
			text_changed = onKeyControlX(event, new_mark_begin, new_mark_end);
			break;
		case KEY_KEY_V:
			text_changed = onKeyControlV(event, new_mark_begin, new_mark_end);
			break;
		case KEY_HOME:
			// move/highlight to start of text
			if (event.KeyInput.Shift) {
				new_mark_end = m_cursor_pos;
				new_mark_begin = 0;
				m_cursor_pos = 0;
			} else {
				m_cursor_pos = 0;
				new_mark_begin = 0;
				new_mark_end = 0;
			}
			break;
		case KEY_END:
			// move/highlight to end of text
			if (event.KeyInput.Shift) {
				new_mark_begin = m_cursor_pos;
				new_mark_end = Text.size();
				m_cursor_pos = 0;
			} else {
				m_cursor_pos = Text.size();
				new_mark_begin = 0;
				new_mark_end = 0;
			}
			break;
		default:
			return false;
		}
	} else {
		switch (event.KeyInput.Key) {
		case KEY_END: {
			s32 p = Text.size();
			if (m_word_wrap || m_multiline) {
				p = getLineFromPos(m_cursor_pos);
				p = m_broken_text_positions[p] +
				    (s32)m_broken_text[p].size();
				if (p > 0 && (Text[p - 1] == L'\r' ||
							     Text[p - 1] == L'\n'))
					p -= 1;
			}

			if (event.KeyInput.Shift) {
				if (m_mark_begin == m_mark_end)
					new_mark_begin = m_cursor_pos;

				new_mark_end = p;
			} else {
				new_mark_begin = 0;
				new_mark_end = 0;
			}
			m_cursor_pos = p;
			m_blink_start_time = porting::getTimeMs();
		} break;
		case KEY_HOME: {

			s32 p = 0;
			if (m_word_wrap || m_multiline) {
				p = getLineFromPos(m_cursor_pos);
				p = m_broken_text_positions[p];
			}

			if (event.KeyInput.Shift) {
				if (m_mark_begin == m_mark_end)
					new_mark_begin = m_cursor_pos;
				new_mark_end = p;
			} else {
				new_mark_begin = 0;
				new_mark_end = 0;
			}
			m_cursor_pos = p;
			m_blink_start_time = porting::getTimeMs();
		} break;
		case KEY_RETURN:
			if (m_multiline) {
				inputChar(L'\n');
			} else {
				calculateScrollPos();
				sendGuiEvent(EGET_EDITBOX_ENTER);
			}
			return true;
		case KEY_LEFT:
			if (event.KeyInput.Shift) {
				if (m_cursor_pos > 0) {
					if (m_mark_begin == m_mark_end)
						new_mark_begin = m_cursor_pos;

					new_mark_end = m_cursor_pos - 1;
				}
			} else {
				new_mark_begin = 0;
				new_mark_end = 0;
			}

			if (m_cursor_pos > 0)
				m_cursor_pos--;
			m_blink_start_time = porting::getTimeMs();
			break;
		case KEY_RIGHT:
			if (event.KeyInput.Shift) {
				if (Text.size() > (u32)m_cursor_pos) {
					if (m_mark_begin == m_mark_end)
						new_mark_begin = m_cursor_pos;

					new_mark_end = m_cursor_pos + 1;
				}
			} else {
				new_mark_begin = 0;
				new_mark_end = 0;
			}

			if (Text.size() > (u32)m_cursor_pos)
				m_cursor_pos++;
			m_blink_start_time = porting::getTimeMs();
			break;
		case KEY_UP:
			if (!onKeyUp(event, new_mark_begin, new_mark_end)) {
				return false;
			}
			break;
		case KEY_DOWN:
			if (!onKeyDown(event, new_mark_begin, new_mark_end)) {
				return false;
			}
			break;
		case KEY_BACK:
			text_changed = onKeyBack(event, new_mark_begin, new_mark_end);
			break;

		case KEY_DELETE:
			text_changed = onKeyDelete(event, new_mark_begin, new_mark_end);
			break;

		case KEY_ESCAPE:
		case KEY_TAB:
		case KEY_SHIFT:
		case KEY_F1:
		case KEY_F2:
		case KEY_F3:
		case KEY_F4:
		case KEY_F5:
		case KEY_F6:
		case KEY_F7:
		case KEY_F8:
		case KEY_F9:
		case KEY_F10:
		case KEY_F11:
		case KEY_F12:
		case KEY_F13:
		case KEY_F14:
		case KEY_F15:
		case KEY_F16:
		case KEY_F17:
		case KEY_F18:
		case KEY_F19:
		case KEY_F20:
		case KEY_F21:
		case KEY_F22:
		case KEY_F23:
		case KEY_F24:
			// ignore these keys
			return false;

		default:
			inputChar(event.KeyInput.Char);
			return true;
		}
	}

	// Set new text markers
	setTextMarkers(new_mark_begin, new_mark_end);

	// break the text if it has changed
	if (text_changed) {
		breakText();
		sendGuiEvent(EGET_EDITBOX_CHANGED);
	}

	calculateScrollPos();

	return true;
}

bool GUIEditBox::onKeyUp(const SEvent &event, s32 &mark_begin, s32 &mark_end)
{
	if (m_multiline || (m_word_wrap && m_broken_text.size() > 1)) {
		s32 lineNo = getLineFromPos(m_cursor_pos);
		s32 mb = (m_mark_begin == m_mark_end) ? m_cursor_pos :
			(m_mark_begin > m_mark_end ? m_mark_begin : m_mark_end);
		if (lineNo > 0) {
			s32 cp = m_cursor_pos - m_broken_text_positions[lineNo];
			if ((s32)m_broken_text[lineNo - 1].size() < cp) {
				m_cursor_pos = m_broken_text_positions[lineNo - 1] +
					core::max_((u32)1, m_broken_text[lineNo - 1].size()) - 1;
			}
			else
				m_cursor_pos = m_broken_text_positions[lineNo - 1] + cp;
		}

		if (event.KeyInput.Shift) {
			mark_begin = mb;
			mark_end = m_cursor_pos;
		} else {
			mark_begin = 0;
			mark_end = 0;
		}

		return true;
	}

	return false;
}

bool GUIEditBox::onKeyDown(const SEvent &event, s32 &mark_begin, s32 &mark_end)
{
	if (m_multiline || (m_word_wrap && m_broken_text.size() > 1)) {
		s32 lineNo = getLineFromPos(m_cursor_pos);
		s32 mb = (m_mark_begin == m_mark_end) ? m_cursor_pos :
			(m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end);
		if (lineNo < (s32)m_broken_text.size() - 1) {
			s32 cp = m_cursor_pos - m_broken_text_positions[lineNo];
			if ((s32)m_broken_text[lineNo + 1].size() < cp) {
				m_cursor_pos = m_broken_text_positions[lineNo + 1] +
					core::max_((u32)1, m_broken_text[lineNo + 1].size()) - 1;
			}
			else
				m_cursor_pos = m_broken_text_positions[lineNo + 1] + cp;
		}

		if (event.KeyInput.Shift) {
			mark_begin = mb;
			mark_end = m_cursor_pos;
		} else {
			mark_begin = 0;
			mark_end = 0;
		}

		return true;
	}

	return false;
}

void GUIEditBox::onKeyControlC(const SEvent &event)
{
	// copy to clipboard
	if (m_passwordbox || !m_operator || m_mark_begin == m_mark_end)
		return;

	const s32 realmbgn = m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
	const s32 realmend = m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;

	std::string s = stringw_to_utf8(Text.subString(realmbgn, realmend - realmbgn));
	m_operator->copyToClipboard(s.c_str());
}

bool GUIEditBox::onKeyControlX(const SEvent &event, s32 &mark_begin, s32 &mark_end)
{
	// First copy to clipboard
	onKeyControlC(event);

	if (!m_writable)
		return false;

	if (m_passwordbox || !m_operator || m_mark_begin == m_mark_end)
		return false;

	const s32 realmbgn = m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
	const s32 realmend = m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;

	// Now remove from box if enabled
	if (isEnabled()) {
		// delete
		core::stringw s;
		s = Text.subString(0, realmbgn);
		s.append(Text.subString(realmend, Text.size() - realmend));
		Text = s;

		m_cursor_pos = realmbgn;
		mark_begin = 0;
		mark_end = 0;
		return true;
	}

	return false;
}

bool GUIEditBox::onKeyControlV(const SEvent &event, s32 &mark_begin, s32 &mark_end)
{
	if (!isEnabled() || !m_writable)
		return false;

	// paste from the clipboard
	if (!m_operator)
		return false;

	const s32 realmbgn = m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
	const s32 realmend = m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;

	// add new character
	if (const c8 *p = m_operator->getTextFromClipboard()) {
		core::stringw inserted_text = utf8_to_stringw(p);
		if (m_mark_begin == m_mark_end) {
			// insert text
			core::stringw s = Text.subString(0, m_cursor_pos);
			s.append(inserted_text);
			s.append(Text.subString(
					m_cursor_pos, Text.size() - m_cursor_pos));

			if (!m_max || s.size() <= m_max) {
				Text = s;
				m_cursor_pos += inserted_text.size();
			}
		} else {
			// replace text

			core::stringw s = Text.subString(0, realmbgn);
			s.append(inserted_text);
			s.append(Text.subString(realmend, Text.size() - realmend));

			if (!m_max || s.size() <= m_max) {
				Text = s;
				m_cursor_pos = realmbgn + inserted_text.size();
			}
		}
	}

	mark_begin = 0;
	mark_end = 0;
	return true;
}

bool GUIEditBox::onKeyBack(const SEvent &event, s32 &mark_begin, s32 &mark_end)
{
	if (!isEnabled() || Text.empty() || !m_writable)
		return false;

	core::stringw s;

	if (m_mark_begin != m_mark_end) {
		// delete marked text
		const s32 realmbgn =
				m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
		const s32 realmend =
				m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;

		s = Text.subString(0, realmbgn);
		s.append(Text.subString(realmend, Text.size() - realmend));
		Text = s;

		m_cursor_pos = realmbgn;
	} else {
		// delete text behind cursor
		if (m_cursor_pos > 0)
			s = Text.subString(0, m_cursor_pos - 1);
		else
			s = L"";
		s.append(Text.subString(m_cursor_pos, Text.size() - m_cursor_pos));
		Text = s;
		--m_cursor_pos;
	}

	if (m_cursor_pos < 0)
		m_cursor_pos = 0;
	m_blink_start_time = porting::getTimeMs(); // os::Timer::getTime();
	mark_begin = 0;
	mark_end = 0;
	return true;
}

bool GUIEditBox::onKeyDelete(const SEvent &event, s32 &mark_begin, s32 &mark_end)
{
	if (!isEnabled() || Text.empty() || !m_writable)
		return false;

	core::stringw s;

	if (m_mark_begin != m_mark_end) {
		// delete marked text
		const s32 realmbgn =
				m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
		const s32 realmend =
				m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;

		s = Text.subString(0, realmbgn);
		s.append(Text.subString(realmend, Text.size() - realmend));
		Text = s;

		m_cursor_pos = realmbgn;
	} else {
		// delete text before cursor
		s = Text.subString(0, m_cursor_pos);
		s.append(Text.subString(
				m_cursor_pos + 1, Text.size() - m_cursor_pos - 1));
		Text = s;
	}

	if (m_cursor_pos > (s32)Text.size())
		m_cursor_pos = (s32)Text.size();

	m_blink_start_time = porting::getTimeMs(); // os::Timer::getTime();
	mark_begin = 0;
	mark_end = 0;
	return true;
}

void GUIEditBox::inputChar(wchar_t c)
{
	if (c == 0)
		return;
	core::stringw s(&c, 1);
	inputString(s);
}

void GUIEditBox::inputString(const core::stringw &str)
{
	if (!isEnabled() || !m_writable)
		return;

	u32 len = str.size();
	if (Text.size()+len <= m_max || m_max == 0) {
		core::stringw s;
		if (m_mark_begin != m_mark_end) {
			// replace marked text
			s32 real_begin = m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
			s32 real_end = m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;

			s = Text.subString(0, real_begin);
			s.append(str);
			s.append(Text.subString(real_end, Text.size() - real_end));
			Text = s;
			m_cursor_pos = real_begin + len;
		} else {
			// append string
			s = Text.subString(0, m_cursor_pos);
			s.append(str);
			s.append(Text.subString(m_cursor_pos,
					Text.size() - m_cursor_pos));
			Text = s;
			m_cursor_pos += len;
		}

		m_blink_start_time = porting::getTimeMs();
		setTextMarkers(0, 0);
	}

	breakText();
	sendGuiEvent(EGET_EDITBOX_CHANGED);
	calculateScrollPos();
}

bool GUIEditBox::processMouse(const SEvent &event)
{
	switch (event.MouseInput.Event) {
	case EMIE_LMOUSE_LEFT_UP:
		if (Environment->hasFocus(this)) {
			m_cursor_pos = getCursorPos(
					event.MouseInput.X, event.MouseInput.Y);
			if (m_mouse_marking) {
				setTextMarkers(m_mark_begin, m_cursor_pos);
			}
			m_mouse_marking = false;
			calculateScrollPos();
			return true;
		}
		break;
	case EMIE_MOUSE_MOVED: {
		if (m_mouse_marking) {
			m_cursor_pos = getCursorPos(
					event.MouseInput.X, event.MouseInput.Y);
			setTextMarkers(m_mark_begin, m_cursor_pos);
			calculateScrollPos();
			return true;
		}
	} break;
	case EMIE_LMOUSE_PRESSED_DOWN:

		if (!Environment->hasFocus(this)) {
			m_blink_start_time = porting::getTimeMs();
			m_mouse_marking = true;
			m_cursor_pos = getCursorPos(
					event.MouseInput.X, event.MouseInput.Y);
			setTextMarkers(m_cursor_pos, m_cursor_pos);
			calculateScrollPos();
			return true;
		} else {
			if (!AbsoluteClippingRect.isPointInside(core::position2d<s32>(
					    event.MouseInput.X, event.MouseInput.Y))) {
				return false;
			} else {
				// move cursor
				m_cursor_pos = getCursorPos(
						event.MouseInput.X, event.MouseInput.Y);

				s32 newMarkBegin = m_mark_begin;
				if (!m_mouse_marking)
					newMarkBegin = m_cursor_pos;

				m_mouse_marking = true;
				setTextMarkers(newMarkBegin, m_cursor_pos);
				calculateScrollPos();
				return true;
			}
		}
	case EMIE_MOUSE_WHEEL:
		if (m_vscrollbar && m_vscrollbar->isVisible()) {
			s32 pos = m_vscrollbar->getTargetPos();
			s32 step = m_vscrollbar->getSmallStep();
			m_vscrollbar->setPosInterpolated(pos - event.MouseInput.Wheel * step);
			return true;
		}
		break;
	case EMIE_MMOUSE_PRESSED_DOWN: {
		if (!AbsoluteClippingRect.isPointInside(core::position2d<s32>(
					event.MouseInput.X, event.MouseInput.Y)))
			return false;

		if (!Environment->hasFocus(this)) {
			m_blink_start_time = porting::getTimeMs();
		}

		// move cursor and disable marking
		m_cursor_pos = getCursorPos(event.MouseInput.X, event.MouseInput.Y);
		m_mouse_marking = false;
		setTextMarkers(m_cursor_pos, m_cursor_pos);

		// paste from the primary selection
		inputString([&] {
			if (!m_operator)
				return core::stringw();
			const c8 *inserted_text_utf8 = m_operator->getTextFromPrimarySelection();
			if (!inserted_text_utf8)
				return core::stringw();
			return utf8_to_stringw(inserted_text_utf8);
		}());

		return true;
	}
	default:
		break;
	}

	return false;
}

s32 GUIEditBox::getLineFromPos(s32 pos)
{
	if (!m_word_wrap && !m_multiline)
		return 0;

	s32 i = 0;
	while (i < (s32)m_broken_text_positions.size()) {
		if (m_broken_text_positions[i] > pos)
			return i - 1;
		++i;
	}
	return (s32)m_broken_text_positions.size() - 1;
}

void GUIEditBox::updateVScrollBar()
{
	if (!m_vscrollbar) {
		return;
	}

	// OnScrollBarChanged(...)
	if (m_vscrollbar->getPos() != m_vscroll_pos) {
		s32 deltaScrollY = m_vscrollbar->getPos() - m_vscroll_pos;
		m_current_text_rect.UpperLeftCorner.Y -= deltaScrollY;
		m_current_text_rect.LowerRightCorner.Y -= deltaScrollY;

		s32 scrollymax = getTextDimension().Height - m_frame_rect.getHeight();
		if (scrollymax != m_vscrollbar->getMax()) {
			// manage a newline or a deleted line
			m_vscrollbar->setMax(scrollymax);
			m_vscrollbar->setPageSize(s32(getTextDimension().Height));
			calculateScrollPos();
		} else {
			// manage a newline or a deleted line
			m_vscroll_pos = m_vscrollbar->getPos();
		}
	}

	// check if a vertical scrollbar is needed ?
	if (getTextDimension().Height > (u32)m_frame_rect.getHeight()) {
		m_frame_rect.LowerRightCorner.X -= m_scrollbar_width;

		s32 scrollymax = getTextDimension().Height - m_frame_rect.getHeight();
		if (scrollymax != m_vscrollbar->getMax()) {
			m_vscrollbar->setMax(scrollymax);
			m_vscrollbar->setPageSize(s32(getTextDimension().Height));
		}

		if (!m_vscrollbar->isVisible()) {
			m_vscrollbar->setVisible(true);
		}
	} else {
		if (m_vscrollbar->isVisible()) {
			m_vscrollbar->setVisible(false);
			m_vscroll_pos = 0;
			m_vscrollbar->setPos(0);
			m_vscrollbar->setMax(1);
			m_vscrollbar->setPageSize(s32(getTextDimension().Height));
		}
	}
}
