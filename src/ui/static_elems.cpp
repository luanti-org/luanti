// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#include "ui/static_elems.h"

#include "debug.h"
#include "log.h"
#include "ui/manager.h"
#include "util/serialize.h"

namespace ui
{
	void Root::reset()
	{
		Elem::reset();

		m_backdrop_box.reset();
	}

	void Root::read(std::istream &is)
	{
		auto super = newIs(readStr32(is));
		Elem::read(super);

		u32 set_mask = readU32(is);

		if (testShift(set_mask))
			m_backdrop_box.read(is);

		m_backdrop_box.setContent({&getMain()});
	}

	bool Root::isBoxFocused(const Box &box) const
	{
		return box.getItem() == BACKDROP_BOX ? getWindow().isFocused() : isFocused();
	}
}
