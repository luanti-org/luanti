// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "debug.h" // sanity_check
#include "exceptions.h"
#include "threading/mutex_auto_lock.h"
#include "threading/semaphore.h"
#include <list>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <cassert>
#include <limits>

/*
	Queue with unique values with fast checking of value existence
*/

template<typename Value>
class UniqueQueue
{
public:

	/*
	Does nothing if value is already queued.
	Return value:
	true: value added
	false: value already exists
	*/
	bool push_back(const Value& value)
	{
		if (m_set.insert(value).second)
		{
			m_queue.push(value);
			return true;
		}
		return false;
	}

	void pop_front()
	{
		m_set.erase(m_queue.front());
		m_queue.pop();
	}

	const Value& front() const
	{
		return m_queue.front();
	}

	size_t size() const
	{
		return m_queue.size();
	}

	bool empty() const
	{
		return m_queue.empty();
	}

private:
	std::set<Value> m_set;
	std::queue<Value> m_queue;
};

/*
	Thread-safe map
*/

template<typename Key, typename Value>
class MutexedMap
{
public:
	MutexedMap() = default;

	void set(const Key &name, const Value &value)
	{
		MutexAutoLock lock(m_mutex);
		m_values[name] = value;
	}

	bool get(const Key &name, Value *result) const
	{
		MutexAutoLock lock(m_mutex);
		auto n = m_values.find(name);
		if (n == m_values.end())
			return false;
		if (result)
			*result = n->second;
		return true;
	}

	std::vector<Value> getValues() const
	{
		MutexAutoLock lock(m_mutex);
		std::vector<Value> result;
		result.reserve(m_values.size());
		for (auto it = m_values.begin(); it != m_values.end(); ++it)
			result.push_back(it->second);
		return result;
	}

	void clear()
	{
		MutexAutoLock lock(m_mutex);
		m_values.clear();
	}

private:
	std::map<Key, Value> m_values;
	mutable std::mutex m_mutex;
};


/*
	Thread-safe double-ended queue
*/

template<typename T>
class MutexedQueue
{
public:
	template<typename Key, typename U, typename Caller, typename CallerData>
	friend class RequestQueue;

	MutexedQueue() = default;

	bool empty() const
	{
		MutexAutoLock lock(m_mutex);
		return m_queue.empty();
	}

	void push_back(const T &t)
	{
		MutexAutoLock lock(m_mutex);
		m_queue.push_back(t);
		m_signal.post();
	}

	void push_back(T &&t)
	{
		MutexAutoLock lock(m_mutex);
		m_queue.push_back(std::move(t));
		m_signal.post();
	}

	/* this version of pop_front returns an empty element of T on timeout.
	* Make sure default constructor of T creates a recognizable "empty" element
	*/
	T pop_frontNoEx(u32 wait_time_max_ms)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.front());
			m_queue.pop_front();
			return t;
		}

		return T();
	}

	T pop_front(u32 wait_time_max_ms)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.front());
			m_queue.pop_front();
			return t;
		}

		throw ItemNotFoundException("MutexedQueue: queue is empty");
	}

	T pop_frontNoEx()
	{
		m_signal.wait();

		MutexAutoLock lock(m_mutex);

		T t = std::move(m_queue.front());
		m_queue.pop_front();
		return t;
	}

	T pop_back(u32 wait_time_max_ms=0)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.back());
			m_queue.pop_back();
			return t;
		}

		throw ItemNotFoundException("MutexedQueue: queue is empty");
	}

	/* this version of pop_back returns an empty element of T on timeout.
	* Make sure default constructor of T creates a recognizable "empty" element
	*/
	T pop_backNoEx(u32 wait_time_max_ms)
	{
		if (m_signal.wait(wait_time_max_ms)) {
			MutexAutoLock lock(m_mutex);

			T t = std::move(m_queue.back());
			m_queue.pop_back();
			return t;
		}

		return T();
	}

	T pop_backNoEx()
	{
		m_signal.wait();

		MutexAutoLock lock(m_mutex);

		T t = std::move(m_queue.back());
		m_queue.pop_back();
		return t;
	}

