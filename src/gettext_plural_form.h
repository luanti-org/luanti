// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <string_view>
#include <memory>

// Note that this only implements a subset of C expressions. See:
// https://git.savannah.gnu.org/gitweb/?p=gettext.git;a=blob;f=gettext-runtime/intl/plural.y
class GettextPluralForm
{
public:
	using NumT = unsigned long;
	using Ptr = std::shared_ptr<GettextPluralForm>;

	size_t size() const
	{
		return nplurals;
	};
	virtual NumT operator()(const NumT) const = 0;
	virtual operator bool() const
	{
		return size() > 0;
	}
	virtual ~GettextPluralForm() {};

	static GettextPluralForm::Ptr parse(const size_t nplurals, std::wstring_view str);
	static GettextPluralForm::Ptr parseHeaderLine(std::wstring_view str);
protected:
	GettextPluralForm(size_t nplurals): nplurals(nplurals) {};
private:
	const size_t nplurals;
};
