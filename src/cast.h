// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

template <class T>
struct type_identity {
	typedef T type;
};

/// Coerses implicit casts. Use this over static_cast when possible.
template <class U>
U icast (typename type_identity<U>::type val) {
	return val;
}