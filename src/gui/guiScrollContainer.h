// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2020 DS

#pragma once

#include "guiScrollBar.h"

class GUIScrollContainer : public gui::IGUIElement
{
public:
	GUIScrollContainer(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
			const core::rect<s32> &rectangle, const std::string &orientation,
			f32 scrollfactor);

	virtual bool OnEvent(const SEvent &event) override;

	virtual void draw() override;

	inline void setContentPadding(std::optional<s32> padding)
	{
		m_content_padding_px = padding;
	}

	inline void onScrollEvent(gui::IGUIElement *caller)
	{
		if (caller == m_scrollbar)
			updateScrolling();
	}

	void setScrollBar(GUIScrollBar *scrollbar);
	void updateScrolling();

private:
	enum OrientationEnum
	{
		VERTICAL,
		HORIZONTAL,
		UNDEFINED
	};

	GUIScrollBar *m_scrollbar;
	OrientationEnum m_orientation;
	f32 m_scrollfactor; //< scrollbar pos * scrollfactor = scroll offset in pixels
	std::optional<s32> m_content_padding_px; //< in pixels

};
