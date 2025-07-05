// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <SColor.h>
#include <deque>
#include <utility>
#include <IGUIFont.h>
#include "profiler.h"

namespace video {
	class IVideoDriver;
}

/* Profiler display */
class ProfilerGraph
{
private:
	struct Piece
	{
		Piece(Profiler::GraphValues v) : values(std::move(v)) {}
		Profiler::GraphValues values;
	};
	struct Meta
	{
		float min;
		float max;
		video::SColor color;
		Meta(float initial = 0,
				video::SColor color = video::SColor(255, 255, 255, 255)) :
				min(initial),
				max(initial), color(color)
		{
		}
	};
	std::deque<Piece> m_log;

public:
	u32 m_log_max_size = 200;

	ProfilerGraph() = default;

	void put(const Profiler::GraphValues &values);

	void draw(s32 x_left, s32 y_bottom, video::IVideoDriver *driver,
			gui::IGUIFont *font) const;
};
