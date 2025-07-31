// Copyright (C) 2002-2012 Nikolaus Gebhardt
// Modified by Mustapha T.
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "guiEditBoxWithScrollbar.h"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "IVideoDriver.h"
#include "rect.h"
#include "porting.h"
#include "Keycodes.h"

/*
todo:
optional scrollbars [done]
ctrl+left/right to select word
double click/ctrl click: word select + drag to select whole words, triple click to select line
optional? dragging selected text
numerical
*/

//! constructor
GUIEditBoxWithScrollBar::GUIEditBoxWithScrollBar(const wchar_t* text, bool border,
	IGUIEnvironment* environment, IGUIElement* parent, s32 id,
	const core::rect<s32>& rectangle, ISimpleTextureSource *tsrc,
	bool writable, bool has_vscrollbar)
	: GUIEditBox(environment, parent, id, rectangle, border, writable),
	m_background(true), m_bg_color_used(false), m_tsrc(tsrc)
{

	Text = text;

	if (Environment)
		m_operator = Environment->getOSOperator();

	if (m_operator)
		m_operator->grab();

	// this element can be tabbed to
	setTabStop(true);
	setTabOrder(-1);

	if (has_vscrollbar) {
		createVScrollBar();
	}

	calculateFrameRect();
	breakText();

	calculateScrollPos();
	setWritable(writable);
}

//! Sets whether to draw the background
void GUIEditBoxWithScrollBar::setDrawBackground(bool draw)
{
	m_background = draw;
}


void GUIEditBoxWithScrollBar::updateAbsolutePosition()
{
	core::rect<s32> old_absolute_rect(AbsoluteRect);
	IGUIElement::updateAbsolutePosition();
	if (old_absolute_rect != AbsoluteRect) {
		calculateFrameRect();
		breakText();
		calculateScrollPos();
	}
}


