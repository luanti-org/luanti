// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 grorp, Gregor Parzefall <grorp@posteo.de>

#pragma once
#include <memory>
#include "IGUIElement.h"

// We cannot use irr_ptr for Irrlicht GUI elements we own.
// Option 1: Pass IGUIElement* returned by IGUIEnvironment::add* into irr_ptr
//           constructor.
//           -> We steal the reference owned by IGUIEnvironment and drop it later,
//           causing the IGUIElement to be deleted while IGUIEnvironment still
//           references it.
// Option 2: Pass IGUIElement* returned by IGUIEnvironment::add* into irr_ptr::grab.
//           -> We add another reference and drop it later, but since IGUIEnvironment
//           still references the IGUIElement, it is never deleted.
// To make IGUIEnvironment drop its reference to the IGUIElement, we have to call
// IGUIElement::remove, so that's what we'll do.
template <typename T>
std::shared_ptr<T> grab_gui_element(T *element)
{
	static_assert(std::is_base_of_v<gui::IGUIElement, T>,
			"grab_gui_element only works for IGUIElement");
	return std::shared_ptr<T>(element, [](T *e) {
		e->remove();
	});
}
