// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Minetest

#pragma once

#include "irrlichttypes.h"
#include "IGUIEditBox.h"
#include "IOSOperator.h"
#include "guiScrollBar.h"
#include <vector>


using namespace gui;

class GUIEditBox : public IGUIEditBox
{
public:
	GUIEditBox(IGUIEnvironment *environment, IGUIElement *parent, s32 id,
			core::rect<s32> rectangle, bool border, bool writable) :
			IGUIEditBox(environment, parent, id, rectangle),
			m_border(border), m_writable(writable), m_frame_rect(rectangle)
	{
	}

	virtual ~GUIEditBox();

	//! Sets another skin independent font.
	virtual void setOverrideFont(IGUIFont *font = 0);

	virtual IGUIFont *getOverrideFont() const { return m_override_font; }

	//! Get the font which is used right now for drawing
	/** Currently this is the override font when one is set and the
	font of the active skin otherwise */
	virtual IGUIFont *getActiveFont() const;

	//! Sets another color for the text.
	virtual void setOverrideColor(video::SColor color);

	//! Gets the override color
	virtual video::SColor getOverrideColor() const;

	//! Sets if the text should use the overide color or the
	//! color in the gui skin.
	virtual void enableOverrideColor(bool enable);

	//! Checks if an override color is enabled
	/** \return true if the override color is enabled, false otherwise */
	virtual bool isOverrideColorEnabled(void) const
	{
		return m_override_color_enabled;
	}

	//! Enables or disables word wrap for using the edit box as multiline text editor.
	virtual void setWordWrap(bool enable);

	//! Checks if word wrap is enabled
	//! \return true if word wrap is enabled, false otherwise
	virtual bool isWordWrapEnabled() const { return m_word_wrap; }

	//! Turns the border on or off
	virtual void setDrawBorder(bool border);

	virtual bool isDrawBorderEnabled() const { return m_border; }

	//! Enables or disables newlines.
	/** \param enable: If set to true, the EGET_EDITBOX_ENTER event will not be fired,
	instead a newline character will be inserted. */
	virtual void setMultiLine(bool enable);

	//! Checks if multi line editing is enabled
	//! \return true if mult-line is enabled, false otherwise
	virtual bool isMultiLineEnabled() const { return m_multiline; }

	//! Enables or disables automatic scrolling with cursor position
	//! \param enable: If set to true, the text will move around with the cursor
	//! position
	virtual void setAutoScroll(bool enable);

	//! Checks to see if automatic scrolling is enabled
	//! \return true if automatic scrolling is enabled, false if not
	virtual bool isAutoScrollEnabled() const { return m_autoscroll; }

	//! Sets whether the edit box is a password box. Setting this to true will
	/** disable MultiLine, WordWrap and the ability to copy with ctrl+c or ctrl+x
	\param passwordBox: true to enable password, false to disable
	\param passwordChar: the character that is displayed instead of letters */
	virtual void setPasswordBox(bool passwordBox, wchar_t passwordChar = L'*');

	//! Returns true if the edit box is currently a password box.
	virtual bool isPasswordBox() const { return m_passwordbox; }

	//! Sets text justification
	virtual void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical);

	//! Sets the new caption of this element.
	virtual void setText(const wchar_t *text);

	//! Sets the maximum amount of characters which may be entered in the box.
	//! \param max: Maximum amount of characters. If 0, the character amount is
	//! infinity.
	virtual void setMax(u32 max);

	//! Returns maximum amount of characters, previously set by setMax();
	virtual u32 getMax() const { return m_max; }

	//! Gets the size area of the text in the edit box
	//! \return Returns the size in pixels of the text
	virtual core::dimension2du getTextDimension();

	//! set true if this EditBox is writable
	virtual void setWritable(bool can_write_text);

	//! called if an event happened.
	virtual bool OnEvent(const SEvent &event);

	virtual bool acceptsIME() { return isEnabled() && m_writable; };

protected:
	virtual void breakText() = 0;

	//! sets the area of the given line
	virtual void setTextRect(s32 line) = 0;

	//! set text markers
	void setTextMarkers(s32 begin, s32 end);

	//! send some gui event to parent
	void sendGuiEvent(EGUI_EVENT_TYPE type);

	//! calculates the current scroll position
	virtual void calculateScrollPos() = 0;

	virtual s32 getCursorPos(s32 x, s32 y) = 0;

	bool processKey(const SEvent &event);
	virtual void inputString(const core::stringw &str);
	virtual void inputChar(wchar_t c);

	//! returns the line number that the cursor is on
	s32 getLineFromPos(s32 pos);

	//! update the vertical scrollBar (visibilty & position)
	void updateVScrollBar();

	gui::IGUIFont *m_override_font = nullptr;

	bool m_override_color_enabled = false;
	bool m_word_wrap = false;
	bool m_multiline = false;
	bool m_autoscroll = true;

	bool m_border;

	bool m_passwordbox = false;
	wchar_t m_passwordchar = L'*';

	std::vector<core::stringw> m_broken_text;
	std::vector<s32> m_broken_text_positions;

	EGUI_ALIGNMENT m_halign = EGUIA_UPPERLEFT;
	EGUI_ALIGNMENT m_valign = EGUIA_CENTER;

	u32 m_blink_start_time = 0;
	s32 m_cursor_pos = 0;
	s32 m_hscroll_pos = 0;
	s32 m_vscroll_pos = 0; // scroll position in characters
	u32 m_max = 0;

	video::SColor m_override_color = video::SColor(101, 255, 255, 255);

	core::rect<s32> m_current_text_rect = core::rect<s32>(0, 0, 1, 1);

	bool m_writable;

	bool m_mouse_marking = false;

	s32 m_mark_begin = 0;
	s32 m_mark_end = 0;

	gui::IGUIFont *m_last_break_font = nullptr;
	IOSOperator *m_operator = nullptr;

	core::rect<s32> m_frame_rect; // temporary values

	u32 m_scrollbar_width = 0;
	GUIScrollBar *m_vscrollbar = nullptr;

private:
	bool processMouse(const SEvent &event);

	bool onKeyUp(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	bool onKeyDown(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	void onKeyControlC(const SEvent &event);
	bool onKeyControlX(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	bool onKeyControlV(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	bool onKeyBack(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	bool onKeyDelete(const SEvent &event, s32 &mark_begin, s32 &mark_end);
};
