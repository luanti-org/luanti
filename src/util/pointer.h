// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "util/basic_macros.h"
#include <cassert>
#include <cstring>
#include <string_view>

template <typename T>
class Buffer
{
public:
	Buffer()
	{
		m_size = 0;
		data = nullptr;
	}
	Buffer(size_t size)
	{
		m_size = size;
		if (size != 0) {
			data = new T[size];
		} else {
			data = nullptr;
		}
	}

	// Disable class copy
	Buffer(const Buffer &) = delete;
	Buffer &operator=(const Buffer &) = delete;

	Buffer(Buffer &&buffer)
	{
		m_size = buffer.m_size;
		if (m_size != 0) {
			data = buffer.data;
			buffer.data = nullptr;
			buffer.m_size = 0;
		} else {
			data = nullptr;
		}
	}
	// Copies whole buffer
	Buffer(const T *t, size_t size)
	{
		m_size = size;
		if (size != 0) {
			data = new T[size];
			memcpy(data, t, sizeof(T) * size);
		} else {
			data = nullptr;
		}
	}

	~Buffer()
	{
		drop();
	}

	Buffer& operator=(Buffer &&buffer)
	{
		if (this == &buffer)
			return *this;
		drop();
		m_size = buffer.m_size;
		if (m_size != 0) {
			data = buffer.data;
			buffer.data = nullptr;
			buffer.m_size = 0;
		} else {
			data = nullptr;
		}
		return *this;
	}

	void copyTo(Buffer &buffer) const
	{
		buffer.drop();
		buffer.m_size = m_size;
		if (m_size != 0) {
			buffer.data = new T[m_size];
			memcpy(buffer.data, data, sizeof(T) * m_size);
		} else {
			buffer.data = nullptr;
		}
	}

	T & operator[](size_t i) const
	{
		return data[i];
	}
	T * operator*() const
	{
		return data;
	}

	size_t getSize() const
	{
		return m_size;
	}

	operator std::string_view() const
	{
		if (!data) {
			return std::string_view();
		}
		return std::string_view(reinterpret_cast<char*>(data), m_size);
	}

private:
	void drop()
	{
		delete[] data;
	}
	T *data;
	size_t m_size;
};

/************************************************
 *           !!!  W A R N I N G  !!!            *
 *                                              *
 * This smart pointer class is NOT thread safe. *
 * ONLY use in a single-threaded context!       *
 *                                              *
 ************************************************/
template <typename T>
class SharedBuffer
{
public:
	SharedBuffer()
	{
		m_size = 0;
		data = nullptr;
		refcount = new u32;
		(*refcount) = 1;
	}

	SharedBuffer(size_t size)
	{
		m_size = size;
		if (m_size != 0) {
			data = new T[m_size];
		} else {
			data = nullptr;
		}

		refcount = new u32;
		memset(data, 0, sizeof(T) * m_size);
		(*refcount) = 1;
	}

	SharedBuffer(const SharedBuffer &buffer)
	{
		m_size = buffer.m_size;
		data = buffer.data;
		refcount = buffer.refcount;
		(*refcount)++;
	}

	SharedBuffer & operator=(const SharedBuffer &buffer)
	{
		if (this == &buffer)
			return *this;
		drop();
		m_size = buffer.m_size;
		data = buffer.data;
		refcount = buffer.refcount;
		(*refcount)++;
		return *this;
	}

	//! Copies whole buffer
	SharedBuffer(const T *t, size_t size)
	{
		m_size = size;
		if (m_size != 0) {
			data = new T[m_size];
			memcpy(data, t, sizeof(T) * m_size);
		} else {
			data = nullptr;
		}
		refcount = new u32;
		(*refcount) = 1;
	}

	//! Copies whole buffer
	SharedBuffer(const Buffer<T> &buffer) : SharedBuffer(*buffer, buffer.getSize())
	{
	}

	~SharedBuffer()
	{
		drop();
	}

	T & operator[](size_t i) const
	{
		assert(i < m_size);
		return data[i];
	}

	T * operator*() const
	{
		return data;
	}

	size_t getSize() const
	{
		return m_size;
	}

	operator Buffer<T>() const
	{
		return Buffer<T>(data, m_size);
	}

private:
	void drop()
	{
		assert((*refcount) > 0);
		(*refcount)--;
		if (*refcount == 0) {
			delete[] data;
			delete refcount;
		}
	}

	T *data;
	size_t m_size;
	u32 *refcount;
};

// This class is not thread-safe!
class IntrusiveReferenceCounted {
public:
	IntrusiveReferenceCounted() = default;
	virtual ~IntrusiveReferenceCounted() = default;
	void grab() noexcept { ++m_refcount; }
	void drop() noexcept { if (--m_refcount == 0) delete this; }

	DISABLE_CLASS_COPY(IntrusiveReferenceCounted)
private:
	u32 m_refcount = 1;
};
