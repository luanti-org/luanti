/*
Part of Minetest
Copyright (C) 2013 RealBadAngel, Maciej Kasatkin <mk@realbadangel.pl>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#pragma once

#include <IGUIElement.h>
#include <IGUIEnvironment.h>
#include "irr_v2d.h"


class ISimpleTextureSource;


class GUIBackgroundImage : public gui::IGUIElement
{
public:
	GUIBackgroundImage(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
		const core::rect<s32> &rectangle, const std::string &name,
		const core::rect<s32> &middle, ISimpleTextureSource *tsrc, bool autoclip,
		v2s32 autoclip_offset);

	virtual void draw() override;

private:
	std::string m_name;
	core::rect<s32> m_middle;
	ISimpleTextureSource *m_tsrc;
	bool m_autoclip;
	v2s32 m_autoclip_offset;
};