//! draws the element and its children
void GUIEditBoxWithScrollBar::draw()
{
	if (!IsVisible)
		return;

	const bool focus = Environment->hasFocus(this);

	IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;

	video::SColor default_bg_color;
	video::SColor bg_color;

	default_bg_color = m_writable ? skin->getColor(EGDC_WINDOW) : video::SColor(0);
	bg_color = m_bg_color_used ? m_bg_color : default_bg_color;

	if (!m_border && m_background) {
		skin->draw2DRectangle(this, bg_color, AbsoluteRect, &AbsoluteClippingRect);
	}

	// draw the border

	if (m_border) {

		if (m_writable) {
			skin->draw3DSunkenPane(this, bg_color, false, m_background,
				AbsoluteRect, &AbsoluteClippingRect);
		}
	}
	calculateFrameRect();

	core::rect<s32> local_clip_rect = m_frame_rect;
	local_clip_rect.clipAgainst(AbsoluteClippingRect);

	// draw the text

	IGUIFont* font = getActiveFont();

	s32 cursor_line = 0;
	s32 charcursorpos = 0;

	if (font) {
		if (m_last_break_font != font) {
			breakText();
		}

		// calculate cursor pos

		core::stringw *txt_line = &Text;
		s32 start_pos = 0;

		core::stringw s, s2;

		// get mark position
		const bool ml = (!m_passwordbox && (m_word_wrap || m_multiline));
		const s32 realmbgn = m_mark_begin < m_mark_end ? m_mark_begin : m_mark_end;
		const s32 realmend = m_mark_begin < m_mark_end ? m_mark_end : m_mark_begin;
		const s32 hline_start = ml ? getLineFromPos(realmbgn) : 0;
		const s32 hline_count = ml ? getLineFromPos(realmend) - hline_start + 1 : 1;
		const s32 line_count = ml ? m_broken_text.size() : 1;

		// Save the override color information.
		// Then, alter it if the edit box is disabled.
		const bool prevOver = m_override_color_enabled;
		const video::SColor prevColor = m_override_color;

		if (Text.size()) {
			if (!isEnabled() && !m_override_color_enabled) {
				m_override_color_enabled = true;
				m_override_color = skin->getColor(EGDC_GRAY_TEXT);
			}

			for (s32 i = 0; i < line_count; ++i) {
				setTextRect(i);

				// clipping test - don't draw anything outside the visible area
				core::rect<s32> c = local_clip_rect;
				c.clipAgainst(m_current_text_rect);
				if (!c.isValid())
					continue;

				// get current line
				if (m_passwordbox) {
					if (m_broken_text.size() != 1) {
						m_broken_text.clear();
						m_broken_text.emplace_back();
					}
					if (m_broken_text[0].size() != Text.size()){
						m_broken_text[0] = Text;
						for (u32 q = 0; q < Text.size(); ++q)
						{
							m_broken_text[0][q] = m_passwordchar;
						}
					}
					txt_line = &m_broken_text[0];
					start_pos = 0;
				} else {
					txt_line = ml ? &m_broken_text[i] : &Text;
					start_pos = ml ? m_broken_text_positions[i] : 0;
				}


				// draw normal text
				font->draw(txt_line->c_str(), m_current_text_rect,
					m_override_color_enabled ? m_override_color : skin->getColor(EGDC_BUTTON_TEXT),
					false, true, &local_clip_rect);

				// draw mark and marked text
				if (focus && m_mark_begin != m_mark_end && i >= hline_start && i < hline_start + hline_count) {

					s32 mbegin = 0, mend = 0;
					s32 lineStartPos = 0, lineEndPos = txt_line->size();

					if (i == hline_start) {
						// highlight start is on this line
						s = txt_line->subString(0, realmbgn - start_pos);
						mbegin = font->getDimension(s.c_str()).Width;

						// deal with kerning
						mbegin += font->getKerning(
							(*txt_line)[realmbgn - start_pos],
							realmbgn - start_pos > 0 ? (*txt_line)[realmbgn - start_pos - 1] : 0).X;

						lineStartPos = realmbgn - start_pos;
					}
					if (i == hline_start + hline_count - 1) {
						// highlight end is on this line
						s2 = txt_line->subString(0, realmend - start_pos);
						mend = font->getDimension(s2.c_str()).Width;
						lineEndPos = (s32)s2.size();
					} else {
						mend = font->getDimension(txt_line->c_str()).Width;
					}


					m_current_text_rect.UpperLeftCorner.X += mbegin;
					m_current_text_rect.LowerRightCorner.X = m_current_text_rect.UpperLeftCorner.X + mend - mbegin;


					// draw mark
					skin->draw2DRectangle(this, skin->getColor(EGDC_HIGH_LIGHT), m_current_text_rect, &local_clip_rect);

					// draw marked text
					s = txt_line->subString(lineStartPos, lineEndPos - lineStartPos);

					if (s.size())
						font->draw(s.c_str(), m_current_text_rect,
							m_override_color_enabled ? m_override_color : skin->getColor(EGDC_HIGH_LIGHT_TEXT),
							false, true, &local_clip_rect);

				}
			}

			// Return the override color information to its previous settings.
			m_override_color_enabled = prevOver;
			m_override_color = prevColor;
		}

		// draw cursor
		if (IsEnabled && m_writable) {
			if (m_word_wrap || m_multiline) {
				cursor_line = getLineFromPos(m_cursor_pos);
				txt_line = &m_broken_text[cursor_line];
				start_pos = m_broken_text_positions[cursor_line];
			}
			s = txt_line->subString(0, m_cursor_pos - start_pos);
			charcursorpos = font->getDimension(s.c_str()).Width +
				font->getKerning(L'_',
					m_cursor_pos - start_pos > 0 ? (*txt_line)[m_cursor_pos - start_pos - 1] : 0).X;

			if (focus && (porting::getTimeMs() - m_blink_start_time) % 700 < 350) {
				setTextRect(cursor_line);
				m_current_text_rect.UpperLeftCorner.X += charcursorpos;

				font->draw(L"_", m_current_text_rect,
					m_override_color_enabled ? m_override_color : skin->getColor(EGDC_BUTTON_TEXT),
					false, true, &local_clip_rect);
			}
		}
	}

	// draw children
	IGUIElement::draw();
}


s32 GUIEditBoxWithScrollBar::getCursorPos(s32 x, s32 y)
{
	IGUIFont* font = getActiveFont();

	const u32 line_count = (m_word_wrap || m_multiline) ? m_broken_text.size() : 1;

	core::stringw *txt_line = 0;
	s32 start_pos = 0;
	x += 3;

	for (u32 i = 0; i < line_count; ++i) {
		setTextRect(i);
		if (i == 0 && y < m_current_text_rect.UpperLeftCorner.Y)
			y = m_current_text_rect.UpperLeftCorner.Y;
		if (i == line_count - 1 && y > m_current_text_rect.LowerRightCorner.Y)
			y = m_current_text_rect.LowerRightCorner.Y;

		// is it inside this region?
		if (y >= m_current_text_rect.UpperLeftCorner.Y && y <= m_current_text_rect.LowerRightCorner.Y) {
			// we've found the clicked line
			txt_line = (m_word_wrap || m_multiline) ? &m_broken_text[i] : &Text;
			start_pos = (m_word_wrap || m_multiline) ? m_broken_text_positions[i] : 0;
			break;
		}
	}

	if (x < m_current_text_rect.UpperLeftCorner.X)
		x = m_current_text_rect.UpperLeftCorner.X;

	if (!txt_line)
		return 0;

	s32 idx = font->getCharacterFromPos(txt_line->c_str(), x - m_current_text_rect.UpperLeftCorner.X);

	// click was on or left of the line
	if (idx != -1)
		return idx + start_pos;

	// click was off the right edge of the line, go to end.
	return txt_line->size() + start_pos;
}


