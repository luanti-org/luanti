// Copyright (C) 2002-2012 Nikolaus Gebhardt
// Copyright (C) 2016 Nathanaëlle Courant:
//   Modified the functions to use EnrichedText instead of string.
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "static_text.h"

#include <IGUIFont.h>
#include <IVideoDriver.h>
#include <rect.h>
#include <SColor.h>

#include "CGUITTFont.h"
#include "util/string.h"


namespace gui
{
//! constructor
StaticText::StaticText(const EnrichedString &text, bool border,
			IGUIEnvironment* environment, IGUIElement* parent,
			s32 id, const core::rect<s32>& rectangle,
			bool background)
: IGUIStaticText(environment, parent, id, rectangle),
	HAlign(EGUIA_UPPERLEFT), VAlign(EGUIA_UPPERLEFT),
	Border(border), WordWrap(false), Background(background),
	RestrainTextInside(true), RightToLeft(false),
	OverrideFont(0), LastBreakFont(0)
{
	setText(text);
}


//! destructor
StaticText::~StaticText()
{
	if (OverrideFont)
		OverrideFont->drop();
}

//! draws the element and its children
void StaticText::draw()
{
	if (!IsVisible)
		return;

	IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;
	video::IVideoDriver* driver = Environment->getVideoDriver();

	core::rect<s32> frameRect(AbsoluteRect);

	// draw background

	if (Background)
		driver->draw2DRectangle(getBackgroundColor(), frameRect, &AbsoluteClippingRect);

	// draw the border

	if (Border)
	{
		skin->draw3DSunkenPane(this, 0, true, false, frameRect, &AbsoluteClippingRect);
		frameRect.UpperLeftCorner.X += skin->getSize(EGDS_TEXT_DISTANCE_X);
	}

	// draw the text
	IGUIFont *font = getActiveFont();
	if (font && BrokenText.size()) {
		if (font != LastBreakFont)
			updateText();

		core::rect<s32> r = frameRect;
		s32 height_line = font->getDimension(L"A").Height + font->getKerning(L'A').Y;
		s32 height_total = height_line * BrokenText.size();
		if (VAlign == EGUIA_CENTER && WordWrap)
		{
			r.UpperLeftCorner.Y = r.getCenter().Y - (height_total / 2);
		}
		else if (VAlign == EGUIA_LOWERRIGHT)
		{
			r.UpperLeftCorner.Y = r.LowerRightCorner.Y - height_total;
		}
		if (HAlign == EGUIA_LOWERRIGHT)
		{
			r.UpperLeftCorner.X = r.LowerRightCorner.X -
				getTextWidth();
		}

		for (const EnrichedString &str : BrokenText) {
			if (HAlign == EGUIA_LOWERRIGHT)
			{
				r.UpperLeftCorner.X = frameRect.LowerRightCorner.X -
					font->getDimension(str.c_str()).Width;
			}

			if (font->getType() == gui::EGFT_CUSTOM) {
				CGUITTFont *tmp = static_cast<CGUITTFont*>(font);
				tmp->draw(str,
					r, HAlign == EGUIA_CENTER, VAlign == EGUIA_CENTER,
					(RestrainTextInside ? &AbsoluteClippingRect : NULL));
			} else
			{
				// Draw non-colored text
				font->draw(str.c_str(),
					r, str.getDefaultColor(), // TODO: Implement colorization
					HAlign == EGUIA_CENTER, VAlign == EGUIA_CENTER,
					(RestrainTextInside ? &AbsoluteClippingRect : NULL));
			}


			r.LowerRightCorner.Y += height_line;
			r.UpperLeftCorner.Y += height_line;
		}
	}

	IGUIElement::draw();
}


//! Sets another skin independent font.
void StaticText::setOverrideFont(IGUIFont* font)
{
	if (OverrideFont == font)
		return;

	if (OverrideFont)
		OverrideFont->drop();

	OverrideFont = font;

	if (OverrideFont)
		OverrideFont->grab();

	updateText();
}

//! Gets the override font (if any)
IGUIFont * StaticText::getOverrideFont() const
{
	return OverrideFont;
}

//! Get the font which is used right now for drawing
IGUIFont* StaticText::getActiveFont() const
{
	if ( OverrideFont )
		return OverrideFont;
	IGUISkin* skin = Environment->getSkin();
	if (skin)
		return skin->getFont();
	return 0;
}

//! Sets another color for the text.
void StaticText::setOverrideColor(video::SColor color)
{
	ColoredText.setDefaultColor(color);
	updateText();
}


//! Sets another color for the text.
void StaticText::setBackgroundColor(video::SColor color)
{
	ColoredText.setBackground(color);
	Background = true;
}


//! Sets whether to draw the background
void StaticText::setDrawBackground(bool draw)
{
	Background = draw;
}


//! Gets the background color
video::SColor StaticText::getBackgroundColor() const
{
	IGUISkin *skin = Environment->getSkin();

	return (ColoredText.hasBackground() || !skin) ?
		ColoredText.getBackground() : skin->getColor(gui::EGDC_3D_FACE);
}


//! Checks if background drawing is enabled
bool StaticText::isDrawBackgroundEnabled() const
{
	return Background;
}


//! Sets whether to draw the border
void StaticText::setDrawBorder(bool draw)
{
	Border = draw;
}


//! Checks if border drawing is enabled
bool StaticText::isDrawBorderEnabled() const
{
	return Border;
}


void StaticText::setTextRestrainedInside(bool restrainTextInside)
{
	RestrainTextInside = restrainTextInside;
}


bool StaticText::isTextRestrainedInside() const
{
	return RestrainTextInside;
}


void StaticText::setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical)
{
	HAlign = horizontal;
	VAlign = vertical;
}


