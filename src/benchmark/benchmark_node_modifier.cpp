// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"
#include "constants.h"
#include "node_modifier.h"
#include <array>
#include <algorithm>

TEST_CASE("benchmark_node_modifier_lookup")
{
	NodeModifierManager manager;
	NodeModifier modifier("benchmark:light");
	modifier.m_modified_props_mask = MP_LightSource;
	modifier.m_light_source = 14;
	u16 id = manager.add(std::move(modifier));

	// One block, with the X-fastest traversal used by mesh generation.
	std::vector<v3s16> positions;
	for (s16 z = 0; z < MAP_BLOCKSIZE; ++z)
	for (s16 y = 0; y < MAP_BLOCKSIZE; ++y)
	for (s16 x = 0; x < MAP_BLOCKSIZE; ++x)
		positions.emplace_back(x, y, z);

	for (size_t count : {16, 256, 4096}) {
		AppliedNodeModifiersList list;
		for (size_t i = 0; i < count; ++i)
			REQUIRE(list.add(positions[i * positions.size() / count], id));

		BENCHMARK("mesh-order lookup, modifiers=" + std::to_string(count)) {
			u32 sum = 0;
			for (v3s16 pos : positions)
				sum += list.resolveLighting(pos, manager).light_source;
			return sum;
		};

		// A repeatable approximation of eight local samples per node. These
		// measure lookup only, not complete mesh generation or light propagation.
		BENCHMARK("eight lookups per node, modifiers=" + std::to_string(count)) {
			u32 sum = 0;
			for (v3s16 pos : positions) {
				for (s16 offset = 0; offset < 8; ++offset) {
					v3s16 sample((pos.X + (offset & 1)) % MAP_BLOCKSIZE,
						(pos.Y + ((offset >> 1) & 1)) % MAP_BLOCKSIZE,
						(pos.Z + ((offset >> 2) & 1)) % MAP_BLOCKSIZE);
					sum += list.resolveLighting(sample, manager).light_source;
				}
			}
			return sum;
		};
	}
}

namespace {

// Flat-vector baseline with the same per-position ordering as the adaptive list.
class FlatModifiers {
public:
	bool add(v3s16 pos, u16 id)
	{
		auto first = std::lower_bound(entries.begin(), entries.end(), pos,
				[](const AppliedNodeModifier &entry, v3s16 p) { return entry.pos < p; });
		auto last = first;
		while (last != entries.end() && last->pos == pos)
			++last;
		for (auto it = first; it != last; ++it) {
			if (it->id == id) {
				std::rotate(it, it + 1, last);
				return true;
			}
		}
		if (last - first == MAX_NODE_MODIFIERS)
			return false;
		entries.insert(last, {pos, id});
		return true;
	}

	bool remove(v3s16 pos, u16 id)
	{
		auto it = std::lower_bound(entries.begin(), entries.end(), pos,
				[](const AppliedNodeModifier &entry, v3s16 p) { return entry.pos < p; });
		for (; it != entries.end() && it->pos == pos; ++it) {
			if (it->id == id) {
				entries.erase(it);
				return true;
			}
		}
		return false;
	}

	size_t size() const { return entries.size(); }

private:
	std::vector<AppliedNodeModifier> entries;
};

std::vector<AppliedNodeModifier> mutationEntries(size_t nodes, u16 per_node, bool clustered = false)
{
	std::vector<AppliedNodeModifier> entries;
	// An odd multiplier permutes the 4096 positions without random-library
	// variation. Small inputs are spread throughout the block.
	for (size_t i = 0; i < nodes; ++i) {
		const size_t p = clustered ? nodes - i - 1 : (i * 4051) % 4096;
		for (u16 id = 0; id < per_node; ++id)
			entries.push_back({v3s16(p / 256, (p / 16) % 16, p % 16), id});
	}
	return entries;
}

template <typename List>
void fillModifiers(List &list, const std::vector<AppliedNodeModifier> &entries)
{
	for (const auto &entry : entries)
		list.add(entry.pos, entry.id);
}

template <typename List>
size_t churnModifiers(List &list, const std::vector<AppliedNodeModifier> &entries)
{
	// Keep occupancy stable, including near split boundaries. Each pair
	// restores the list, so repeated measurements use the same contents.
	size_t changes = 0;
	for (const auto &entry : entries) {
		changes += list.remove(entry.pos, entry.id);
		changes += list.add(entry.pos, entry.id);
	}
	return changes;
}

} // namespace

TEST_CASE("benchmark_node_modifier_mutation")
{
	for (bool clustered : {false, true})
	for (size_t nodes : {16, 64, 128, 256, 4096}) {
		for (u16 per_node : {1, 4}) {
			auto entries = mutationEntries(nodes, per_node, clustered);
			const auto label = std::string(clustered ? ", clustered" : ", spread") +
					", nodes=" + std::to_string(nodes) +
					", modifiers/node=" + std::to_string(per_node);
			AppliedNodeModifiersList adaptive;
			FlatModifiers flat;
			fillModifiers(adaptive, entries);
			fillModifiers(flat, entries);
			REQUIRE(adaptive.size() == entries.size());
			REQUIRE(flat.size() == entries.size());
			REQUIRE(churnModifiers(adaptive, entries) == 2 * entries.size());
			REQUIRE(churnModifiers(flat, entries) == 2 * entries.size());

			BENCHMARK("adaptive build" + label) {
				AppliedNodeModifiersList list;
				fillModifiers(list, entries);
				return list.size();
			};
			BENCHMARK("flat build" + label) {
				FlatModifiers list;
				fillModifiers(list, entries);
				return list.size();
			};
			BENCHMARK("adaptive remove/add" + label) {
				return churnModifiers(adaptive, entries);
			};
			BENCHMARK("flat remove/add" + label) {
				return churnModifiers(flat, entries);
			};
		}
	}
}
