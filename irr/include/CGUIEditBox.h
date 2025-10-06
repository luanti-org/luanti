// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIEditBox.h"
#include "irrArray.h"
#include "IOSOperator.h"

namespace gui
{

class IGUIScrollBar;

class CGUIEditBox : public IGUIEditBox
{
public:
	//! constructor
	CGUIEditBox(const wchar_t *text, bool border, IGUIEnvironment *environment,
			IGUIElement *parent, s32 id, const core::rect<s32> &rectangle);

	//! destructor
	virtual ~CGUIEditBox();

	//! Sets another skin independent font.
	void setOverrideFont(IGUIFont *font = 0) override;

	//! Gets the override font (if any)
	/** \return The override font (may be 0) */
	IGUIFont *getOverrideFont() const override;

	//! Get the font which is used right now for drawing
	/** Currently this is the override font when one is set and the
	font of the active skin otherwise */
	IGUIFont *getActiveFont() const override;

	//! Sets another color for the text.
	void setOverrideColor(video::SColor color) override;

	//! Gets the override color
	video::SColor getOverrideColor() const override;

	//! Sets if the text should use the override color or the
	//! color in the gui skin.
	void enableOverrideColor(bool enable) override;

	//! Checks if an override color is enabled
	/** \return true if the override color is enabled, false otherwise */
	bool isOverrideColorEnabled(void) const override;

	//! Sets whether to draw the background
	void setDrawBackground(bool draw) override;

	//! Checks if background drawing is enabled
	bool isDrawBackgroundEnabled() const override;

	//! Turns the border on or off
	void setDrawBorder(bool border) override;

	//! Checks if border drawing is enabled
	bool isDrawBorderEnabled() const override;

	//! Enables or disables word wrap for using the edit box as multiline text editor.
	void setWordWrap(bool enable) override;

	//! Checks if word wrap is enabled
	//! \return true if word wrap is enabled, false otherwise
	bool isWordWrapEnabled() const override;

	//! Enables or disables newlines.
	/** \param enable: If set to true, the EGET_EDITBOX_ENTER event will not be fired,
	instead a newline character will be inserted. */
	void setMultiLine(bool enable) override;

	//! Checks if multi line editing is enabled
	//! \return true if mult-line is enabled, false otherwise
	bool isMultiLineEnabled() const override;

	//! Enables or disables automatic scrolling with cursor position
	//! \param enable: If set to true, the text will move around with the cursor position
	void setAutoScroll(bool enable) override;

	//! Checks to see if automatic scrolling is enabled
	//! \return true if automatic scrolling is enabled, false if not
	bool isAutoScrollEnabled() const override;

	//! Gets the size area of the text in the edit box
	//! \return Returns the size in pixels of the text
	core::dimension2du getTextDimension() override;

	//! Sets text justification
	void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical) override;

	//! called if an event happened.
	bool OnEvent(const SEvent &event) override;

	//! draws the element and its children
	void draw() override;

	//! Sets the new caption of this element.
	void setText(const wchar_t *text) override;

	//! Sets the maximum amount of characters which may be entered in the box.
	//! \param max: Maximum amount of characters. If 0, the character amount is
	//! infinity.
	void setMax(u32 max) override;

	//! Returns maximum amount of characters, previously set by setMax();
	u32 getMax() const override;

	//! Set the character used for the cursor.
	/** By default it's "_" */
	void setCursorChar(const wchar_t cursorChar) override;

	//! Get the character used for the cursor.
	wchar_t getCursorChar() const override;

	//! Set the blinktime for the cursor. 2x blinktime is one full cycle.
	//** \param timeMs Blinktime in milliseconds. When set to 0 the cursor is constantly on without blinking */
	void setCursorBlinkTime(u32 timeMs) override;

	//! Get the cursor blinktime
	u32 getCursorBlinkTime() const override;

	//! Sets whether the edit box is a password box. Setting this to true will
	/** disable MultiLine, WordWrap and the ability to copy with ctrl+c or ctrl+x
	\param passwordBox: true to enable password, false to disable
	\param passwordChar: the character that is displayed instead of letters */
	void setPasswordBox(bool passwordBox, wchar_t passwordChar = L'*') override;

	//! Returns true if the edit box is currently a password box.
	bool isPasswordBox() const override;

	//! Updates the absolute position, splits text if required
	void updateAbsolutePosition() override;

	//! Returns whether the element takes input from the IME
	bool acceptsIME() override { return isEnabled() && IsWritable; };

	//! set true if this EditBox is writable
	void setWritable(bool writable) { IsWritable = writable; }

protected:
	//! Breaks the single text line.
	void breakText();
	//! sets the area of the given line
	void setTextRect(s32 line);
	//! returns the line number that the cursor is on
	s32 getLineFromPos(s32 pos);
	//! adds a letter to the edit box
	void inputChar(wchar_t c);
	//! adds a string to the edit box
	void inputString(const core::stringw &str);
	//! calculates the current scroll position
	void calculateScrollPos();
	//! calculated the FrameRect
	void calculateFrameRect();
	//! send some gui event to parent
	void sendGuiEvent(EGUI_EVENT_TYPE type);
	//! set text markers
	void setTextMarkers(s32 begin, s32 end);
	//! update the vertical scrollBar (visibilty & position)
	void updateVScrollBar();

	bool processKey(const SEvent &event);
	//! KEY_LEFT / KEY_RIGHT inputs
	void processKeyLR(const SEvent::SKeyInput &input, s32 &new_mark_begin,
			s32 &new_mark_end);

	bool onKeyUp(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	bool onKeyDown(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	void onKeyControlC(const SEvent &event);
	bool onKeyControlX(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	bool onKeyControlV(const SEvent &event, s32 &mark_begin, s32 &mark_end);
	bool onKeyBack();
	//! delete current selection or next char
	bool onKeyDelete();

	bool processMouse(const SEvent &event);
	s32 getCursorPos(s32 x, s32 y);

	bool OverwriteMode;
	bool MouseMarking;
	bool Border;
	bool Background;
	bool OverrideColorEnabled;
	s32 MarkBegin;
	s32 MarkEnd;

	video::SColor OverrideBgColor = 0;
	video::SColor OverrideColor;
	gui::IGUIFont *OverrideFont, *LastBreakFont;
	IOSOperator *Operator;

	u32 BlinkStartTime;
	u32 CursorBlinkTime;
	core::stringw CursorChar; // IGUIFont::draw needs stringw instead of wchar_t
	s32 CursorPos;
	s32 HScrollPos, VScrollPos; // scroll position in characters
	u32 Max;
	u32 VScrollBarWidth = 0;
	IGUIScrollBar *VScrollBar = nullptr;

	bool WordWrap = false,
		MultiLine = false,
		AutoScroll = true,
		PasswordBox = false,
		IsWritable = true;

	wchar_t PasswordChar;
	EGUI_ALIGNMENT HAlign, VAlign;

	core::array<core::stringw> BrokenText;
	core::array<s32> BrokenTextPositions;

	core::rect<s32> CurrentTextRect, FrameRect; // temporary values
};

} // end namespace gui
