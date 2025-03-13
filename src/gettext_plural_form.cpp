// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gettext_plural_form.h"
#include "util/string.h"

static size_t minsize(const GettextPluralForm::Ptr &form)
{
	return form ? form->size() : 0;
}

static size_t minsize(const GettextPluralForm::Ptr &f, const GettextPluralForm::Ptr &g)
{
	if (sizeof(g) > 0)
		return std::min(minsize(f), minsize(g));
	return f ? f->size() : 0;
}

class Identity: public GettextPluralForm
{
	public:
		Identity(size_t nplurals): GettextPluralForm(nplurals) {};
		NumT operator()(const NumT n) const override
		{
			return n;
		}
};

class ConstValue: public GettextPluralForm
{
	public:
		ConstValue(size_t nplurals, NumT val): GettextPluralForm(nplurals), value(val) {};
		NumT operator()(const NumT n) const override
		{
			return value;
		}
	private:
		NumT value;
};

template<template<typename> typename F>
class UnaryOperation: public GettextPluralForm
{
	public:
		UnaryOperation(const Ptr &op):
			GettextPluralForm(minsize(op)), op(op) {}
		NumT operator()(const NumT n) const override
		{
			if (operator bool())
				return func((*op)(n));
			return 0;
		}
	private:
		Ptr op;
		static constexpr F<NumT> func = {};
};

template<template<typename> typename F>
class BinaryOperation: public GettextPluralForm
{
	public:
		BinaryOperation(const Ptr &lhs, const Ptr &rhs):
			GettextPluralForm(minsize(lhs, rhs)),
			lhs(lhs), rhs(rhs) {}
		NumT operator()(const NumT n) const override
		{
			if (operator bool())
				return func((*lhs)(n), (*rhs)(n));
			return 0;
		}
	private:
		Ptr lhs, rhs;
		static constexpr F<NumT> func = {};
};

class TernaryOperation: public GettextPluralForm
{
	public:
		TernaryOperation(const Ptr &cond, const Ptr &val, const Ptr &alt):
			GettextPluralForm(std::min(minsize(cond), minsize(val, alt))),
			cond(cond), val(val), alt(alt) {}
		NumT operator()(const NumT n) const override
		{
			if (operator bool())
				return (*cond)(n) ? (*val)(n) : (*alt)(n);
			return 0;
		}
	private:
		Ptr cond, val, alt;
};

typedef std::pair<GettextPluralForm::Ptr, std::wstring_view> ParserResult;
typedef ParserResult (*Parser)(const size_t, std::wstring_view);

static ParserResult parse_expr(const size_t nplurals, std::wstring_view str);

template<Parser Parser, template<typename> typename Operator>
static ParserResult reduce_ltr(const size_t nplurals, const ParserResult &res, const wchar_t* pattern)
{
	if (!str_starts_with(res.second, pattern))
		return ParserResult(nullptr, res.second);
	auto next = Parser(nplurals, trim(res.second.substr(std::char_traits<wchar_t>::length(pattern))));
	if (!next.first)
		return next;
	next.first = GettextPluralForm::Ptr(new BinaryOperation<Operator>(res.first, next.first));
	next.second = trim(next.second);
	return next;
}

template<Parser Parser>
static ParserResult reduce_ltr(const size_t nplurals, const ParserResult &res, const wchar_t**)
{
	return ParserResult(nullptr, res.second);
}

template<Parser Parser, template<typename> typename Operator, template<typename> typename... Operators>
static ParserResult reduce_ltr(const size_t nplurals, const ParserResult &res, const wchar_t** patterns)
{
	auto next = reduce_ltr<Parser, Operator>(nplurals, res, patterns[0]);
	if (next.first || next.second != res.second)
		return next;
	return reduce_ltr<Parser, Operators...>(nplurals, res, patterns+1);
}

template<Parser Parser, template<typename> typename Operator, template<typename> typename... Operators>
static ParserResult parse_ltr(const size_t nplurals, std::wstring_view str, const wchar_t** patterns)
{
	auto &&pres = Parser(nplurals, str);
	if (!pres.first)
		return pres;
	pres.second = trim(pres.second);
	while (!pres.second.empty()) {
		auto next = reduce_ltr<Parser, Operator, Operators...>(nplurals, pres, patterns);
		if (!next.first)
			return pres;
		next.second = trim(next.second);
		pres = next;
	}
	return pres;
}