protected:
	std::mutex &getMutex() { return m_mutex; }

	std::deque<T> &getQueue() { return m_queue; }

	std::deque<T> m_queue;
	mutable std::mutex m_mutex;
	Semaphore m_signal;
};

/*
	LRU cache
*/

template<typename K, typename V>
class LRUCache
{
public:
	LRUCache(size_t limit, void (*cache_miss)(void *data, const K &key, V *dest),
			void *data)
	{
		m_limit = limit;
		m_cache_miss = cache_miss;
		m_cache_miss_data = data;
	}

	void setLimit(size_t limit)
	{
		m_limit = limit;
		invalidate();
	}

	void invalidate()
	{
		m_map.clear();
		m_queue.clear();
	}

	const V *lookupCache(K key)
	{
		typename cache_type::iterator it = m_map.find(key);
		V *ret;
		if (it != m_map.end()) {
			// found!

			cache_entry_t &entry = it->second;

			ret = &entry.second;

			// update the usage information
			m_queue.erase(entry.first);
			m_queue.push_front(key);
			entry.first = m_queue.begin();
		} else {
			// cache miss -- enter into cache
			cache_entry_t &entry =
				m_map[key];
			ret = &entry.second;
			m_cache_miss(m_cache_miss_data, key, &entry.second);

			// delete old entries
			if (m_queue.size() == m_limit) {
				const K &id = m_queue.back();
				m_map.erase(id);
				m_queue.pop_back();
			}

			m_queue.push_front(key);
			entry.first = m_queue.begin();
		}
		return ret;
	}
private:
	void (*m_cache_miss)(void *data, const K &key, V *dest);
	void *m_cache_miss_data;
	size_t m_limit;
	typedef typename std::template pair<typename std::template list<K>::iterator, V> cache_entry_t;
	typedef std::template map<K, cache_entry_t> cache_type;
	cache_type m_map;
	// we can't use std::deque here, because its iterators get invalidated
	std::list<K> m_queue;
};

/*
	Map that can be safely modified (insertion, deletion) during iteration
	Caveats:
	- you cannot insert null elements
	- you have to check for null elements during iteration, those are ones already deleted
	- size() and empty() don't work during iteration
	- not thread-safe in any way

	How this is implemented:
	- there are two maps: new and real
	- if inserting duration iteration, the value is inserted into the "new" map
	- if deleting during iteration, the value is set to null (to be GC'd later)
	- when iteration finishes the "new" map is merged into the "real" map
*/

template<typename K, typename V>
class ModifySafeMap
{
public:
	// this allows bare pointers but also e.g. std::unique_ptr
	static_assert(std::is_default_constructible<V>::value,
		"Value type must be default constructible");
	static_assert(std::is_constructible<bool, V>::value,
		"Value type must be convertible to bool");
	static_assert(std::is_move_assignable<V>::value,
		"Value type must be move-assignable");

	typedef K key_type;
	typedef V mapped_type;

	ModifySafeMap() {
		// the null value must convert to false and all others to true, but
		// we can't statically check the latter.
		sanity_check(!null_value);
	}
	~ModifySafeMap() {
		assert(!m_iterating);
	}

	// possible to implement but we don't need it
	DISABLE_CLASS_COPY(ModifySafeMap)
	ALLOW_CLASS_MOVE(ModifySafeMap)

	const V &get(const K &key) const {
		if (m_iterating) {
			auto it = m_new.find(key);
			if (it != m_new.end())
				return it->second;
		}
		auto it = m_values.find(key);
		// This conditional block was converted from a ternary to ensure no
		// temporary values are created in evaluating the return expression,
		// which could cause a dangling reference.
		if (it != m_values.end())
			return it->second;
		else
			return null_value;
	}

