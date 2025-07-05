// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#include "ui/clickable_elems.h"

#include "debug.h"
#include "log.h"
#include "ui/manager.h"
#include "util/serialize.h"

namespace ui
{
	void Button::reset()
	{
		Elem::reset();

		m_disabled = false;
	}

	void Button::read(std::istream &is)
	{
		auto super = newIs(readStr32(is));
		Elem::read(super);

		u32 set_mask = readU32(is);

		m_disabled = testShift(set_mask);

		if (testShift(set_mask))
			enableEvent(ON_PRESS);
	}

	bool Button::processInput(const SDL_Event &event)
	{
		return getMain().processFullPress(event, UI_CALLBACK(onPress));
	}

	void Button::onPress()
	{
		if (!m_disabled && testEvent(ON_PRESS)) {
			g_manager.sendMessage(createEvent(ON_PRESS).str());
		}
	}

	void Toggle::reset()
	{
		Elem::reset();

		m_disabled = false;
	}

	void Toggle::read(std::istream &is)
	{
		auto super = newIs(readStr32(is));
		Elem::read(super);

		u32 set_mask = readU32(is);

		m_disabled = testShift(set_mask);
		testShiftBool(set_mask, m_selected);

		if (testShift(set_mask))
			enableEvent(ON_PRESS);
		if (testShift(set_mask))
			enableEvent(ON_CHANGE);
	}

	bool Toggle::processInput(const SDL_Event &event)
	{
		return getMain().processFullPress(event, UI_CALLBACK(onPress));
	}

	void Toggle::onPress()
	{
		if (m_disabled) {
			return;
		}

		m_selected = !m_selected;

		// Send both a press and a change event since both occurred.
		if (testEvent(ON_PRESS)) {
			g_manager.sendMessage(createEvent(ON_PRESS).str());
		}
		if (testEvent(ON_CHANGE)) {
			auto os = createEvent(ON_CHANGE);
			writeU8(os, m_selected);

			g_manager.sendMessage(os.str());
		}
	}

	void Option::reset()
	{
		Elem::reset();

		m_disabled = false;
		m_family.clear();
	}

	void Option::read(std::istream &is)
	{
		auto super = newIs(readStr32(is));
		Elem::read(super);

		u32 set_mask = readU32(is);

		m_disabled = testShift(set_mask);
		testShiftBool(set_mask, m_selected);

		if (testShift(set_mask))
			m_family = readNullStr(is);

		if (testShift(set_mask))
			enableEvent(ON_PRESS);
		if (testShift(set_mask))
			enableEvent(ON_CHANGE);
	}

	bool Option::processInput(const SDL_Event &event)
	{
		return getMain().processFullPress(event, UI_CALLBACK(onPress));
	}

	void Option::onPress()
	{
		if (m_disabled) {
			return;
		}

		// Send a press event for this pressed option button.
		if (testEvent(ON_PRESS)) {
			g_manager.sendMessage(createEvent(ON_PRESS).str());
		}

		// Select this option button unconditionally before deselecting the
		// others in the family.
		onChange(true);

		// If this option button has no family, then don't do anything else
		// since there may be other buttons with the same empty family string.
		if (m_family.empty()) {
			return;
		}

		// If we find any other option buttons in this family, deselect them.
		for (Elem *elem : getWindow().getElems()) {
			if (elem->getType() != getType()) {
				continue;
			}

			Option *option = (Option *)elem;
			if (option->m_family == m_family && option != this) {
				option->onChange(false);
			}
		}
	}

	void Option::onChange(bool selected)
	{
		bool was_selected = m_selected;
		m_selected = selected;

		// If the state of the option button changed, send a change event.
		if (was_selected != m_selected && testEvent(ON_CHANGE)) {
			auto os = createEvent(ON_CHANGE);
			writeU8(os, m_selected);

			g_manager.sendMessage(os.str());
		}
	}
}
