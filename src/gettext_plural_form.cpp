// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

/* This file implements a recursive descent parser for gettext plural forms.
 * Left recursion (for left-associative operators) is implemented by parse_ltr, which iteratively attempts to reduce
 * expressions from operations of the same precedence. This should not be confused with reduce_ltr, which recurses
 * through a list of operators with the same precedence (not the input string!) to search for a match.
 * Note that this only implements a subset of C expressions. See:
 * https://git.savannah.gnu.org/gitweb/?p=gettext.git;a=blob;f=gettext-runtime/intl/plural.y
 */

#include "gettext_plural_form.h"
#include "util/string.h"
#include <type_traits>

static GettextPluralForm::NumT identity(GettextPluralForm::NumT n)
{
	return n;
}

static GettextPluralForm::NumT ternary_op(GettextPluralForm::NumT n, const GettextPluralForm::Function &cond,
		const GettextPluralForm::Function &val, const GettextPluralForm::Function &alt)
{
	return cond(n) ? val(n) : alt(n);
}

template<template<typename> typename Func, class... Args>
static GettextPluralForm::Function wrap_op(Args&&... args)
{
	return std::bind(Func<GettextPluralForm::NumT>(), std::bind(std::move(args), std::placeholders::_1)...);
}

typedef std::pair<GettextPluralForm::Function, std::wstring_view> ParserResult;
typedef ParserResult (*Parser)(std::wstring_view);

static ParserResult parse_expr(std::wstring_view str);

template<Parser Parser, template<typename> typename Operator>
static ParserResult reduce_ltr_single(const ParserResult &res, const std::wstring &pattern)
{
	if (!str_starts_with(res.second, pattern))
		return ParserResult(nullptr, res.second);
	auto next = Parser(trim(res.second.substr(pattern.size())));
	if (!next.first)
		return next;
	next.first = wrap_op<Operator>(res.first, next.first);
	next.second = trim(next.second);
	return next;
}

template<Parser Parser>
static ParserResult reduce_ltr(const ParserResult &res)
{
	return ParserResult(nullptr, res.second);
}

template<Parser Parser, template<typename> typename Operator, template<typename> typename... Operators>
static ParserResult reduce_ltr(const ParserResult &res, const std::wstring &pattern, const typename std::conditional<1,std::wstring,Operators<GettextPluralForm::NumT>>::type&... patterns)
{
	auto next = reduce_ltr_single<Parser, Operator>(res, pattern);
	if (next.first || next.second != res.second)
		return next;
	return reduce_ltr<Parser, Operators...>(res, patterns...);
}

template<Parser Parser, template<typename> typename... Operators>
static ParserResult parse_ltr(std::wstring_view str, const typename std::conditional<1,std::wstring,Operators<GettextPluralForm::NumT>>::type&... patterns)
{
	auto &&pres = Parser(str);
	if (!pres.first)
		return pres;
	pres.second = trim(pres.second);
	while (!pres.second.empty()) {
		auto next = reduce_ltr<Parser, Operators...>(pres, patterns...);
		if (!next.first)
			return pres;
		next.second = trim(next.second);
		pres = next;
	}
	return pres;
}

static ParserResult parse_atomic(std::wstring_view str)
{
	if (str.empty())
		return ParserResult(nullptr, str);
	if (str[0] == 'n')
		return ParserResult(identity, trim(str.substr(1)));

	wchar_t* endp;
	auto val = wcstoul(str.data(), &endp, 10);
	return ParserResult([val](GettextPluralForm::NumT _) -> GettextPluralForm::NumT { return val; },
			trim(str.substr(endp-str.data())));
}

static ParserResult parse_parenthesized(std::wstring_view str)
{
	if (str.empty())
		return ParserResult(nullptr, str);
	if (str[0] != '(')
		return parse_atomic(str);
	auto result = parse_expr(str.substr(1));
	if (result.first) {
		if (result.second.empty() || result.second[0] != ')')
			result.first = nullptr;
		else
			result.second = trim(result.second.substr(1));
	}
	return result;
}

static ParserResult parse_negation(std::wstring_view str)
{
	if (str.empty())
		return ParserResult(nullptr, str);
	if (str[0] != '!')
		return parse_parenthesized(str);
	auto result = parse_negation(trim(str.substr(1)));
	if (result.first)
		result.first = wrap_op<std::logical_not>(result.first);
	return result;
}

template<typename T> struct safe_divides {
	T operator()(T lhs, T rhs) const
	{
		return rhs == 0 ? 0 : (lhs / rhs);
	}
};

template<typename T> struct safe_modulus {
	T operator()(T lhs, T rhs) const
	{
		return rhs == 0 ? 0 : (lhs % rhs);
	}
};

static ParserResult parse_multiplicative(std::wstring_view str)
{
	return parse_ltr<parse_negation, std::multiplies, safe_divides, safe_modulus>(str, L"*", L"/", L"%");
}

static ParserResult parse_additive(std::wstring_view str)
{
	return parse_ltr<parse_multiplicative, std::plus, std::minus>(str, L"+", L"-");
}

static ParserResult parse_comparison(std::wstring_view str)
{
	return parse_ltr<parse_additive, std::less_equal, std::greater_equal, std::less, std::greater>(str, L"<=", L">=", L"<", L">");
}

static ParserResult parse_equality(std::wstring_view str)
{
	return parse_ltr<parse_comparison, std::equal_to, std::not_equal_to>(str, L"==", L"!=");
}

static ParserResult parse_conjunction(std::wstring_view str)
{
	return parse_ltr<parse_equality, std::logical_and>(str, L"&&");
}

static ParserResult parse_disjunction(std::wstring_view str)
{
	return parse_ltr<parse_conjunction, std::logical_or>(str, L"||");
}

static ParserResult parse_ternary(std::wstring_view str)
{
	auto pres = parse_disjunction(str);
	if (pres.second.empty() || pres.second[0] != '?') // no ? :
		return pres;
	auto cond = pres.first;
	pres = parse_ternary(trim(pres.second.substr(1)));
	if (pres.second.empty() || pres.second[0] != ':')
		return ParserResult(nullptr, pres.second);
	auto val = pres.first;
	pres = parse_ternary(trim(pres.second.substr(1)));
	return ParserResult(std::bind(ternary_op, std::placeholders::_1,
				std::move(cond), std::move(val), std::move(pres.first)), pres.second);
}

static ParserResult parse_expr(std::wstring_view str)
{
	return parse_ternary(trim(str));
}

static GettextPluralForm::Function parse(std::wstring_view str)
{
	auto result = parse_expr(str);
	if (!result.second.empty())
		return nullptr;
	return result.first;
}

GettextPluralForm::GettextPluralForm(std::wstring_view str)
{
	if (!str_starts_with(str, L"Plural-Forms: nplurals=") || !str_ends_with(str, L";"))
		return;
	auto size = wcstoul(str.data()+23, nullptr, 10);
	auto pos = str.find(L"plural=");
	if (pos == str.npos)
		return;
	auto result = parse(str.substr(pos+7, str.size()-pos-8));
	if (size > 0 && result) {
		nplurals = size;
		func = result;
	}
}
