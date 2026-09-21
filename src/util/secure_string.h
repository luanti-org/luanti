// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 SFENCE <sfence.software@gmail.com>

#pragma once

#include "porting.h"
#include <istream>
#include <string>
#include <string_view>

template <typename StringT>
class BasicSecureString {
public:
	using string_type = StringT;
	using value_type = typename StringT::value_type;
	using view_type = std::basic_string_view<value_type>;
	using iterator = typename StringT::iterator;
	using const_iterator = typename StringT::const_iterator;

	BasicSecureString() = default;

	BasicSecureString(const string_type &s) :
		m_string(s)
	{
	}

	BasicSecureString(view_type s) :
		m_string(s)
	{
	}

	BasicSecureString(const value_type *s) :
		m_string(s)
	{
	}

	BasicSecureString(const BasicSecureString &other) :
		m_string(other.m_string)
	{
	}

	BasicSecureString(BasicSecureString &&other) noexcept :
		m_string(std::move(other.m_string))
	{
	}

	~BasicSecureString()
	{
		safeClear();
	}

	BasicSecureString &operator=(const string_type &s)
	{
		m_string = s;
		return *this;
	}

	BasicSecureString &operator=(view_type s)
	{
		m_string = s;
		return *this;
	}

	BasicSecureString &operator=(const value_type *s)
	{
		m_string = s;
		return *this;
	}

	BasicSecureString &operator=(const BasicSecureString &other)
	{
		m_string = other.m_string;
		return *this;
	}

	BasicSecureString &operator=(BasicSecureString &&other) noexcept
	{
		m_string = std::move(other.m_string);
		return *this;
	}

	value_type &operator[](size_t pos) { return m_string[pos]; }
	const value_type &operator[](size_t pos) const { return m_string[pos]; }

	BasicSecureString &operator+=(const BasicSecureString &other)
	{
		m_string += other.m_string;
		return *this;
	}

	BasicSecureString &operator+=(view_type s)
	{
		m_string += s;
		return *this;
	}

	BasicSecureString &operator+=(value_type c)
	{
		m_string += c;
		return *this;
	}

	bool operator==(const BasicSecureString &other) const { return m_string == other.m_string; }
	bool operator!=(const BasicSecureString &other) const { return m_string != other.m_string; }
	bool operator==(view_type s) const { return m_string == s; }
	bool operator!=(view_type s) const { return m_string != s; }

	// Overwrite memory with zeros and then clear
	void safeClear()
	{
		porting::secure_clear_memory(
				(void *)m_string.data(),
				m_string.size() * sizeof(value_type));
		m_string.clear();
	}

	const value_type *c_str() const { return m_string.c_str(); }
	const value_type *data() const { return m_string.data(); }
	value_type *data() { return m_string.data(); }
	size_t size() const { return m_string.size(); }
	size_t length() const { return m_string.length(); }
	bool empty() const { return m_string.empty(); }

	void clear() { m_string.clear(); }
	void reserve(size_t n) { m_string.reserve(n); }
	void resize(size_t n) { m_string.resize(n); }
	void push_back(value_type c) { m_string.push_back(c); }

	iterator begin() { return m_string.begin(); }
	iterator end() { return m_string.end(); }
	const_iterator begin() const { return m_string.begin(); }
	const_iterator end() const { return m_string.end(); }

	const string_type &str() const { return m_string; }

	// Hidden friends: operator+
	friend BasicSecureString operator+(const BasicSecureString &lhs, const BasicSecureString &rhs)
	{
		BasicSecureString result(lhs);
		result += rhs;
		return result;
	}

	friend BasicSecureString operator+(const BasicSecureString &lhs, const string_type &rhs)
	{
		BasicSecureString result(lhs);
		result += view_type(rhs);
		return result;
	}

	friend BasicSecureString operator+(const string_type &lhs, const BasicSecureString &rhs)
	{
		BasicSecureString result(lhs);
		result += rhs;
		return result;
	}

	friend BasicSecureString operator+(const BasicSecureString &lhs, view_type rhs)
	{
		BasicSecureString result(lhs);
		result += rhs;
		return result;
	}

	friend BasicSecureString operator+(view_type lhs, const BasicSecureString &rhs)
	{
		BasicSecureString result(lhs);
		result += rhs;
		return result;
	}

	friend BasicSecureString operator+(const BasicSecureString &lhs, const value_type *rhs)
	{
		BasicSecureString result(lhs);
		result += view_type(rhs);
		return result;
	}

	friend BasicSecureString operator+(const value_type *lhs, const BasicSecureString &rhs)
	{
		BasicSecureString result(lhs);
		result += rhs;
		return result;
	}

	// Hidden friends: allow std::getline to read directly into the internal string
	friend std::basic_istream<value_type, typename StringT::traits_type> &
	getline(std::basic_istream<value_type, typename StringT::traits_type> &is,
			BasicSecureString &str)
	{
		return std::getline(is, str.m_string);
	}

	friend std::basic_istream<value_type, typename StringT::traits_type> &
	getline(std::basic_istream<value_type, typename StringT::traits_type> &is,
			BasicSecureString &str,
			value_type delim)
	{
		return std::getline(is, str.m_string, delim);
	}

private:
	string_type m_string;
};

using SecureString = BasicSecureString<std::string>;
using SecureWString = BasicSecureString<std::wstring>;
