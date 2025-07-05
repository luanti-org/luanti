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
	class Root : public Elem
	{
	private:
		Box m_backdrop_box;

		static constexpr u32 BACKDROP_BOX = 1;

	public:
		Root(Window &window, std::string id) :
			Elem(window, std::move(id)),
			m_backdrop_box(*this, Box::NO_GROUP, BACKDROP_BOX)
		{}

		virtual Type getType() const override { return ROOT; }

		Box &getBackdrop() { return m_backdrop_box; }

		virtual void reset() override;
		virtual void read(std::istream &is) override;

		virtual bool isBoxFocused(const Box &box) const override;
	};
}
