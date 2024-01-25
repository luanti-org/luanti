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

namespace ui
{
	class Window;

	class Elem
	{
	public:
		// Serialized enum; do not change values of entries.
		enum Type : u8
		{
			ELEM = 0x00,
			ROOT = 0x01,
		};

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

	public:
		static std::unique_ptr<Elem> create(Type type, Window &window, std::string id);

		Elem(Window &window, std::string id);

		DISABLE_CLASS_COPY(Elem)

		virtual ~Elem() = default;

		Window &getWindow() { return m_window; }
		const Window &getWindow() const { return m_window; }

		const std::string &getId() const { return m_id; }
		virtual Type getType() const { return ELEM; }

		size_t getOrder() const { return m_order; }
		void setOrder(size_t order) { m_order = order; }

		Elem *getParent() { return m_parent; }
		const std::vector<Elem *> &getChildren() { return m_children; }

		Box &getMain() { return m_main_box; }

		virtual void reset();
		virtual void read(std::istream &is);

	protected:
		void enableEvent(u32 event);
		bool testEvent(u32 event) const;

		std::ostringstream createEvent(u32 event) const;

	private:
		void readChildren(std::istream &is);
	};
}