//! Breaks the single text line.
void GUIEditBoxWithScrollBar::breakText()
{
	if ((!m_word_wrap && !m_multiline))
		return;

	m_broken_text.clear(); // need to reallocate :/
	m_broken_text_positions.clear();

	IGUIFont* font = getActiveFont();
	if (!font)
		return;

	m_last_break_font = font;

	core::stringw line;
	core::stringw word;
	core::stringw whitespace;
	s32 last_line_start = 0;
	s32 size = Text.size();
	s32 length = 0;
	s32 el_width = RelativeRect.getWidth() - m_scrollbar_width - 10;
	wchar_t c;

	for (s32 i = 0; i < size; ++i) {
		c = Text[i];
		bool line_break = false;

		if (c == L'\r') { // Mac or Windows breaks

			line_break = true;
			c = 0;
			if (Text[i + 1] == L'\n') { // Windows breaks
				Text.erase(i);
				--size;
				if (m_cursor_pos > i)
					--m_cursor_pos;
			}
		} else if (c == L'\n') { // Unix breaks
			line_break = true;
			c = 0;
		}

		// don't break if we're not a multi-line edit box
		if (!m_multiline)
			line_break = false;

		if (c == L' ' || c == 0 || i == (size - 1)) {
			// here comes the next whitespace, look if
			// we can break the last word to the next line
			// We also break whitespace, otherwise cursor would vanish beside the right border.
			s32 whitelgth = font->getDimension(whitespace.c_str()).Width;
			s32 worldlgth = font->getDimension(word.c_str()).Width;

			if (m_word_wrap && length + worldlgth + whitelgth > el_width && line.size() > 0) {
				// break to next line
				length = worldlgth;
				m_broken_text.push_back(line);
				m_broken_text_positions.push_back(last_line_start);
				last_line_start = i - (s32)word.size();
				line = word;
			} else {
				// add word to line
				line += whitespace;
				line += word;
				length += whitelgth + worldlgth;
			}

			word = L"";
			whitespace = L"";


			if (c)
				whitespace += c;

			// compute line break
			if (line_break) {
				line += whitespace;
				line += word;
				m_broken_text.push_back(line);
				m_broken_text_positions.push_back(last_line_start);
				last_line_start = i + 1;
				line = L"";
				word = L"";
				whitespace = L"";
				length = 0;
			}
		} else {
			// yippee this is a word..
			word += c;
		}
	}

	line += whitespace;
	line += word;
	m_broken_text.push_back(line);
	m_broken_text_positions.push_back(last_line_start);
}

