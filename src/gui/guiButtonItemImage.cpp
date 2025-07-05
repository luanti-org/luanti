// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiButtonItemImage.h"

#include "client/client.h"
#include "guiItemImage.h"
#include "IGUIEnvironment.h"
#include "itemdef.h"


using namespace gui;

GUIButtonItemImage::GUIButtonItemImage(gui::IGUIEnvironment *environment,
		gui::IGUIElement *parent, s32 id, core::rect<s32> rectangle,
		ISimpleTextureSource *tsrc, const std::string &item, Client *client,
		bool noclip)
		: GUIButton (environment, parent, id, rectangle, tsrc, noclip)
{
	m_image = new GUIItemImage(environment, this, id,
			core::rect<s32>(0,0,rectangle.getWidth(),rectangle.getHeight()),
			item, getActiveFont(), client);
	sendToBack(m_image);

	m_client = client;
}

GUIButtonItemImage *GUIButtonItemImage::addButton(IGUIEnvironment *environment,
		const core::rect<s32> &rectangle, ISimpleTextureSource *tsrc,
		IGUIElement *parent, s32 id, const wchar_t *text, const std::string &item,
		Client *client)
{
	GUIButtonItemImage *button = new GUIButtonItemImage(environment,
			parent ? parent : environment->getRootGUIElement(),
			id, rectangle, tsrc, item, client);

	if (text)
		button->setText(text);

	button->drop();
	return button;
}
