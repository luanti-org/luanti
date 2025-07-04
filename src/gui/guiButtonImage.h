// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "guiButton.h"
#include "IGUIButton.h"
#include "guiAnimatedImage.h"
#include "irr_ptr.h"


class GUIButtonImage : public GUIButton
{
public:
	//! constructor
	GUIButtonImage(gui::IGUIEnvironment *environment, gui::IGUIElement *parent,
			s32 id, core::rect<s32> rectangle, ISimpleTextureSource *tsrc,
			bool noclip = false);

	void setForegroundImage(irr_ptr<video::ITexture> image = nullptr,
			const core::rect<s32> &middle = core::rect<s32>());

	//! Set element properties from a StyleSpec
	virtual void setFromStyle(const StyleSpec &style) override;

	//! Do not drop returned handle
	static GUIButtonImage *addButton(gui::IGUIEnvironment *environment,
			const core::rect<s32> &rectangle, ISimpleTextureSource *tsrc,
			IGUIElement *parent, s32 id, const wchar_t *text,
			const wchar_t *tooltiptext = L"");

private:
	irr_ptr<video::ITexture> m_foreground_image;
	irr_ptr<GUIAnimatedImage> m_image;
};