video::SColor StaticText::getOverrideColor() const
{
	return ColoredText.getDefaultColor();
}

video::SColor StaticText::getActiveColor() const
{
	return getOverrideColor();
}

//! Sets if the static text should use the overide color or the
//! color in the gui skin.
void StaticText::enableOverrideColor(bool enable)
{
	// TODO
}


bool StaticText::isOverrideColorEnabled() const
{
	return true;
}


//! Enables or disables word wrap for using the static text as
//! multiline text control.
void StaticText::setWordWrap(bool enable)
{
	WordWrap = enable;
	updateText();
}


bool StaticText::isWordWrapEnabled() const
{
	return WordWrap;
}


void StaticText::setRightToLeft(bool rtl)
{
	if (RightToLeft != rtl)
	{
		RightToLeft = rtl;
		updateText();
	}
}


bool StaticText::isRightToLeft() const
{
	return RightToLeft;
}


//! Breaks the single text line.
// Updates the font colors
void StaticText::updateText()
{
	const EnrichedString &cText = ColoredText;
	BrokenText.clear();

	if (cText.hasBackground())
		setBackgroundColor(cText.getBackground());
	else
		setDrawBackground(false);

	if (!WordWrap) {
		BrokenText.push_back(cText);
		return;
	}

	// Update word wrap

	IGUISkin* skin = Environment->getSkin();
	IGUIFont* font = getActiveFont();
	if (!font)
		return;

	LastBreakFont = font;

	EnrichedString line;
	EnrichedString word;
	EnrichedString whitespace;
	s32 size = cText.size();
	s32 length = 0;
	s32 elWidth = RelativeRect.getWidth();
	if (Border)
		elWidth -= 2*skin->getSize(EGDS_TEXT_DISTANCE_X);
	wchar_t c;

	//std::vector<video::SColor> colors;

	// We have to deal with right-to-left and left-to-right differently
	// However, most parts of the following code is the same, it's just
	// some order and boundaries which change.
	if (!RightToLeft)
	{
		// regular (left-to-right)
		for (s32 i=0; i<size; ++i)
		{
			c = cText.getString()[i];
			bool lineBreak = false;

			if (c == L'\r') // Mac or Windows breaks
			{
				lineBreak = true;
				//if (Text[i+1] == L'\n') // Windows breaks
				//{
				//	Text.erase(i+1);
				//	--size;
				//}
				c = '\0';
			}
			else if (c == L'\n') // Unix breaks
			{
				lineBreak = true;
				c = '\0';
			}

			bool isWhitespace = (c == L' ' || c == 0);
			if ( !isWhitespace )
			{
				// part of a word
				//word += c;
				word.addChar(cText, i);
			}

			if ( isWhitespace || i == (size-1))
			{
				if (word.size())
				{
					// here comes the next whitespace, look if
					// we must break the last word to the next line.
					const s32 whitelgth = font->getDimension(whitespace.c_str()).Width;
					//const std::wstring sanitized = removeEscapes(word.c_str());
					const s32 wordlgth = font->getDimension(word.c_str()).Width;

					if (wordlgth > elWidth)
					{
						// This word is too long to fit in the available space, look for
						// the Unicode Soft HYphen (SHY / 00AD) character for a place to
						// break the word at
						int where = core::stringw(word.c_str()).findFirst( wchar_t(0x00AD) );
						if (where != -1)
						{
							EnrichedString first = word.substr(0, where);
							EnrichedString second = word.substr(where, word.size() - where);
							first.addCharNoColor(L'-');
							BrokenText.push_back(line + first);
							const s32 secondLength = font->getDimension(second.c_str()).Width;

							length = secondLength;
							line = second;
						}
						else
						{
							// No soft hyphen found, so there's nothing more we can do
							// break to next line
							if (length)
								BrokenText.push_back(line);
							length = wordlgth;
							line = word;
						}
					}
					else if (length && (length + wordlgth + whitelgth > elWidth))
					{
						// break to next line
						BrokenText.push_back(line);
						length = wordlgth;
						line = word;
					}
					else
					{
						// add word to line
						line += whitespace;
						line += word;
						length += whitelgth + wordlgth;
					}

					word.clear();
					whitespace.clear();
				}

				if ( isWhitespace && c != 0)
				{
					whitespace.addChar(cText, i);
				}

				// compute line break
				if (lineBreak)
				{
					line += whitespace;
					line += word;
					BrokenText.push_back(line);
					line.clear();
					word.clear();
					whitespace.clear();
					length = 0;
				}
			}
		}

		line += whitespace;
		line += word;
		BrokenText.push_back(line);
	}
	else
	{
		// right-to-left
		for (s32 i=size; i>=0; --i)
		{
			c = cText.getString()[i];
			bool lineBreak = false;

			if (c == L'\r') // Mac or Windows breaks
			{
				lineBreak = true;
				//if ((i>0) && Text[i-1] == L'\n') // Windows breaks
				//{
				//	Text.erase(i-1);
				//	--size;
				//}
				c = '\0';
			}
			else if (c == L'\n') // Unix breaks
			{
				lineBreak = true;
				c = '\0';
			}

			if (c==L' ' || c==0 || i==0)
			{
				if (word.size())
				{
					// here comes the next whitespace, look if
					// we must break the last word to the next line.
					const s32 whitelgth = font->getDimension(whitespace.c_str()).Width;
					const s32 wordlgth = font->getDimension(word.c_str()).Width;

					if (length && (length + wordlgth + whitelgth > elWidth))
					{
						// break to next line
						BrokenText.push_back(line);
						length = wordlgth;
						line = word;
					}
					else
					{
						// add word to line
						line = whitespace + line;
						line = word + line;
						length += whitelgth + wordlgth;
					}

					word.clear();
					whitespace.clear();
				}

				if (c != 0)
				//	whitespace = core::stringw(&c, 1) + whitespace;
				whitespace = cText.substr(i, 1) + whitespace;

				// compute line break
				if (lineBreak)
				{
					line = whitespace + line;
					line = word + line;
					BrokenText.push_back(line);
					line.clear();
					word.clear();
					whitespace.clear();
					length = 0;
				}
			}
			else
			{
				// yippee this is a word..
				//word = core::stringw(&c, 1) + word;
				word = cText.substr(i, 1) + word;
			}
		}

		line = whitespace + line;
		line = word + line;
		BrokenText.push_back(line);
	}
}


//! Sets the new caption of this element.
void StaticText::setText(const wchar_t* text)
{
	setText(EnrichedString(text, getOverrideColor()));
}

void StaticText::setText(const EnrichedString &text)
{
	ColoredText = text;
	IGUIElement::setText(ColoredText.c_str());
	updateText();
}

void StaticText::updateAbsolutePosition()
{
	IGUIElement::updateAbsolutePosition();
	updateText();
}


//! Returns the height of the text in pixels when it is drawn.
s32 StaticText::getTextHeight() const
{
	IGUIFont* font = getActiveFont();
	if (!font)
		return 0;

	if (WordWrap) {
		s32 height = font->getDimension(L"A").Height + font->getKerning(L'A').Y;
		return height * BrokenText.size();
	}
	// There may be intentional new lines without WordWrap
	return font->getDimension(BrokenText[0].c_str()).Height;
}


s32 StaticText::getTextWidth() const
{
	IGUIFont *font = getActiveFont();
	if (!font)
		return 0;

	s32 widest = 0;

	for (const EnrichedString &line : BrokenText) {
		s32 width = font->getDimension(line.c_str()).Width;

		if (width > widest)
			widest = width;
	}

	return widest;
}


} // end namespace gui
