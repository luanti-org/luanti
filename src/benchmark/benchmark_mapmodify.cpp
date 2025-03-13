// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 sfan5

#include "catch.h"
#include "util/container.h"

// Testing the standard library is not useful except to compare
//#define TEST_STDLIB

using TestMap = ModifySafeMap<u16, void*>;

static inline void fill(TestMap &map, size_t n)
{
	map.clear();
	for (size_t i = 0; i < n; i++)
		map.put(i, reinterpret_cast<TestMap::mapped_type>(0x40000U + i));
}

static inline void pollute(TestMap &map)
{
	auto dummy = reinterpret_cast<TestMap::mapped_type>(123);
	// produce some garbage to avoid best case behaviour
	map.put(0xffff, dummy);
	for (auto it : map.iter()) {
		(void)it;
		map.remove(0xffff);
		break;
	}
}

static inline void remove(TestMap &map, size_t offset, size_t count)
{
	for (size_t i = 0; i < count; i++)
		map.remove(static_cast<TestMap::key_type>(i + offset));
}

#define BENCH_ITERATE_(_label, _count, _best) \
	BENCHMARK_ADVANCED(_label)(Catch::Benchmark::Chronometer meter) { \
		TestMap map; \
		fill(map, _count); \
		if (!_best) pollute(map); \
		meter.measure([&] { \
			size_t x = map.size(); \
			for (auto &it : map.iter()) { \
				if (!it.second) \
					continue; \
				x ^= reinterpret_cast<intptr_t>(it.second); \
			} \
			return x; \
		}); \
	};

#define BENCH_ITERATE(_count) \
	BENCH_ITERATE_("iterate_" #_count, _count, 0) \
	BENCH_ITERATE_("iterate_bestcase_" #_count, _count, 1)

#define BENCH_REMOVE(_count) \
	BENCHMARK_ADVANCED("remove_" #_count)(Catch::Benchmark::Chronometer meter) { \
		TestMap map; \
		fill(map, _count); \
		meter.measure([&] { \
			for (auto it : map.iter()) { \
				(void)it; \
				remove(map, (_count) / 7, (_count) / 2); /* delete half */ \
				break; \
			} \
		}); \
	};

TEST_CASE("ModifySafeMap") {
	BENCH_ITERATE(50)
	BENCH_ITERATE(400)
	BENCH_ITERATE(1000)

	BENCH_REMOVE(50)
	BENCH_REMOVE(400)
	BENCH_REMOVE(1000)
}

using TestMap2 = std::map<u16, void*>;

static inline void fill2(TestMap2 &map, size_t n)
{
	map.clear();
	for (size_t i = 0; i < n; i++)
		map.emplace(i, reinterpret_cast<TestMap2::mapped_type>(0x40000U + i));
}

static inline void remove2(TestMap2 &map, size_t offset, size_t count)
{
	for (size_t i = 0; i < count; i++)
		map.erase(static_cast<TestMap2::key_type>(i + offset));
}

#define BENCH2_ITERATE(_count) \
	BENCHMARK_ADVANCED("iterate_" #_count)(Catch::Benchmark::Chronometer meter) { \
		TestMap2 map; \
		fill2(map, _count); \
		meter.measure([&] { \
			size_t x = map.size(); \
			/* mirrors what ActiveObjectMgr::step used to do */ \
			std::vector<TestMap2::key_type> keys; \
			keys.reserve(x); \
			for (auto &it : map) \
				keys.push_back(it.first); \
			for (auto key : keys) { \
				auto it = map.find(key); \
				if (it == map.end()) \
					continue; \
				x ^= reinterpret_cast<intptr_t>(it->second); \
			} \
			return x; \
		}); \
	};

#define BENCH2_REMOVE(_count) \
	BENCHMARK_ADVANCED("remove_" #_count)(Catch::Benchmark::Chronometer meter) { \
		TestMap2 map; \
		fill2(map, _count); \
		meter.measure([&] { \
			/* no overhead so no fake iteration */ \
			remove2(map, (_count) / 7, (_count) / 2); \
		}); \
	};

#ifdef TEST_STDLIB
TEST_CASE("std::map") {
	BENCH2_ITERATE(50)
	BENCH2_ITERATE(400)
	BENCH2_ITERATE(1000)

	BENCH2_REMOVE(50)
	BENCH2_REMOVE(400)
	BENCH2_REMOVE(1000)
}
#endif
