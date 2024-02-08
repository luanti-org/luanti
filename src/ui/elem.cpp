// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#include "ui/elem.h"

#include "debug.h"
#include "log.h"
#include "ui/manager.h"
#include "ui/window.h"
#include "util/serialize.h"

// Include every element header for Elem::create()
#include "ui/clickable_elems.h"
#include "ui/static_elems.h"

#include <SDL2/SDL.h>

namespace ui
{
	std::unique_ptr<Elem> Elem::create(Type type, Window &window, std::string id)
	{
		std::unique_ptr<Elem> elem = nullptr;

#define CREATE(name, type)										\
	case name:													\
		elem = std::make_unique<type>(window, std::move(id));	\
		break

		switch (type) {
			CREATE(ELEM, Elem);
			CREATE(ROOT, Root);
			CREATE(BUTTON, Button);
			CREATE(TOGGLE, Toggle);
			CREATE(OPTION, Option);
		default:
			return nullptr;
		}

#undef CREATE

		// It's a pain to call reset() in the constructor of every single
		// element due to how virtual functions work in C++, so we reset
		// elements after creating them here.
		elem->reset();
		return elem;
	}

	Elem::Elem(Window &window, std::string id) :
		m_window(window),
		m_id(std::move(id)),
		m_main_box(*this, Box::NO_GROUP, MAIN_BOX)
	{}

	Elem::~Elem()
	{
		// Make sure we don't leave any dangling pointers in the window.
		m_window.clearElem(this);
	}

	void Elem::reset()
	{
		m_order = (size_t)-1;

		m_parent = nullptr;
		m_children.clear();

		m_main_box.reset();

		m_events = 0;
	}

	void Elem::read(std::istream &is)
	{
		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			readChildren(is);
		if (testShift(set_mask))
			m_main_box.read(is);

		std::vector<Box *> content;
		for (Elem *elem : m_children) {
			content.push_back(&elem->getMain());
		}
		m_main_box.setContent(std::move(content));
	}

	bool Elem::isFocused() const
	{
		return m_window.isFocused() && m_window.getFocused() == this;
	}

	void Elem::enableEvent(u32 event)
	{
		m_events |= (1 << event);
	}

	bool Elem::testEvent(u32 event) const
	{
		return m_events & (1 << event);
	}

	std::ostringstream Elem::createEvent(u32 event) const
	{
		auto os = newOs();

		writeU8(os, Manager::ELEM_EVENT);
		writeU64(os, m_window.getId());
		writeU8(os, event);
		writeU8(os, getType());
		writeNullStr(os, m_id);

		return os;
	}

	void Elem::readChildren(std::istream &is)
	{
		u32 num_children = readU32(is);

		for (size_t i = 0; i < num_children; i++) {
			std::string id = readNullStr(is);

			Elem *child = m_window.getElem(id, true);
			if (child == nullptr) {
				continue;
			}

			/* Check if this child already has a parent before adding it as a
			 * child. Elements are deserialized in unspecified order rather
			 * than a prefix order of parents before their children, so
			 * isolated circular element refrences are still possible at this
			 * point. However, cycles including the root are impossible.
			 */
			if (child->m_parent != nullptr) {
				errorstream << "Element \"" << id << "\" already has parent \"" <<
					child->m_parent->m_id << "\"" << std::endl;
			} else if (child == m_window.getRoot()) {
				errorstream << "Element \"" << id <<
					"\" is the root element and cannot have a parent" << std::endl;
			} else {
				m_children.push_back(child);
				child->m_parent = this;
			}
		}
	}
}
