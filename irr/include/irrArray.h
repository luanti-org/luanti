// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine" and the "irrXML" project.
// For conditions of distribution and use, see copyright notice in irrlicht.h and irrXML.h

#pragma once
#include <algorithm>
#include <iterator>
#include <vector>
#include <cassert>

#include "irrTypes.h"
#include "irrMath.h"

namespace core
{

//! Self reallocating template array (like stl vector) with additional features.
/** Some features are: Heap sorting, binary search methods, easier debugging.
 */
template <class T>
class array
{
public:
	static_assert(!std::is_same<T, bool>::value,
			"core::array<T> with T = bool not supported. Use std::vector instead.");

	//! Default constructor for empty array.
	array() :
			m_data(), is_sorted(true)
	{
	}

	//! Constructs an array and allocates an initial chunk of memory.
	/** \param start_count Amount of elements to pre-allocate. */
	explicit array(u32 start_count) :
			m_data(), is_sorted(true)
	{
		m_data.reserve(start_count);
	}

	//! Copy constructor
	array(const array<T> &other) :
			m_data(other.m_data), is_sorted(other.is_sorted)
	{
	}

	//! Move constructor
	array(std::vector<T> &&data) :
			m_data(std::move(data)), is_sorted(false) {}

	//! Reallocates the array, make it bigger or smaller.
	/** \param new_size New size of array.
	\param canShrink Specifies whether the array is reallocated even if
	enough space is available. Setting this flag to false can speed up
	array usage, but may use more memory than required by the data.
	*/
	void reallocate(u32 new_size, bool canShrink = true)
	{
		size_t allocated = m_data.capacity();
		if (new_size < allocated) {
			if (canShrink) {
				// since capacity != size don't accidentally make it bigger
				if (m_data.size() > new_size)
					m_data.resize(new_size);
				m_data.shrink_to_fit();
			}
		} else {
			m_data.reserve(new_size);
		}
	}

	//! Adds an element at back of array.
	/** If the array is too small to add this new element it is made bigger.
	\param element: Element to add at the back of the array. */
	void push_back(const T &element)
	{
		m_data.push_back(element);
		is_sorted = false;
	}

	void push_back(T &&element)
	{
		m_data.push_back(std::move(element));
		is_sorted = false;
	}

	//! Adds an element at the front of the array.
	/** If the array is to small to add this new element, the array is
	made bigger. Please note that this is slow, because the whole array
	needs to be copied for this.
	\param element Element to add at the back of the array. */
	void push_front(const T &element)
	{
		m_data.insert(m_data.begin(), element);
		is_sorted = false;
	}

	void push_front(T &&element)
	{
		m_data.insert(m_data.begin(), std::move(element));
		is_sorted = false;
	}

	//! Insert item into array at specified position.
	/**
	\param element: Element to be inserted
	\param index: Where position to insert the new element. */
	void insert(const T &element, u32 index = 0)
	{
		assert(index <= m_data.size());
		auto pos = std::next(m_data.begin(), index);
		m_data.insert(pos, element);
		is_sorted = false;
	}

	//! Clears the array and deletes all allocated memory.
	void clear()
	{
		// vector::clear() reduces the size to 0, but doesn't free memory.
		// This swap is guaranteed to delete the allocated memory.
		std::vector<T>().swap(m_data);
		is_sorted = true;
	}

	//! Set (copy) data from given memory block
	/** \param newData data to set, must have newSize elements
	\param newSize Amount of elements in newData
	\param canShrink When true we reallocate the array even it can shrink.
	May reduce memory usage, but call is more whenever size changes.
	\param newDataIsSorted Info if you pass sorted/unsorted data
	*/
	void set_data(const T *newData, u32 newSize, bool newDataIsSorted = false, bool canShrink = false)
	{
		m_data.resize(newSize);
		if (canShrink) {
			m_data.shrink_to_fit();
		}
		std::copy(newData, newData + newSize, m_data.begin());
		is_sorted = newDataIsSorted;
	}

	//! Compare if given data block is identical to the data in our array
	/** Like operator ==, but without the need to create the array
	\param otherData Address to data against which we compare, must contain size elements
	\param size Amount of elements in otherData	*/
	bool equals(const T *otherData, u32 size) const
	{
		if (m_data.size() != size)
			return false;
		return std::equal(m_data.begin(), m_data.end(), otherData);
	}

	//! Sets the size of the array and allocates new elements if necessary.
	/** \param usedNow Amount of elements now used. */
	void set_used(u32 usedNow)
	{
		m_data.resize(usedNow);
	}

	//! Assignment operator
	array<T> &operator=(const array<T> &other)
	{
		if (this == &other)
			return *this;
		m_data = other.m_data;
		is_sorted = other.is_sorted;
		return *this;
	}

	array<T> &operator=(const std::vector<T> &other)
	{
		m_data = other;
		is_sorted = false;
		return *this;
	}

	//! Equality operator
	bool operator==(const array<T> &other) const
	{
		return equals(other.const_pointer(), other.size());
	}

	//! Inequality operator
	bool operator!=(const array<T> &other) const
	{
		return !(*this == other);
	}

	//! Direct access operator
	T &operator[](u32 index)
	{
		assert(index < m_data.size());
		return m_data[index];
	}

	//! Direct const access operator
	const T &operator[](u32 index) const
	{
		assert(index < m_data.size());
		return m_data[index];
	}

	//! Gets last element.
	T &getLast()
	{
		assert(!m_data.empty());
		return m_data.back();
	}

	//! Gets last element
	const T &getLast() const
	{
		assert(!m_data.empty());
		return m_data.back();
	}

