// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#pragma once

#include "ui/box.h"
#include "ui/helpers.h"
#include "util/basic_macros.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

union SDL_Event;

namespace ui
{
	class Window;

#define UI_CALLBACK(method)                          \
	[](Elem &elem) {                                 \
		static_cast<decltype(*this)>(elem).method(); \
	}

	class Elem
	{
	public:
		// Serialized enum; do not change values of entries.
		enum Type : u8
		{
			ELEM = 0x00,
			ROOT = 0x01,
			BUTTON = 0x02,
			TOGGLE = 0x03,
			OPTION = 0x04,
		};

		// The main box is always the zeroth item in the Box::NO_GROUP group.
		static constexpr u32 MAIN_BOX = 0;

	private:
		// The window and ID are intrinsic to the element's identity, so they
		// are set by the constructor and aren't cleared in reset() or changed
		// in read().
		Window &m_window;
		std::string m_id;

		size_t m_order;

		Elem *m_parent;
		std::vector<Elem *> m_children;

		Box m_main_box;
		u64 m_hovered_box = Box::NO_ID; // Persistent
		u64 m_pressed_box = Box::NO_ID; // Persistent

		u32 m_events;

	public:
		static std::unique_ptr<Elem> create(Type type, Window &window, std::string id);

		Elem(Window &window, std::string id);

		DISABLE_CLASS_COPY(Elem)

		virtual ~Elem();

		Window &getWindow() { return m_window; }
		const Window &getWindow() const { return m_window; }

		const std::string &getId() const { return m_id; }
		virtual Type getType() const { return ELEM; }

		size_t getOrder() const { return m_order; }
		void setOrder(size_t order) { m_order = order; }

		Elem *getParent() { return m_parent; }
		const std::vector<Elem *> &getChildren() { return m_children; }

		Box &getMain() { return m_main_box; }

		u64 getHoveredBox() const { return m_hovered_box; }
		u64 getPressedBox() const { return m_pressed_box; }

		void setHoveredBox(u64 id) { m_hovered_box = id; }
		void setPressedBox(u64 id) { m_pressed_box = id; }

		virtual void reset();
		virtual void read(std::istream &is);

		bool isFocused() const;

		virtual bool isBoxFocused (const Box &box) const { return isFocused(); }
		virtual bool isBoxSelected(const Box &box) const { return false; }
		virtual bool isBoxHovered (const Box &box) const { return box.getId() == m_hovered_box; }
		virtual bool isBoxPressed (const Box &box) const { return box.getId() == m_pressed_box; }
		virtual bool isBoxDisabled(const Box &box) const { return false; }

		virtual bool processInput(const SDL_Event &event) { return false; }

	protected:
		void enableEvent(u32 event);
		bool testEvent(u32 event) const;

		std::ostringstream createEvent(u32 event) const;

	private:
		void readChildren(std::istream &is);
	};
}
