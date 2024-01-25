// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#pragma once

#include "ui/helpers.h"
#include "ui/style.h"
#include "util/basic_macros.h"

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace ui
{
	class Elem;
	class Window;

	class Box
	{
	public:
		using State = u32;

		// These states are organized in order of precedence. States with a
		// larger value will override the styles of states with a lower value.
		static constexpr State STATE_NONE = 0;

		static constexpr State STATE_FOCUSED  = 1 << 0;
		static constexpr State STATE_SELECTED = 1 << 1;
		static constexpr State STATE_HOVERED  = 1 << 2;
		static constexpr State STATE_PRESSED  = 1 << 3;
		static constexpr State STATE_DISABLED = 1 << 4;

		static constexpr State NUM_STATES = 1 << 5;

	private:
		// Indicates that there is no style string for this state combination.
		static constexpr u32 NO_STYLE = -1;

		Elem &m_elem;

		std::vector<Box *> m_content;

		Style m_style;
		std::array<u32, NUM_STATES> m_style_refs;

		// Cached information about the layout of the box, which is cleared in
		// restyle() and recomputed in resize() and relayout().
		SizeF m_min_layout;
		SizeF m_min_content;

		RectF m_display_rect;
		RectF m_icon_rect;
		RectF m_content_rect;

		RectF m_clip_rect;

	public:
		Box(Elem &elem) :
			m_elem(elem)
		{
			reset();
		}

		DISABLE_CLASS_COPY(Box)

		Elem &getElem() { return m_elem; }
		const Elem &getElem() const { return m_elem; }

		Window &getWindow();
		const Window &getWindow() const;

		const std::vector<Box *> &getContent() const { return m_content; }
		void setContent(std::vector<Box *> content) { m_content = std::move(content); }

		void reset();
		void read(std::istream &is);

		void restyle();
		void resize();
		void relayout(RectF layout_rect, RectF layout_clip);

		void draw();

	private:
		static RectF getLayerSource(const Layer &layer);
		static SizeF getLayerSize(const Layer &layer);

		DispF getMiddleEdges();

		void resizeBox();
		void relayoutBox(RectF layout_rect, RectF layout_clip);

		void resizePlace();
		void relayoutPlace();

		void drawBox();
		void drawIcon();
	};
}
