// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <IGUIElement.h>
#include <IGUIEnvironment.h>


class Client;

class GUIItemImage : public gui::IGUIElement
{
public:
	GUIItemImage(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
		const core::rect<s32> &rectangle, const std::string &item_name,
		gui::IGUIFont *font, Client *client);

	virtual void draw() override;

	virtual void setText(const wchar_t *text) override
	{
		m_label = text;
	}

private:
	std::string m_item_name;
	gui::IGUIFont *m_font;
	Client *m_client;
	core::stringw m_label;
};