	//! Gets a pointer to the array.
	/** \return Pointer to the array. */
	T *pointer()
	{
		return m_data.empty() ? nullptr : &m_data[0];
	}

	//! Gets a const pointer to the array.
	/** \return Pointer to the array. */
	const T *const_pointer() const
	{
		return m_data.empty() ? nullptr : &m_data[0];
	}

	//! Get number of occupied elements of the array.
	/** \return Size of elements in the array which are actually occupied. */
	u32 size() const
	{
		return static_cast<u32>(m_data.size());
	}

	//! Get amount of memory allocated.
	/** \return Amount of memory allocated. The amount of bytes
	allocated would be allocated_size() * sizeof(ElementTypeUsed); */
	u32 allocated_size() const
	{
		return m_data.capacity();
	}

	//! Check if array is empty.
	/** \return True if the array is empty false if not. */
	bool empty() const
	{
		return m_data.empty();
	}

	//! Sorts the array using heapsort.
	/** There is no additional memory waste and the algorithm performs
	O(n*log n) in worst case. */
	void sort()
	{
		if (!is_sorted) {
			std::sort(m_data.begin(), m_data.end());
			is_sorted = true;
		}
	}

	//! Performs a binary search for an element, returns -1 if not found.
	/** The array will be sorted before the binary search if it is not
	already sorted. Caution is advised! Be careful not to call this on
	unsorted const arrays, or the slower method will be used.
	\param element Element to search for.
	\return Position of the searched element if it was found,
	otherwise -1 is returned. */
	s32 binary_search(const T &element)
	{
		sort();
		return binary_search(element, 0, (s32)m_data.size() - 1);
	}

	//! Performs a binary search for an element if possible, returns -1 if not found.
	/** This method is for const arrays and so cannot call sort(), if the array is
	not sorted then linear_search will be used instead. Potentially very slow!
	\param element Element to search for.
	\return Position of the searched element if it was found,
	otherwise -1 is returned. */
	s32 binary_search(const T &element) const
	{
		if (is_sorted)
			return binary_search(element, 0, (s32)m_data.size() - 1);
		else
			return linear_search(element);
	}

	//! Performs a binary search for an element, returns -1 if not found.
	/** \param element: Element to search for.
	\param left First left index
	\param right Last right index.
	\return Position of the searched element if it was found, otherwise -1
	is returned. */
	s32 binary_search(const T &element, s32 left, s32 right) const
	{
		if (left > right)
			return -1;
		auto lpos = std::next(m_data.begin(), left);
		auto rpos = std::next(m_data.begin(), right);
		auto it = std::lower_bound(lpos, rpos, element);
		// *it = first element in [first, last) that is >= element, or last if not found.
		if (*it < element || element < *it)
			return -1;
		return static_cast<u32>(it - m_data.begin());
	}

	//! Performs a binary search for an element, returns -1 if not found.
	//! it is used for searching a multiset
	/** The array will be sorted before the binary search if it is not
	already sorted.
	\param element Element to search for.
	\param &last return lastIndex of equal elements
	\return Position of the first searched element if it was found,
	otherwise -1 is returned. */
	s32 binary_search_multi(const T &element, s32 &last)
	{
		sort();
		auto iters = std::equal_range(m_data.begin(), m_data.end(), element);
		if (iters.first == iters.second)
			return -1;
		last = static_cast<s32>((iters.second - m_data.begin()) - 1);
		return static_cast<s32>(iters.first - m_data.begin());
	}

	//! Finds an element in linear time, which is very slow.
	/** Use binary_search for faster finding. Only works if ==operator is
	implemented.
	\param element Element to search for.
	\return Position of the searched element if it was found, otherwise -1
	is returned. */
	s32 linear_search(const T &element) const
	{
		auto it = std::find(m_data.begin(), m_data.end(), element);
		if (it == m_data.end())
			return -1;
		return static_cast<u32>(it - m_data.begin());
	}

	//! Finds an element in linear time, which is very slow.
	/** Use binary_search for faster finding. Only works if ==operator is
	implemented.
	\param element: Element to search for.
	\return Position of the searched element if it was found, otherwise -1
	is returned. */
	s32 linear_reverse_search(const T &element) const
	{
		auto it = std::find(m_data.rbegin(), m_data.rend(), element);
		if (it == m_data.rend())
			return -1;
		size_t offset = it - m_data.rbegin();
		return m_data.size() - offset - 1;
	}

	//! Erases an element from the array.
	/** May be slow, because all elements following after the erased
	element have to be copied.
	\param index: Index of element to be erased. */
	void erase(u32 index)
	{
		assert(index < m_data.size());
		auto it = std::next(m_data.begin(), index);
		m_data.erase(it);
	}

	//! Erases some elements from the array.
	/** May be slow, because all elements following after the erased
	element have to be copied.
	\param index: Index of the first element to be erased.
	\param count: Amount of elements to be erased. */
	void erase(u32 index, s32 count)
	{
		if (index >= m_data.size() || count < 1)
			return;
		count = core::min_(count, (s32)m_data.size() - (s32)index);
		auto first = std::next(m_data.begin(), index);
		auto last = std::next(first, count);
		m_data.erase(first, last);
	}

	//! Sets if the array is sorted
	void set_sorted(bool _is_sorted)
	{
		is_sorted = _is_sorted;
	}

	//! Swap the content of this array container with the content of another array
	/** Afterward this object will contain the content of the other object and the other
	object will contain the content of this object.
	\param other Swap content with this object */
	void swap(array<T> &other)
	{
		m_data.swap(other.m_data);
		std::swap(is_sorted, other.is_sorted);
	}

	typedef T value_type;
	typedef u32 size_type;

private:
	std::vector<T> m_data;
	bool is_sorted;
};

} // end namespace core
