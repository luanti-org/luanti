// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#pragma once

#include "ui/elem.h"
#include "ui/helpers.h"
#include "util/basic_macros.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

union SDL_Event;

namespace ui
{
	class Root;

	SizeI getTextureSize(video::ITexture *texture);

	// Serialized enum; do not change order of entries.
	enum class WindowType : u8
	{
		FILTER,
		MASK,
		HUD,
		CHAT,
		GUI,

		MAX = GUI,
	};

	WindowType toWindowType(u8 type);

	class Window
	{
	private:
		// Serialized constants; do not change values of entries.
		static constexpr u32 ON_CLOSE = 0x00;
		static constexpr u32 ON_SUBMIT = 0x01;
		static constexpr u32 ON_FOCUS_CHANGE = 0x02;

		static constexpr size_t MAX_TREE_DEPTH = 64;

		// The ID and type are intrinsic to the box's identity, so they aren't
		// cleared in reset(). The ID is set by the constructor, whereas the
		// type is deserialized when the window is first opened.
		u64 m_id;
		WindowType m_type = WindowType::GUI;

		std::unordered_map<std::string, std::unique_ptr<Elem>> m_elems;
		std::vector<Elem *> m_ordered_elems;

		Root *m_root_elem;

		std::vector<std::string> m_style_strs;

		Elem *m_focused_elem;
		Elem *m_hovered_elem;

		bool m_uncloseable;
		u32 m_events;

	public:
		Window(u64 id) :
			m_id(id)
		{
			reset();
		}

		DISABLE_CLASS_COPY(Window)

		u64 getId() const { return m_id; }
		WindowType getType() const { return m_type; }

		const std::vector<Elem *> &getElems() { return m_ordered_elems; }

		Elem *getElem(const std::string &id, bool required);
		Elem *getNextElem(Elem *elem, bool reverse);

		Root *getRoot() { return m_root_elem; }
		Elem *getFocused() { return m_focused_elem; }
		Elem *getHovered() { return m_hovered_elem; }

		void clearElem(Elem *elem);

		const std::string *getStyleStr(u32 index) const;

		void reset();
		bool read(std::istream &is, bool opening);

		float getScale() const;

		SizeF getScreenSize() const;
		PosF getPointerPos() const;

		void drawRect(RectF dst, RectF clip, video::SColor color);
		void drawTexture(RectF dst, RectF clip, video::ITexture *texture,
				RectF src = RectF(0.0f, 0.0f, 1.0f, 1.0f), video::SColor tint = WHITE);

		void drawAll();

		bool isFocused() const;
		bool processInput(const SDL_Event &event);

	private:
		void enableEvent(u32 event);
		bool testEvent(u32 event) const;

		std::ostringstream createEvent(u32 event) const;

		// Warning: This method causes the window object to be destroyed.
		// Return immediately after use, and don't use the window object again.
		void destroyWindow();

		Elem *sendTreeInput(Elem *elem, const SDL_Event &event, bool direct);
		Elem *sendPointedInput(const SDL_Event &event);
		Elem *sendFocusedInput(const SDL_Event &event);

		void changeFocusedElem(Elem *new_focused, bool send_event);
		bool requestFocusedElem(Elem *new_focused, bool send_event);

		void focusNextElem(bool reverse);

		void focusPointedElem();
		void hoverPointedElem();

		bool isPointerOutside() const;

		void readElems(std::istream &is,
				std::unordered_map<Elem *, std::string> &elem_contents);
		bool readRootElem(std::istream &is);
		void readStyles(std::istream &is);

		bool updateElems(std::unordered_map<Elem *, std::string> &elem_contents,
				bool set_focus, Elem *new_focused);
		bool updateTree(Elem *elem, size_t depth);
	};
}