// TODO: that function does interpret VAlign according to line-index (indexed
// line is placed on top-center-bottom) but HAlign according to line-width
// (pixels) and not by row.
// Intuitively I suppose HAlign handling is better as VScrollPos should handle
// the line-scrolling.
// But please no one change this without also rewriting (and this time
// testing!!!) autoscrolling (I noticed this when fixing the old autoscrolling).
void GUIEditBoxWithScrollBar::setTextRect(s32 line)
{
	if (line < 0)
		return;

	IGUIFont* font = getActiveFont();
	if (!font)
		return;

	core::dimension2du d;

	// get text dimension
	const u32 line_count = (m_word_wrap || m_multiline) ? m_broken_text.size() : 1;
	if (m_word_wrap || m_multiline) {
		d = font->getDimension(m_broken_text[line].c_str());
	} else {
		d = font->getDimension(Text.c_str());
		d.Height = AbsoluteRect.getHeight();
	}
	d.Height += font->getKerning(L'A').Y;

	// justification
	switch (m_halign) {
	case EGUIA_CENTER:
		// align to h center
		m_current_text_rect.UpperLeftCorner.X = (m_frame_rect.getWidth() / 2) - (d.Width / 2);
		m_current_text_rect.LowerRightCorner.X = (m_frame_rect.getWidth() / 2) + (d.Width / 2);
		break;
	case EGUIA_LOWERRIGHT:
		// align to right edge
		m_current_text_rect.UpperLeftCorner.X = m_frame_rect.getWidth() - d.Width;
		m_current_text_rect.LowerRightCorner.X = m_frame_rect.getWidth();
		break;
	default:
		// align to left edge
		m_current_text_rect.UpperLeftCorner.X = 0;
		m_current_text_rect.LowerRightCorner.X = d.Width;

	}

	switch (m_valign) {
	case EGUIA_CENTER:
		// align to v center
		m_current_text_rect.UpperLeftCorner.Y =
			(m_frame_rect.getHeight() / 2) - (line_count*d.Height) / 2 + d.Height*line;
		break;
	case EGUIA_LOWERRIGHT:
		// align to bottom edge
		m_current_text_rect.UpperLeftCorner.Y =
			m_frame_rect.getHeight() - line_count*d.Height + d.Height*line;
		break;
	default:
		// align to top edge
		m_current_text_rect.UpperLeftCorner.Y = d.Height*line;
		break;
	}

	m_current_text_rect.UpperLeftCorner.X -= m_hscroll_pos;
	m_current_text_rect.LowerRightCorner.X -= m_hscroll_pos;
	m_current_text_rect.UpperLeftCorner.Y -= m_vscroll_pos;
	m_current_text_rect.LowerRightCorner.Y = m_current_text_rect.UpperLeftCorner.Y + d.Height;

	m_current_text_rect += m_frame_rect.UpperLeftCorner;
}

// calculate autoscroll
void GUIEditBoxWithScrollBar::calculateScrollPos()
{
	if (!m_autoscroll)
		return;

	IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;
	IGUIFont* font = m_override_font ? m_override_font : skin->getFont();
	if (!font)
		return;

	s32 curs_line = getLineFromPos(m_cursor_pos);
	if (curs_line < 0)
		return;
	setTextRect(curs_line);
	const bool has_broken_text = m_multiline || m_word_wrap;

	// Check horizonal scrolling
	// NOTE: Calculations different to vertical scrolling because setTextRect interprets VAlign relative to line but HAlign not relative to row
	{
		// get cursor position
		IGUIFont* font = getActiveFont();
		if (!font)
			return;

		// get cursor area
		u32 cursor_width = font->getDimension(L"_").Width;
		core::stringw *txt_line = has_broken_text ? &m_broken_text[curs_line] : &Text;
		s32 cpos = has_broken_text ? m_cursor_pos - m_broken_text_positions[curs_line] : m_cursor_pos;	// column
		s32 cstart = font->getDimension(txt_line->subString(0, cpos).c_str()).Width;		// pixels from text-start
		s32 cend = cstart + cursor_width;
		s32 txt_width = font->getDimension(txt_line->c_str()).Width;

		if (txt_width < m_frame_rect.getWidth()) {
			// TODO: Needs a clean left and right gap removal depending on HAlign, similar to vertical scrolling tests for top/bottom.
			// This check just fixes the case where it was most noticeable (text smaller than clipping area).

			m_hscroll_pos = 0;
			setTextRect(curs_line);
		}

		if (m_current_text_rect.UpperLeftCorner.X + cstart < m_frame_rect.UpperLeftCorner.X) {
			// cursor to the left of the clipping area
			m_hscroll_pos -= m_frame_rect.UpperLeftCorner.X - (m_current_text_rect.UpperLeftCorner.X + cstart);
			setTextRect(curs_line);

			// TODO: should show more characters to the left when we're scrolling left
			//	and the cursor reaches the border.
		} else if (m_current_text_rect.UpperLeftCorner.X + cend > m_frame_rect.LowerRightCorner.X)	{
			// cursor to the right of the clipping area
			m_hscroll_pos += (m_current_text_rect.UpperLeftCorner.X + cend) - m_frame_rect.LowerRightCorner.X;
			setTextRect(curs_line);
		}
	}

	// calculate vertical scrolling
	if (has_broken_text) {
		u32 line_height = font->getDimension(L"A").Height + font->getKerning(L'A').Y;
		// only up to 1 line fits?
		if (line_height >= (u32)m_frame_rect.getHeight()) {
			m_vscroll_pos = 0;
			setTextRect(curs_line);
			s32 unscrolledPos = m_current_text_rect.UpperLeftCorner.Y;
			s32 pivot = m_frame_rect.UpperLeftCorner.Y;
			switch (m_valign) {
			case EGUIA_CENTER:
				pivot += m_frame_rect.getHeight() / 2;
				unscrolledPos += line_height / 2;
				break;
			case EGUIA_LOWERRIGHT:
				pivot += m_frame_rect.getHeight();
				unscrolledPos += line_height;
				break;
			default:
				break;
			}
			m_vscroll_pos = unscrolledPos - pivot;
			setTextRect(curs_line);
		} else {
			// First 2 checks are necessary when people delete lines
			setTextRect(0);
			if (m_current_text_rect.UpperLeftCorner.Y > m_frame_rect.UpperLeftCorner.Y && m_valign != EGUIA_LOWERRIGHT) {
				// first line is leaving a gap on top
				m_vscroll_pos = 0;
			} else if (m_valign != EGUIA_UPPERLEFT) {
				u32 lastLine = m_broken_text_positions.empty() ? 0 : m_broken_text_positions.size() - 1;
				setTextRect(lastLine);
				if (m_current_text_rect.LowerRightCorner.Y < m_frame_rect.LowerRightCorner.Y)
				{
					// last line is leaving a gap on bottom
					m_vscroll_pos -= m_frame_rect.LowerRightCorner.Y - m_current_text_rect.LowerRightCorner.Y;
				}
			}

			setTextRect(curs_line);
			if (m_current_text_rect.UpperLeftCorner.Y < m_frame_rect.UpperLeftCorner.Y) {
				// text above valid area
				m_vscroll_pos -= m_frame_rect.UpperLeftCorner.Y - m_current_text_rect.UpperLeftCorner.Y;
				setTextRect(curs_line);
			} else if (m_current_text_rect.LowerRightCorner.Y > m_frame_rect.LowerRightCorner.Y){
				// text below valid area
				m_vscroll_pos += m_current_text_rect.LowerRightCorner.Y - m_frame_rect.LowerRightCorner.Y;
				setTextRect(curs_line);
			}
		}
	}

	if (m_vscrollbar) {
		m_vscrollbar->setPos(m_vscroll_pos);
	}
}

