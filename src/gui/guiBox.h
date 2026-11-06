// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <array>
#include <IGUIElement.h>
#include <IGUIEnvironment.h>


class GUIBox : public gui::IGUIElement
{
public:
	GUIBox(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
		const core::rect<s32> &rectangle,
		const std::array<video::SColor, 4> &colors,
		const std::array<video::SColor, 4> &bordercolors,
		const std::array<s32, 4> &borderwidths,
		s32 borderradius = 0, s32 animation_start_y = -1,
		u32 animation_duration = 0);

	virtual void draw() override;

private:
	std::array<video::SColor, 4> m_colors;
	std::array<video::SColor, 4> m_bordercolors;
	std::array<s32, 4> m_borderwidths;
	s32 m_borderradius = 0;
	s32 m_animation_start_y = -1;
	u32 m_animation_start_time = 0;
	u32 m_animation_duration = 0;
};
