// Copyright (C) 2002-2012 Nikolaus Gebhardt
// Copyright (C) 2016 Nathanaëlle Courant
//   Modified this class to work with EnrichedStrings too
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIStaticText.h"
#include "irrArray.h"

#include "log.h"

#include <vector>

#include "util/enriched_string.h"
#include "config.h"
#include <IGUIEnvironment.h>


namespace gui
{

	const EGUI_ELEMENT_TYPE EGUIET_ENRICHED_STATIC_TEXT = (EGUI_ELEMENT_TYPE)(0x1000);

	class StaticText : public IGUIStaticText
	{
	public:

		// StaticText is translated by EnrichedString.
		// No need to use translate_string()
		StaticText(const EnrichedString &text, bool border, IGUIEnvironment* environment,
			IGUIElement* parent, s32 id, const core::rect<s32>& rectangle,
			bool background = false);

		//! destructor
		virtual ~StaticText();

		static gui::IGUIStaticText *add(
			gui::IGUIEnvironment *guienv,
			const EnrichedString &text,
			const core::rect< s32 > &rectangle,
			bool border = false,
			bool wordWrap = true,
			gui::IGUIElement *parent = NULL,
			s32 id = -1,
			bool fillBackground = false)
		{
			parent = parent ? parent : guienv->getRootGUIElement();
			gui::IGUIStaticText *result = new gui::StaticText(
				text, border, guienv, parent,
				id, rectangle, fillBackground);

			result->setWordWrap(wordWrap);
			result->drop();
			return result;
		}

		static gui::IGUIStaticText *add(
			gui::IGUIEnvironment *guienv,
			std::wstring_view text,
			const core::rect< s32 > &rectangle,
			bool border = false,
			bool wordWrap = true,
			gui::IGUIElement *parent = NULL,
			s32 id = -1,
			bool fillBackground = false)
		{
			return add(guienv, EnrichedString(text), rectangle, border, wordWrap, parent,
				id, fillBackground);
		}

		//! draws the element and its children
		virtual void draw();

		//! Sets another skin independent font.
		virtual void setOverrideFont(IGUIFont* font=0);

		//! Gets the override font (if any)
		virtual IGUIFont* getOverrideFont() const;

		//! Get the font which is used right now for drawing
		virtual IGUIFont* getActiveFont() const;

		//! Sets another color for the text.
		virtual void setOverrideColor(video::SColor color);

		//! Sets another color for the background.
		virtual void setBackgroundColor(video::SColor color);

		//! Sets whether to draw the background
		virtual void setDrawBackground(bool draw);

		//! Gets the background color
		virtual video::SColor getBackgroundColor() const;

		//! Checks if background drawing is enabled
		virtual bool isDrawBackgroundEnabled() const;

		//! Sets whether to draw the border
		virtual void setDrawBorder(bool draw);

		//! Checks if border drawing is enabled
		virtual bool isDrawBorderEnabled() const;

		//! Sets alignment mode for text
		virtual void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical);

		//! Gets the override color
		virtual video::SColor getOverrideColor() const;

		//! Gets the currently used text color
		virtual video::SColor getActiveColor() const;

		//! Sets if the static text should use the overide color or the
		//! color in the gui skin.
		virtual void enableOverrideColor(bool enable);

		//! Checks if an override color is enabled
		virtual bool isOverrideColorEnabled() const;

		//! Set whether the text in this label should be clipped if it goes outside bounds
		virtual void setTextRestrainedInside(bool restrainedInside);

		//! Checks if the text in this label should be clipped if it goes outside bounds
		virtual bool isTextRestrainedInside() const;

		//! Enables or disables word wrap for using the static text as
		//! multiline text control.
		virtual void setWordWrap(bool enable);

		//! Checks if word wrap is enabled
		virtual bool isWordWrapEnabled() const;

		//! Sets the new caption of this element.
		virtual void setText(const wchar_t* text);

		//! Returns the height of the text in pixels when it is drawn.
		virtual s32 getTextHeight() const;

		//! Returns the width of the current text, in the current font
		virtual s32 getTextWidth() const;

		//! Updates the absolute position, splits text if word wrap is enabled
		virtual void updateAbsolutePosition();

		//! Set whether the string should be interpreted as right-to-left (RTL) text
		/** \note This component does not implement the Unicode bidi standard, the
		text of the component should be already RTL if you call this. The
		main difference when RTL is enabled is that the linebreaks for multiline
		elements are performed starting from the end.
		*/
		virtual void setRightToLeft(bool rtl);

		//! Checks if the text should be interpreted as right-to-left text
		virtual bool isRightToLeft() const;

		virtual bool hasType(EGUI_ELEMENT_TYPE t) const {
			return (t == EGUIET_ENRICHED_STATIC_TEXT) || (t == EGUIET_STATIC_TEXT);
		};

		virtual bool hasType(EGUI_ELEMENT_TYPE t) {
			return (t == EGUIET_ENRICHED_STATIC_TEXT) || (t == EGUIET_STATIC_TEXT);
		};

		void setText(const EnrichedString &text);

	private:

		//! Breaks the single text line.
		void updateText();

		EGUI_ALIGNMENT HAlign, VAlign;
		bool Border;
		bool WordWrap;
		bool Background;
		bool RestrainTextInside;
		bool RightToLeft;

		gui::IGUIFont* OverrideFont;
		gui::IGUIFont* LastBreakFont; // stored because: if skin changes, line break must be recalculated.

		EnrichedString ColoredText;
		std::vector<EnrichedString> BrokenText;
	};


} // end namespace gui

inline void setStaticText(gui::IGUIStaticText *static_text, const EnrichedString &text)
{
	// dynamic_cast not possible due to some distributions shipped
	// without rtti support in irrlicht
	if (static_text->hasType(gui::EGUIET_ENRICHED_STATIC_TEXT)) {
		gui::StaticText* stext = static_cast<gui::StaticText*>(static_text);
		stext->setText(text);
	} else {
		static_text->setText(text.c_str());
	}
}

inline void setStaticText(gui::IGUIStaticText *static_text, std::wstring_view text)
{
	setStaticText(static_text, EnrichedString(text, static_text->getOverrideColor()));
}
