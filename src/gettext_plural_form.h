// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <string_view>
#include <memory>
#include <functional>

// Note that this only implements a subset of C expressions. See:
// https://git.savannah.gnu.org/gitweb/?p=gettext.git;a=blob;f=gettext-runtime/intl/plural.y
class GettextPluralForm
{
public:
	using NumT = unsigned long;
	using Function = std::function<NumT(NumT)>;
	using Ptr = std::shared_ptr<GettextPluralForm>;

	GettextPluralForm(std::wstring_view str);

	size_t size() const
	{
		return nplurals;
	};
	NumT operator()(const NumT n) const {
		return func ? func(n) : 0;
	}
	operator bool() const
	{
		return nplurals > 0;
	}

	static Ptr parseHeaderLine(std::wstring_view str) {
		return Ptr(new GettextPluralForm(str));
	}
private:
	size_t nplurals;
	Function func;
};
