// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#pragma once

#include "ui/box.h"
#include "ui/elem.h"
#include "ui/helpers.h"

#include <iostream>
#include <string>

namespace ui
{
	class Button : public Elem
	{
	private:
		// Serialized constants; do not change values of entries.
		static constexpr u32 ON_PRESS = 0x00;

		bool m_disabled;

	public:
		Button(Window &window, std::string id) :
			Elem(window, std::move(id))
		{}

		virtual Type getType() const override { return BUTTON; }

		virtual void reset() override;
		virtual void read(std::istream &is) override;

		virtual bool isBoxDisabled(const Box &box) const override { return m_disabled; }

		virtual bool processInput(const SDL_Event &event) override;

	private:
		void onPress();
	};

	class Toggle : public Elem
	{
	private:
		// Serialized constants; do not change values of entries.
		static constexpr u32 ON_PRESS = 0x00;
		static constexpr u32 ON_CHANGE = 0x01;

		bool m_disabled;
		bool m_selected = false; // Persistent

	public:
		Toggle(Window &window, std::string id) :
			Elem(window, std::move(id))
		{}

		virtual Type getType() const override { return TOGGLE; }

		virtual void reset() override;
		virtual void read(std::istream &is) override;

		virtual bool isBoxSelected(const Box &box) const override { return m_selected; }
		virtual bool isBoxDisabled(const Box &box) const override { return m_disabled; }

		virtual bool processInput(const SDL_Event &event) override;

	private:
		void onPress();
	};

	class Option : public Elem
	{
	private:
		// Serialized constants; do not change values of entries.
		static constexpr u32 ON_PRESS = 0x00;
		static constexpr u32 ON_CHANGE = 0x01;

		bool m_disabled;
		std::string m_family;

		bool m_selected = false; // Persistent

	public:
		Option(Window &window, std::string id) :
			Elem(window, std::move(id))
		{}

		virtual Type getType() const override { return OPTION; }

		virtual void reset() override;
		virtual void read(std::istream &is) override;

		virtual bool isBoxSelected(const Box &box) const override { return m_selected; }
		virtual bool isBoxDisabled(const Box &box) const override { return m_disabled; }

		virtual bool processInput(const SDL_Event &event) override;

	private:
		void onPress();
		void onChange(bool selected);
	};
}
