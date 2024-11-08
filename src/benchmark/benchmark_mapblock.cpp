// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Minetest Authors

#include "catch.h"
#include "mapblock.h"
#include <vector>

typedef std::vector<MapBlock*> MBContainer;

static void allocateSome(MBContainer &vec, u32 n)
{
	vec.reserve(vec.size() + n);
	for (u32 i = 0; i < n; i++) {
		auto *mb = new MapBlock(v3s16(i & 0xff, 0, (i >> 8) & S16_MAX), nullptr);
		vec.push_back(mb);
	}
}

static void freeSome(MBContainer &vec, u32 n)
{
	// deallocate from end since that has no cost moving data inside the vector
	u32 start_i = 0;
	if (vec.size() > n)
		start_i = vec.size() - n;
	for (u32 i = start_i; i < vec.size(); i++)
		delete vec[i];
	vec.resize(start_i);
}

static inline void freeAll(MBContainer &vec) { freeSome(vec, vec.size()); }

// usage patterns inspired by ClientMap::updateDrawList()
static void workOnMetadata(const MBContainer &vec)
{
	for (MapBlock *block : vec) {
#if CHECK_CLIENT_BUILD()
		bool foo = !!block->mesh;
#else
		bool foo = true;
#endif

		if (block->refGet() > 2)
			block->refDrop();

		v3s16 pos = block->getPos() * MAP_BLOCKSIZE;
		if (foo)
			pos += v3s16(MAP_BLOCKSIZE / 2);

		if (pos.getDistanceFrom(v3s16(0)) > 30000)
			continue;

		block->resetUsageTimer();
		block->refGrab();
	}
}

// usage patterns inspired by LBMManager::applyLBMs()
static u32 workOnNodes(const MBContainer &vec)
{
	u32 foo = 0;
	for (MapBlock *block : vec) {
		block->resetUsageTimer();

		if (block->isOrphan())
			continue;

		v3s16 pos_of_block = block->getPosRelative();
		v3s16 pos;
		MapNode n;
		for (pos.X = 0; pos.X < MAP_BLOCKSIZE; pos.X++) {
			for (pos.Y = 0; pos.Y < MAP_BLOCKSIZE; pos.Y++) {
				for (pos.Z = 0; pos.Z < MAP_BLOCKSIZE; pos.Z++) {
					n = block->getNodeNoCheck(pos);

					if (n.getContent() == CONTENT_AIR) {
						auto p = pos + pos_of_block;
						foo ^= p.X + p.Y + p.Z;
					}
				}
			}
		}
	}
	return foo;
}

// usage patterns inspired by ABMHandler::apply()
// touches both metadata and node data at the same time
static u32 workOnBoth(const MBContainer &vec)
{
	int foo = 0;
	for (MapBlock *block : vec) {
		block->contents.clear();

		bool want_contents_cached = block->contents.empty() && !block->do_not_cache_contents;

		v3s16 p0;
		for(p0.X=0; p0.X<MAP_BLOCKSIZE; p0.X++)
		for(p0.Y=0; p0.Y<MAP_BLOCKSIZE; p0.Y++)
		for(p0.Z=0; p0.Z<MAP_BLOCKSIZE; p0.Z++)
		{
			MapNode n = block->getNodeNoCheck(p0);
			content_t c = n.getContent();

			if (want_contents_cached && !CONTAINS(block->contents, c)) {
				if (block->contents.size() >= 10) {
					want_contents_cached = false;
					block->do_not_cache_contents = true;
					block->contents.clear();
					block->contents.shrink_to_fit();
				} else {
					block->contents.push_back(c);
				}
			}
		}

		foo += block->contents.size();
	}
	return foo;
}

#define BENCH1(_count) \
	BENCHMARK_ADVANCED("allocate_" #_count)(Catch::Benchmark::Chronometer meter) { \
		MBContainer vec; \
		const u32 pcount = _count / meter.runs(); \
		meter.measure([&] { \
			allocateSome(vec, pcount); \
			return vec.size(); \
		}); \
		freeAll(vec); \
	}; \
	BENCHMARK_ADVANCED("testCase1_" #_count)(Catch::Benchmark::Chronometer meter) { \
		MBContainer vec; \
		allocateSome(vec, _count); \
		meter.measure([&] { \
			workOnMetadata(vec); \
		}); \
		freeAll(vec); \
	}; \
	BENCHMARK_ADVANCED("testCase2_" #_count)(Catch::Benchmark::Chronometer meter) { \
		MBContainer vec; \
		allocateSome(vec, _count); \
		meter.measure([&] { \
			return workOnNodes(vec); \
		}); \
		freeAll(vec); \
	}; \
	BENCHMARK_ADVANCED("testCase3_" #_count)(Catch::Benchmark::Chronometer meter) { \
		MBContainer vec; \
		allocateSome(vec, _count); \
		meter.measure([&] { \
			return workOnBoth(vec); \
		}); \
		freeAll(vec); \
	}; \
	BENCHMARK_ADVANCED("free_" #_count)(Catch::Benchmark::Chronometer meter) { \
		MBContainer vec; \
		allocateSome(vec, _count); \
		/* catch2 does multiple runs so we have to be careful to not dealloc too many */ \
		const u32 pcount = _count / meter.runs(); \
		meter.measure([&] { \
			freeSome(vec, pcount); \
			return vec.size(); \
		}); \
		freeAll(vec); \
	};

TEST_CASE("benchmark_mapblock") {
	BENCH1(900)
	BENCH1(2200)
	BENCH1(7500) // <- default client_mapblock_limit
}