	void put(const K &key, const V &value) {
		if (!value) {
			assert(false);
			return;
		}
		if (m_iterating) {
			auto it = m_values.find(key);
			if (it != m_values.end()) {
				it->second = V();
				m_garbage++;
			}
			m_new[key] = value;
		} else {
			m_values[key] = value;
		}
	}

	void put(const K &key, V &&value) {
		if (!value) {
			assert(false);
			return;
		}
		if (m_iterating) {
			auto it = m_values.find(key);
			if (it != m_values.end()) {
				it->second = V();
				m_garbage++;
			}
			m_new[key] = std::move(value);
		} else {
			m_values[key] = std::move(value);
		}
	}

	V take(const K &key) {
		V ret = V();
		if (m_iterating) {
			auto it = m_new.find(key);
			if (it != m_new.end()) {
				ret = std::move(it->second);
				m_new.erase(it);
			}
		}
		auto it = m_values.find(key);
		if (it == m_values.end())
			return ret;
		if (!ret)
			ret = std::move(it->second);
		if (m_iterating) {
			it->second = V();
			m_garbage++;
		} else {
			m_values.erase(it);
		}
		return ret;
	}

	bool remove(const K &key) {
		return !!take(key);
	}

	/// @warning not constant-time!
	size_t size() const {
		if (m_iterating) {
			// This is by no means impossible to determine, it's just annoying
			// to code and we happen to not need this.
			return unknown;
		}
		assert(m_new.empty());
		if (m_garbage == 0)
			return m_values.size();
		size_t n = 0;
		for (auto &it : m_values)
			n += !it.second ? 0 : 1;
		return n;
	}

	/// @warning not constant-time!
	bool empty() const {
		if (m_iterating)
			return false; // maybe
		if (m_garbage == 0)
			return m_values.empty();
		for (auto &it : m_values) {
			if (!!it.second)
				return false;
		}
		return true;
	}

	auto iter() { return IterationHelper(this); }

	void clear() {
		if (m_iterating) {
			for (auto &it : m_values)
				it.second = V();
			m_garbage = m_values.size();
		} else {
			m_values.clear();
			m_garbage = 0;
		}
	}

	static inline const V null_value = V();

	// returned by size() if called during iteration
	static constexpr size_t unknown = static_cast<size_t>(-1);

protected:
	void merge_new() {
		assert(!m_iterating);
		if (!m_new.empty()) {
			m_new.merge(m_values); // entries in m_new take precedence
			m_values.clear();
			std::swap(m_values, m_new);
		}
	}

	void collect_garbage() {
		assert(!m_iterating);
		if (m_values.size() < GC_MIN_SIZE || m_garbage < m_values.size() / 2)
			return;
		for (auto it = m_values.begin(); it != m_values.end(); ) {
			if (!it->second)
				it = m_values.erase(it);
			else
				++it;
		}
		m_garbage = 0;
	}

	struct IterationHelper {
		friend class ModifySafeMap<K, V>;
		~IterationHelper() {
			assert(m->m_iterating);
			m->m_iterating--;
			if (!m->m_iterating) {
				m->merge_new();
				m->collect_garbage();
			}
		}

		auto begin() { return m->m_values.cbegin(); }
		auto end() { return m->m_values.cend(); }

	private:
		IterationHelper(ModifySafeMap<K, V> *parent) : m(parent) {
			assert(m->m_iterating < std::numeric_limits<decltype(m_iterating)>::max());
			m->m_iterating++;
		}

		ModifySafeMap<K, V> *m;
	};

private:
	std::map<K, V> m_values;
	std::map<K, V> m_new;
	unsigned int m_iterating = 0;
	// approximate amount of null-placeholders in m_values, reliable for != 0 tests
	size_t m_garbage = 0;

	static constexpr size_t GC_MIN_SIZE = 30;
};
