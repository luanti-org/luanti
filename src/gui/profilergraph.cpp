// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "porting.h"
#include "profilergraph.h"
#include "IVideoDriver.h"
#include "util/string.h"

void ProfilerGraph::put(const Profiler::GraphValues &values)
{
	m_log.emplace_back(values);

	while (m_log.size() > m_log_max_size)
		m_log.erase(m_log.begin());
}

void ProfilerGraph::draw(s32 x_left, s32 y_bottom, video::IVideoDriver *driver,
		gui::IGUIFont *font) const
{
	// Do *not* use UNORDERED_MAP here as the order needs
	// to be the same for each call to prevent flickering
	std::map<std::string, Meta> m_meta;

	for (const Piece &piece : m_log) {
		for (const auto &i : piece.values) {
			const std::string &id = i.first;
			const float &value = i.second;
			auto j = m_meta.find(id);

			if (j == m_meta.end()) {
				m_meta[id] = Meta(value);
				continue;
			}

			if (value < j->second.min)
				j->second.min = value;

			if (value > j->second.max)
				j->second.max = value;
		}
	}

	// Assign colors
	static const video::SColor usable_colors[] = {video::SColor(255, 255, 100, 100),
			video::SColor(255, 90, 225, 90),
			video::SColor(255, 100, 100, 255),
			video::SColor(255, 255, 150, 50),
			video::SColor(255, 220, 220, 100)};
	static const u32 usable_colors_count =
			sizeof(usable_colors) / sizeof(*usable_colors);
	u32 next_color_i = 0;

	for (auto &i : m_meta) {
		Meta &meta = i.second;
		video::SColor color(255, 200, 200, 200);

		if (next_color_i < usable_colors_count)
			color = usable_colors[next_color_i++];

		meta.color = color;
	}

	s32 graphh = 50;
	s32 textx = x_left + m_log_max_size + 15;
	s32 textx2 = textx + 200 - 15;
	s32 meta_i = 0;

	for (const auto &p : m_meta) {
		const std::string &id = p.first;
		const Meta &meta = p.second;
		s32 x = x_left;
		s32 y = y_bottom - meta_i * 50;
		float show_min = meta.min;
		float show_max = meta.max;

		if (show_min >= -0.0001 && show_max >= -0.0001) {
			if (show_min <= show_max * 0.5)
				show_min = 0;
		}

		const s32 texth = 15;
		char buf[20];
		if (floorf(show_max) == show_max)
			porting::mt_snprintf(buf, sizeof(buf), "%.5g", show_max);
		else
			porting::mt_snprintf(buf, sizeof(buf), "%.3g", show_max);
		font->draw(utf8_to_wide(buf).c_str(),
				core::rect<s32>(textx, y - graphh, textx2,
						y - graphh + texth),
				meta.color);

		if (floorf(show_min) == show_min)
			porting::mt_snprintf(buf, sizeof(buf), "%.5g", show_min);
		else
			porting::mt_snprintf(buf, sizeof(buf), "%.3g", show_min);
		font->draw(utf8_to_wide(buf).c_str(),
				core::rect<s32>(textx, y - texth, textx2, y), meta.color);

		font->draw(utf8_to_wide(id).c_str(),
				core::rect<s32>(textx, y - graphh / 2 - texth / 2, textx2,
						y - graphh / 2 + texth / 2),
				meta.color);

		s32 graph1y = y;
		s32 graph1h = graphh;
		bool relativegraph = (show_min != 0 && show_min != show_max);
		float lastscaledvalue = 0.0;
		bool lastscaledvalue_exists = false;

		for (const Piece &piece : m_log) {
			float value = 0;
			bool value_exists = false;
			Profiler::GraphValues::const_iterator k = piece.values.find(id);

			if (k != piece.values.end()) {
				value = k->second;
				value_exists = true;
			}

			if (!value_exists) {
				x++;
				lastscaledvalue_exists = false;
				continue;
			}

			float scaledvalue = 1.0;

			if (show_max != show_min)
				scaledvalue = (value - show_min) / (show_max - show_min);

			if (scaledvalue == 1.0 && value == 0) {
				x++;
				lastscaledvalue_exists = false;
				continue;
			}

			if (relativegraph) {
				if (lastscaledvalue_exists) {
					s32 ivalue1 = lastscaledvalue * graph1h;
					s32 ivalue2 = scaledvalue * graph1h;
					driver->draw2DLine(
							v2s32(x - 1, graph1y - ivalue1),
							v2s32(x, graph1y - ivalue2),
							meta.color);
				}

				lastscaledvalue = scaledvalue;
				lastscaledvalue_exists = true;
			} else {
				s32 ivalue = scaledvalue * graph1h;
				driver->draw2DLine(v2s32(x, graph1y),
						v2s32(x, graph1y - ivalue), meta.color);
			}

			x++;
		}

		meta_i++;
	}
}