void GUIEditBoxWithScrollBar::calculateFrameRect()
{
	m_frame_rect = AbsoluteRect;


	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();
	if (m_border && skin) {
		m_frame_rect.UpperLeftCorner.X += skin->getSize(EGDS_TEXT_DISTANCE_X) + 1;
		m_frame_rect.UpperLeftCorner.Y += skin->getSize(EGDS_TEXT_DISTANCE_Y) + 1;
		m_frame_rect.LowerRightCorner.X -= skin->getSize(EGDS_TEXT_DISTANCE_X) + 1;
		m_frame_rect.LowerRightCorner.Y -= skin->getSize(EGDS_TEXT_DISTANCE_Y) + 1;
	}

	updateVScrollBar();
}

//! create a vertical scroll bar
void GUIEditBoxWithScrollBar::createVScrollBar()
{
	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();

	s32 fontHeight = 1;

	if (m_override_font) {
		fontHeight = m_override_font->getDimension(L"Ay").Height;
	} else {
		IGUIFont *font;
		if (skin && (font = skin->getFont())) {
			fontHeight = font->getDimension(L"Ay").Height;
		}
	}

	m_scrollbar_width = skin ? skin->getSize(gui::EGDS_SCROLLBAR_SIZE) : 16;

	core::rect<s32> scrollbarrect = m_frame_rect;
	scrollbarrect.UpperLeftCorner.X += m_frame_rect.getWidth() - m_scrollbar_width;
	m_vscrollbar = new GUIScrollBar(Environment, getParent(), -1,
			scrollbarrect, false, true, m_tsrc);

	m_vscrollbar->setVisible(false);
	m_vscrollbar->setSmallStep(3 * fontHeight);
	m_vscrollbar->setLargeStep(10 * fontHeight);
}



//! Change the background color
void GUIEditBoxWithScrollBar::setBackgroundColor(const video::SColor &bg_color)
{
	m_bg_color = bg_color;
	m_bg_color_used = true;
}

bool GUIEditBoxWithScrollBar::isDrawBackgroundEnabled() const { return false; }
bool GUIEditBoxWithScrollBar::isDrawBorderEnabled() const { return false; }
void GUIEditBoxWithScrollBar::setCursorChar(const wchar_t cursorChar) { }
wchar_t GUIEditBoxWithScrollBar::getCursorChar() const { return '|'; }
void GUIEditBoxWithScrollBar::setCursorBlinkTime(u32 timeMs) { }
u32 GUIEditBoxWithScrollBar::getCursorBlinkTime() const { return 500; }
