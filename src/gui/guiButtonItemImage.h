// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "guiButton.h"
#include "IGUIButton.h"


class Client;
class GUIItemImage;

class GUIButtonItemImage : public GUIButton
{
public:
	//! constructor
	GUIButtonItemImage(gui::IGUIEnvironment *environment, gui::IGUIElement *parent,
			s32 id, core::rect<s32> rectangle, ISimpleTextureSource *tsrc,
			const std::string &item, Client *client, bool noclip = false);

	//! Do not drop returned handle
	static GUIButtonItemImage *addButton(gui::IGUIEnvironment *environment,
			const core::rect<s32> &rectangle, ISimpleTextureSource *tsrc,
			IGUIElement *parent, s32 id, const wchar_t *text,
			const std::string &item, Client *client);

private:
	Client *m_client;
	GUIItemImage *m_image;
};
