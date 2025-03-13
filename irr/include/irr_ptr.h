/*
Minetest
Copyright (C) 2018 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once
#include <type_traits>
#include <utility>
namespace irr { class IReferenceCounted; }

/** Shared pointer for IrrLicht objects.
 *
 * It should only be used for user-managed objects, i.e. those created with
 * the @c new operator or @c create* functions, like:
 * `irr_ptr<scene::IMeshBuffer> buf{new scene::SMeshBuffer()};`
 * The reference counting is *not* balanced as new objects have reference
 * count set to one, and the @c irr_ptr constructor (and @c reset) assumes
 * ownership of that reference.
 *
 * It shouldn’t be used for engine-managed objects, including those created
 * with @c addTexture and similar methods. Constructing @c irr_ptr directly
 * from such object is a bug and may lead to a crash. Indirect construction
 * is possible though; see the @c grab free function for details and use cases.
 */
template <class ReferenceCounted>
class irr_ptr
{
	ReferenceCounted *value = nullptr;

public:
	irr_ptr() noexcept = default;

	irr_ptr(std::nullptr_t) noexcept {}

	irr_ptr(const irr_ptr &b) noexcept { grab(b.get()); }

	irr_ptr(irr_ptr &&b) noexcept { reset(b.release()); }

	template <typename B,
			std::enable_if_t<std::is_convertible_v<B *, ReferenceCounted *>, bool> = true>
	irr_ptr(const irr_ptr<B> &b) noexcept
	{
		grab(b.get());
	}

	template <typename B,
			std::enable_if_t<std::is_convertible_v<B *, ReferenceCounted *>, bool> = true>
	irr_ptr(irr_ptr<B> &&b) noexcept
	{
		reset(b.release());
	}

	/** Constructs a shared pointer out of a plain one to control object lifetime.
	 * @param object The object, usually returned by some @c create* function.
	 * @note Move semantics: reference counter is *not* increased.
	 * @warning Never wrap any @c add* function with this!
	 */
	explicit irr_ptr(ReferenceCounted *object) noexcept { reset(object); }

	~irr_ptr() { reset(); }

	irr_ptr &operator=(const irr_ptr &b) noexcept
	{
		grab(b.get());
		return *this;
	}

	irr_ptr &operator=(irr_ptr &&b) noexcept
	{
		reset(b.release());
		return *this;
	}

	template <typename B,
			std::enable_if_t<std::is_convertible_v<B *, ReferenceCounted *>, bool> = true>
	irr_ptr &operator=(const irr_ptr<B> &b) noexcept
	{
		grab(b.get());
		return *this;
	}

	template <typename B,
			std::enable_if_t<std::is_convertible_v<B *, ReferenceCounted *>, bool> = true>
	irr_ptr &operator=(irr_ptr<B> &&b) noexcept
	{
		reset(b.release());
		return *this;
	}

	ReferenceCounted &operator*() const noexcept { return *value; }
	ReferenceCounted *operator->() const noexcept { return value; }
	explicit operator ReferenceCounted *() const noexcept { return value; }
	explicit operator bool() const noexcept { return !!value; }

	/** Returns the stored pointer.
	 */
	ReferenceCounted *get() const noexcept { return value; }

	/** Returns the stored pointer, erasing it from this class.
	 * @note Move semantics: reference counter is not changed.
	 */
	ReferenceCounted *release() noexcept
	{
		ReferenceCounted *object = value;
		value = nullptr;
		return object;
	}

	/** Drops stored pointer replacing it with the given one.
	 * @note Move semantics: reference counter is *not* increased.
	 */
	void reset(ReferenceCounted *object = nullptr) noexcept
	{
		static_assert(std::is_base_of_v<irr::IReferenceCounted, ReferenceCounted>,
				"Class is not an IReferenceCounted");
		if (value)
			value->drop();
		value = object;
	}

	/** Drops stored pointer replacing it with the given one.
	 * @note Copy semantics: reference counter *is* increased.
	 */
	void grab(ReferenceCounted *object) noexcept
	{
		static_assert(std::is_base_of_v<irr::IReferenceCounted, ReferenceCounted>,
				"Class is not an IReferenceCounted");
		if (object)
			object->grab();
		reset(object);
	}
};

/** Constructs a shared pointer as a *secondary* reference to an object
 *
 * This function is intended to make a temporary reference to an object which
 * is owned elsewhere so that it is not destroyed too early. To achieve that
 * it does balanced reference counting, i.e. reference count is increased
 * in this function and decreased when the returned pointer is destroyed.
 */
template <class ReferenceCounted>
[[nodiscard]]
irr_ptr<ReferenceCounted> grab(ReferenceCounted *object) noexcept
{
	irr_ptr<ReferenceCounted> ptr;
	ptr.grab(object);
	return ptr;
}

template <typename ReferenceCounted>
bool operator==(const irr_ptr<ReferenceCounted> &a, const irr_ptr<ReferenceCounted> &b) noexcept
{
	return a.get() == b.get();
}

template <typename ReferenceCounted>
bool operator==(const irr_ptr<ReferenceCounted> &a, const ReferenceCounted *b) noexcept
{
	return a.get() == b;
}

template <typename ReferenceCounted>
bool operator==(const ReferenceCounted *a, const irr_ptr<ReferenceCounted> &b) noexcept
{
	return a == b.get();
}

template <typename ReferenceCounted>
bool operator!=(const irr_ptr<ReferenceCounted> &a, const irr_ptr<ReferenceCounted> &b) noexcept
{
	return a.get() != b.get();
}

template <typename ReferenceCounted>
bool operator!=(const irr_ptr<ReferenceCounted> &a, const ReferenceCounted *b) noexcept
{
	return a.get() != b;
}

template <typename ReferenceCounted>
bool operator!=(const ReferenceCounted *a, const irr_ptr<ReferenceCounted> &b) noexcept
{
	return a != b.get();
}

/** Same as std::make_unique<T>, but for irr_ptr.
 */
template <class T, class... Args>
irr_ptr<T> make_irr(Args&&... args)
{
	return irr_ptr<T>(new T(std::forward<Args>(args)...));
}