static ParserResult parse_atomic(const size_t nplurals, std::wstring_view str)
{
	if (str.empty())
		return ParserResult(nullptr, str);
	if (str[0] == 'n')
		return ParserResult(new Identity(nplurals), trim(str.substr(1)));

	wchar_t* endp;
	auto val = wcstoul(str.data(), &endp, 10);
	return ParserResult(new ConstValue(nplurals, val), trim(str.substr(endp-str.data())));
}

static ParserResult parse_parenthesized(const size_t nplurals, std::wstring_view str)
{
	if (str.empty())
		return ParserResult(nullptr, str);
	if (str[0] != '(')
		return parse_atomic(nplurals, str);
	auto result = parse_expr(nplurals, str.substr(1));
	if (result.first) {
		if (result.second.empty() || result.second[0] != ')')
			result.first = nullptr;
		else
			result.second = trim(result.second.substr(1));
	}
	return result;
}

static ParserResult parse_negation(const size_t nplurals, std::wstring_view str)
{
	if (str.empty())
		return ParserResult(nullptr, str);
	if (str[0] != '!')
		return parse_parenthesized(nplurals, str);
	auto result = parse_negation(nplurals, trim(str.substr(1)));
	if (result.first)
		result.first = GettextPluralForm::Ptr(new UnaryOperation<std::logical_not>(result.first));
	return result;
}

static ParserResult parse_multiplicative(const size_t nplurals, std::wstring_view str)
{
	static const wchar_t *patterns[] = { L"*", L"/", L"%" };
	return parse_ltr<parse_negation, std::multiplies, std::divides, std::modulus>(nplurals, str, patterns);
}

static ParserResult parse_additive(const size_t nplurals, std::wstring_view str)
{
	static const wchar_t *patterns[] = { L"+", L"-" };
	return parse_ltr<parse_multiplicative, std::plus, std::minus>(nplurals, str, patterns);
}

static ParserResult parse_comparison(const size_t nplurals, std::wstring_view str)
{
	static const wchar_t *patterns[] = { L"<=", L">=", L"<", L">" };
	return parse_ltr<parse_additive, std::less_equal, std::greater_equal, std::less, std::greater>(nplurals, str, patterns);
}

static ParserResult parse_equality(const size_t nplurals, std::wstring_view str)
{
	static const wchar_t *patterns[] = { L"==", L"!=" };
	return parse_ltr<parse_comparison, std::equal_to, std::not_equal_to>(nplurals, str, patterns);
}

static ParserResult parse_conjunction(const size_t nplurals, std::wstring_view str)
{
	static const wchar_t *and_pattern[] = { L"&&" };
	return parse_ltr<parse_equality, std::logical_and>(nplurals, str, and_pattern);
}

static ParserResult parse_disjunction(const size_t nplurals, std::wstring_view str)
{
	static const wchar_t *or_pattern[] = { L"||" };
	return parse_ltr<parse_conjunction, std::logical_or>(nplurals, str, or_pattern);
}

static ParserResult parse_ternary(const size_t nplurals, std::wstring_view str)
{
	auto pres = parse_disjunction(nplurals, str);
	if (pres.second.empty() || pres.second[0] != '?') // no ? :
		return pres;
	auto cond = pres.first;
	pres = parse_ternary(nplurals, trim(pres.second.substr(1)));
	if (pres.second.empty() || pres.second[0] != ':')
		return ParserResult(nullptr, pres.second);
	auto val = pres.first;
	pres = parse_ternary(nplurals, trim(pres.second.substr(1)));
	return ParserResult(new TernaryOperation(cond, val, pres.first), pres.second);
}

static ParserResult parse_expr(const size_t nplurals, std::wstring_view str)
{
	return parse_ternary(nplurals, trim(str));
}

GettextPluralForm::Ptr GettextPluralForm::parse(const size_t nplurals, std::wstring_view str)
{
	if (nplurals == 0)
		return nullptr;
	auto result = parse_expr(nplurals, str);
	if (!result.second.empty())
		return nullptr;
	return result.first;
}

GettextPluralForm::Ptr GettextPluralForm::parseHeaderLine(std::wstring_view str)
{
	if (!str_starts_with(str, L"Plural-Forms: nplurals=") || !str_ends_with(str, L";"))
		return nullptr;
	auto nplurals = wcstoul(str.data()+23, nullptr, 10);
	auto pos = str.find(L"plural=");
	if (pos == str.npos)
		return nullptr;
	return parse(nplurals, str.substr(pos+7, str.size()-pos-8));
}
