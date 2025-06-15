// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Krock/SmallJoker <mk939@ymail.com>

#pragma once

#include "StyleSpec.h" // StyleSpecMap
#include "../../irr/src/CGUISkin.h"

class GUISkin : public gui::CGUISkin {
public:
	GUISkin(video::IVideoDriver *driver);
	virtual ~GUISkin();

	void setTextureSource(ISimpleTextureSource *src) { m_texture_source = src; }

	virtual void drawColored3DButtonPaneStandard(gui::IGUIElement *element,
			const core::rect<s32> &rect,
			const core::rect<s32> *clip = 0,
			const video::SColor *colors = 0) override;

	virtual void drawColored3DButtonPanePressed(gui::IGUIElement *element,
			const core::rect<s32> &rect,
			const core::rect<s32> *clip = 0,
			const video::SColor *colors = 0) override;

	StyleSpecMap &getThemeRef() { return m_theme; }

private:
	bool tryDrawPane(const char *type, StyleSpec::State state,
			const core::rect<s32> &rect,
			const core::rect<s32> *clip = 0);

	ISimpleTextureSource *m_texture_source = nullptr;
	StyleSpecMap m_theme;
};

