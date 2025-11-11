// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <string_view>
#include <memory>
#include <functional>

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

	// Note that this function does not perform any bounds check as the number of plural
	// translations provided by the translation file may deviate from nplurals,
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
	// The number of plural forms.
	size_t nplurals = 0;

	// The formula for determining the plural form based on the input value; see
	// https://www.gnu.org/software/gettext/manual/html_node/Translating-plural-forms.html
	// for details.
	Function func = nullptr;
};
