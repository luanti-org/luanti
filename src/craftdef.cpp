// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "craftdef.h"

#include "irrlichttypes.h"
#include "log.h"
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <queue>
#include "gamedef.h"
#include "inventory.h"
#include "util/serialize.h"
#include "util/string.h"
#include "util/numeric.h"
#include "util/strfnd.h"
#include "exceptions.h"

inline bool isGroupRecipeStr(const std::string &rec_name)
{
	return str_starts_with(rec_name, "group:");
}

static bool hasGroupItem(const std::vector<std::string> &recipe)
{
	for (const auto &item : recipe) {
		if (isGroupRecipeStr(item))
			return true;
	}
	return false;
}

inline u64 getHashForString(const std::string &recipe_str)
{
	/*errorstream << "Hashing craft string  \"" << recipe_str << '"';*/
	return murmur_hash_64_ua(recipe_str.data(), recipe_str.length(), 0xdeadbeef);
}

static u64 getHashForGrid(CraftHashType type, const std::vector<std::string> &grid_names)
{
	switch (type) {
		case CRAFT_HASH_TYPE_ITEM_NAMES: {
			std::ostringstream os;
			bool is_first = true;
			for (const std::string &grid_name : grid_names) {
				if (!grid_name.empty()) {
					os << (is_first ? "" : "\n") << grid_name;
					is_first = false;
				}
			}
			return getHashForString(os.str());
		} case CRAFT_HASH_TYPE_COUNT: {
			u64 cnt = 0;
			for (const std::string &grid_name : grid_names)
				if (!grid_name.empty())
					cnt++;
			return cnt;
		} case CRAFT_HASH_TYPE_UNHASHED:
			return 0;
	}
	// invalid CraftHashType
	assert(false);
	return 0;
}

// Check if input matches recipe
// Takes recipe groups into account
static bool inputItemMatchesRecipe(const std::string &inp_name,
		const std::string &rec_name, IItemDefManager *idef)
{
	// Exact name
	if (inp_name == rec_name)
		return true;

	// Group
	if (isGroupRecipeStr(rec_name) && idef->isKnown(inp_name)) {
		const struct ItemDefinition &def = idef->get(inp_name);
		Strfnd f(rec_name.substr(6));
		bool all_groups_match = true;
		do {
			std::string check_group = f.next(",");
			if (itemgroup_get(def.groups, check_group) == 0) {
				all_groups_match = false;
				break;
			}
		} while (!f.at_end());
		if (all_groups_match)
			return true;
	}

	// Didn't match
	return false;
}

// Deserialize an itemstring then return the name of the item
static std::string craftGetItemName(const std::string &itemstring, IGameDef *gamedef)
{
	ItemStack item;
	item.deSerialize(itemstring, gamedef->idef());
	return item.name;
}

// (mapcar craftGetItemName itemstrings)
static std::vector<std::string> craftGetItemNames(
		const std::vector<std::string> &itemstrings, IGameDef *gamedef)
{
	std::vector<std::string> result;
	result.reserve(itemstrings.size());
	for (const auto &itemstring : itemstrings) {
		result.push_back(craftGetItemName(itemstring, gamedef));
	}
	return result;
}

// Get name of each item, and return them as a new list.
static std::vector<std::string> craftGetItemNames(
		const std::vector<ItemStack> &items, IGameDef *gamedef)
{
	std::vector<std::string> result;
	result.reserve(items.size());
	for (const auto &item : items) {
		result.push_back(item.name);
	}
	return result;
}

// convert a list of item names, to ItemStacks.
static std::vector<ItemStack> craftGetItems(
		const std::vector<std::string> &items, IGameDef *gamedef)
{
	std::vector<ItemStack> result;
	result.reserve(items.size());
	for (const auto &item : items) {
		result.emplace_back(std::string(item), (u16)1,
			(u16)0, gamedef->getItemDefManager());
	}
	return result;
}

// Compute bounding rectangle given a matrix of items
// Returns false if every item is ""
static bool craftGetBounds(const std::vector<std::string> &items, unsigned int width,
		unsigned int &min_x, unsigned int &max_x,
		unsigned int &min_y, unsigned int &max_y)
{
	bool success = false;
	unsigned int x = 0;
	unsigned int y = 0;
	for (const std::string &item : items) {
		// Is this an actual item?
		if (!item.empty()) {
			if (!success) {
				// This is the first nonempty item
				min_x = max_x = x;
				min_y = max_y = y;
				success = true;
			} else {
				if (x < min_x) min_x = x;
				if (x > max_x) max_x = x;
				if (y < min_y) min_y = y;
				if (y > max_y) max_y = y;
			}
		}

		// Step coordinate
		x++;
		if (x == width) {
			x = 0;
			y++;
		}
	}
	return success;
}

// Removes 1 from each item stack
static void craftDecrementInput(CraftInput &input, IGameDef *gamedef)
{
	for (auto &item : input.items) {
		if (item.count != 0)
			item.remove(1);
	}
}

// Removes 1 from each item stack with replacement support
// Example: if replacements contains the pair ("bucket:bucket_water", "bucket:bucket_empty"),
//   a water bucket will not be removed but replaced by an empty bucket.
static void craftDecrementOrReplaceInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements,
		const CraftReplacements &replacements,
		IGameDef *gamedef)
{
	if (replacements.pairs.empty()) {
		craftDecrementInput(input, gamedef);
		return;
	}

	// Make a copy of the replacements pair list
	std::vector<std::pair<std::string, std::string> > pairs = replacements.pairs;

	for (auto &item : input.items) {
		// Find an appropriate replacement
		bool found_replacement = false;
		for (auto j = pairs.begin(); j != pairs.end(); ++j) {
			if (inputItemMatchesRecipe(item.name, j->first, gamedef->idef())) {
				if (item.count == 1) {
					item.deSerialize(j->second, gamedef->idef());
					found_replacement = true;
					pairs.erase(j);
					break;
				}

				ItemStack rep;
				rep.deSerialize(j->second, gamedef->idef());
				item.remove(1);
				found_replacement = true;
				pairs.erase(j);
				output_replacements.push_back(rep);
				break;

			}
		}
		// No replacement was found, simply decrement count by one
		if (!found_replacement && item.count > 0)
			item.remove(1);
	}
}

// Dump an itemstring matrix
static std::string craftDumpMatrix(const std::vector<std::string> &items,
		unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os << "{ ";
	unsigned int x = 0;
	for(std::vector<std::string>::size_type i = 0;
			i < items.size(); i++, x++) {
		if (x == width) {
			os << "; ";
			x = 0;
		} else if (x != 0) {
			os << ",";
		}
		os << '"' << items[i] << '"';
	}
	os << " }";
	return os.str();
}

// Dump an item matrix
std::string craftDumpMatrix(const std::vector<ItemStack> &items,
		unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os << "{ ";
	unsigned int x = 0;
	for (std::vector<ItemStack>::size_type i = 0;
			i < items.size(); i++, x++) {
		if (x == width) {
			os << "; ";
			x = 0;
		} else if (x != 0) {
			os << ",";
		}
		os << '"' << (items[i].getItemString()) << '"';
	}
	os << " }";
	return os.str();
}


/*
	CraftInput
*/

bool CraftInput::empty() const
{
	for (const auto &item : items) {
		if (!item.empty())
			return false;
	}
	return true;
}

std::string CraftInput::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(method=" << ((int)method) << ", items="
		<< craftDumpMatrix(items, width) << ")";
	return os.str();
}

/*
	CraftOutput
*/

std::string CraftOutput::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(item=\"" << item << "\", time=" << time << ")";
	return os.str();
}

/*
	CraftReplacements
*/

std::string CraftReplacements::dump() const
{
	std::ostringstream os(std::ios::binary);
	os<<"{";
	const char *sep = "";
	for (const auto &repl_p : pairs) {
		os << sep
			<< '"' << (repl_p.first)
			<< "\"=>\"" << (repl_p.second) << '"';
		sep = ",";
	}
	os << "}";
	return os.str();
}

/*
	CraftDefinitionShaped
*/

CraftDefinitionShaped::CraftDefinitionShaped(
		const std::string &output_,
		unsigned int width_,
		const std::vector<std::string> &recipe_,
		const CraftReplacements &replacements_):
	output(output_), width(width_), recipe(recipe_), replacements(replacements_)
{
	if (hasGroupItem(recipe))
		priority = PRIORITY_SHAPED_AND_GROUPS;
	else
		priority = PRIORITY_SHAPED;
}

std::string CraftDefinitionShaped::getName() const
{
	return "shaped";
}

bool CraftDefinitionShaped::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	// Get input item matrix
	std::vector<std::string> inp_names = craftGetItemNames(input.items, gamedef);
	unsigned int inp_width = input.width;
	if (inp_width == 0)
		return false;
	while (inp_names.size() % inp_width != 0)
		inp_names.emplace_back("");

	// Get input bounds
	unsigned int inp_min_x = 0, inp_max_x = 0, inp_min_y = 0, inp_max_y = 0;
	if (!craftGetBounds(inp_names, inp_width, inp_min_x, inp_max_x,
			inp_min_y, inp_max_y))
		return false;  // it was empty

	std::vector<std::string> rec_names;
	if (hash_inited)
		rec_names = recipe_names;
	else
		rec_names = craftGetItemNames(recipe, gamedef);

	// Get recipe item matrix
	unsigned int rec_width = width;
	if (rec_width == 0)
		return false;
	while (rec_names.size() % rec_width != 0)
		rec_names.emplace_back("");

	// Get recipe bounds
	unsigned int rec_min_x=0, rec_max_x=0, rec_min_y=0, rec_max_y=0;
	if (!craftGetBounds(rec_names, rec_width, rec_min_x, rec_max_x,
			rec_min_y, rec_max_y))
		return false;  // it was empty

	// Different sizes?
	if (inp_max_x - inp_min_x != rec_max_x - rec_min_x ||
			inp_max_y - inp_min_y != rec_max_y - rec_min_y)
		return false;

	// Verify that all item names in the bounding box are equal
	unsigned int w = inp_max_x - inp_min_x + 1;
	unsigned int h = inp_max_y - inp_min_y + 1;

	for (unsigned int y=0; y < h; y++) {
		unsigned int inp_y = (inp_min_y + y) * inp_width;
		unsigned int rec_y = (rec_min_y + y) * rec_width;

		for (unsigned int x=0; x < w; x++) {
			unsigned int inp_x = inp_min_x + x;
			unsigned int rec_x = rec_min_x + x;

			if (!inputItemMatchesRecipe(
					inp_names[inp_y + inp_x],
					rec_names[rec_y + rec_x], gamedef->idef())) {
				return false;
			}
		}
	}

	return true;
}

CraftOutput CraftDefinitionShaped::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput(output, 0);
}

CraftInput CraftDefinitionShaped::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	return CraftInput(CRAFT_METHOD_NORMAL,width,craftGetItems(recipe,gamedef));
}

void CraftDefinitionShaped::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	 IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

u64 CraftDefinitionShaped::getHash(CraftHashType type) const
{
	assert(hash_inited); // Pre-condition
	assert((type == CRAFT_HASH_TYPE_ITEM_NAMES)
		|| (type == CRAFT_HASH_TYPE_COUNT)); // Pre-condition

	std::vector<std::string> rec_names = recipe_names;
	std::sort(rec_names.begin(), rec_names.end());
	return getHashForGrid(type, rec_names);
}

void CraftDefinitionShaped::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_names = craftGetItemNames(recipe, gamedef);

	if (hasGroupItem(recipe_names))
		hash_type = CRAFT_HASH_TYPE_COUNT;
	else
		hash_type = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftDefinitionShaped::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(shaped, output=\"" << output
		<< "\", recipe=" << craftDumpMatrix(recipe, width)
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	CraftDefinitionShapeless
*/

CraftDefinitionShapeless::CraftDefinitionShapeless(
		const std::string &output_,
		const std::vector<std::string> &recipe_,
		const CraftReplacements &replacements_):
	output(output_), recipe(recipe_), replacements(replacements_)
{
	if (hasGroupItem(recipe))
		priority = PRIORITY_SHAPELESS_AND_GROUPS;
	else
		priority = PRIORITY_SHAPELESS;
}

std::string CraftDefinitionShapeless::getName() const
{
	return "shapeless";
}

constexpr u16 SHAPELESS_GROUPS_MAX = 30000;

// Checks if there's a matching that matches all nodes in a given bipartite graph.
// bip_graph has graph_size nodes on each side. It is stored as list of lists of
// neighbors from one side.
// See https://en.wikipedia.org/w/index.php?title=Hopcroft-Karp_algorithm for
// details.
static bool hopcroft_karp_can_match_all(const std::vector<std::vector<u16>> &bip_graph)
{
	assert(bip_graph.size() <= SHAPELESS_GROUPS_MAX);
	u16 graph_size = bip_graph.size();
	const u16 nil = graph_size; // nil / dummy index
	constexpr u16 inf = UINT16_MAX; // bigger than any path length (> SHAPELESS_GROUPS_MAX * 2)

	auto pair_u = std::make_unique<u16[]>(graph_size + 1); // for each u (or nil) the matched v (or nil)
	auto pair_v = std::make_unique<u16[]>(graph_size + 1); // for each v (or nil) the matched u (or nil)
	auto dist = std::make_unique<u16[]>(graph_size + 1); // for each u (or nil) the bfs distance
	u16 num_matched;
	std::queue<u16> queue{};

	// calculates distances from unmatched nodes for augmentation paths until
	// dummy is reached
	// returns false if dummy can't be reached (and hence there are no further
	// augmentation paths)
	auto do_bfs = [&]() {
		assert(queue.empty());

		// enqueue all unmatched, give inf dist to the rest
		for (u16 u = 0; u < graph_size; ++u) {
			if (pair_u[u] == nil) {
				dist[u] = 0;
				queue.push(u);
			} else {
				dist[u] = inf;
			}
		}
		dist[nil] = inf;

		while (!queue.empty()) {
			u16 u = queue.front();
			queue.pop();

			if (dist[u] < dist[nil]) { // if dummy not yet reached
				for (u16 v : bip_graph[u]) { // for all adjanced of u
					u16 u_back = pair_v[v];
					// if u_back unvisited, go there
					if (dist[u_back] == inf) {
						dist[u_back] = dist[u] + 1;
						queue.push(u_back);
					}
				}
			}
		}

		return dist[nil] != inf;
	};

	// tries to find an augmenting path from u to the dummy
	// if successful, swaps all edges along path and returns true
	// otherwise returns false
	auto do_dfs_raw = [&](u16 u, auto &&recurse) -> bool {
		if (u == nil) // dummy => dest reached
			return true;

		for (u16 v : bip_graph[u]) { // for all adjanced of u
			u16 u_back = pair_v[v];
			// only walk according to bfs dists
			if (dist[u_back] != dist[u] + 1)
				continue;

			// if walk along u_back reached dummy, swap edges and backtrack
			if (recurse(u_back, recurse)) {
				pair_v[v] = u;
				pair_u[u] = v;
				return true;
			}
		}

		// unsuccessful path, don't walk here again
		dist[u] = inf;
		return false;
	};

	auto do_dfs = [&](u16 u) {
		return do_dfs_raw(u, do_dfs_raw);
	};

	// everyone starts as matched to dummy
	std::fill_n(&pair_u[0], graph_size + 1, nil);
	std::fill_n(&pair_v[0], graph_size + 1, nil);

	num_matched = 0;

	while (do_bfs()) {
		// try to match unmatched u nodes
		for (u16 u = 0; u < graph_size; ++u) {
			if (pair_u[u] == nil) {
				if (do_dfs(u)) {
					num_matched += 1;
				}
			}
		}
	}

	return num_matched == graph_size;
}

bool CraftDefinitionShapeless::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for (const auto &item : input.items) {
		if (!item.name.empty())
			input_filtered.push_back(item.name);
	}

	// If there is a wrong number of items in input, no match
	if (input_filtered.size() != recipe.size()) {
		/*dstream<<"Number of input items ("<<input_filtered.size()
				<<") does not match recipe size ("<<recipe.size()<<") "
				<<"of recipe with output="<<output<<std::endl;*/
		return false;
	}

	// Sort input and recipe
	std::sort(input_filtered.begin(), input_filtered.end());

	std::vector<std::string> recipe_copy;
	if (hash_inited) {
		recipe_copy = recipe_names;
	} else {
		recipe_copy = craftGetItemNames(recipe, gamedef);
		std::sort(recipe_copy.begin(), recipe_copy.end());
	}

	// Split recipe in group and non-group
	std::vector<std::string> recipe_nogroup;
	std::vector<std::string> recipe_onlygroup;
	std::partition_copy(recipe_copy.begin(), recipe_copy.end(),
			std::back_inserter(recipe_onlygroup),
			std::back_inserter(recipe_nogroup),
			[](const std::string &name) { return str_starts_with(name, "group:"); });

	// Filter out non-group recipe slots, using sorted merge.
	// (This prefiltering is only a performance optimization and not strictly
	// necessary.)
	std::vector<std::string> input_for_group;
	std::set_difference(input_filtered.begin(), input_filtered.end(),
			recipe_nogroup.begin(), recipe_nogroup.end(),
			std::back_inserter(input_for_group));

	// All non-group slots must be satisfied
	if (input_filtered.size() - input_for_group.size() != recipe_nogroup.size())
		return false;

	// Find out which recipe slots each input item satisfies. This creates a
	// bipartite graph
	assert(recipe_onlygroup.size() == input_for_group.size());
	if (recipe_onlygroup.size() > SHAPELESS_GROUPS_MAX) {
		// SHAPELESS_GROUPS_MAX is large enough that this should never happen by
		// accident
		errorstream << "Too many groups in shapless craft." << std::endl;
		return false;
	}
	u16 graph_size = recipe_onlygroup.size();
	// bip_graph[i] are the group-slots that item i can satisfy
	std::vector<std::vector<u16>> bip_graph;
	bip_graph.resize(graph_size);
	for (u16 i = 0; i < graph_size; ++i) {
		std::vector<u16> &neighbors_i = bip_graph[i];
		for (u16 j = 0; j < graph_size; ++j) {
			if (inputItemMatchesRecipe(input_for_group[i], recipe_onlygroup[j],
					gamedef->idef()))
				neighbors_i.push_back(j);
		}
	}

	// Check if the maximum cardinality matching of bip_graph matches all items
	return hopcroft_karp_can_match_all(bip_graph);
}

CraftOutput CraftDefinitionShapeless::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput(output, 0);
}

CraftInput CraftDefinitionShapeless::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	return CraftInput(CRAFT_METHOD_NORMAL, 0, craftGetItems(recipe, gamedef));
}

void CraftDefinitionShapeless::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

u64 CraftDefinitionShapeless::getHash(CraftHashType type) const
{
	assert(hash_inited); // Pre-condition
	assert(type == CRAFT_HASH_TYPE_ITEM_NAMES
		|| type == CRAFT_HASH_TYPE_COUNT); // Pre-condition
	return getHashForGrid(type, recipe_names);
}

void CraftDefinitionShapeless::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_names = craftGetItemNames(recipe, gamedef);
	std::sort(recipe_names.begin(), recipe_names.end());

	if (hasGroupItem(recipe_names))
		hash_type = CRAFT_HASH_TYPE_COUNT;
	else
		hash_type = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftDefinitionShapeless::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(shapeless, output=\"" << output
		<< "\", recipe=" << craftDumpMatrix(recipe, recipe.size())
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	CraftDefinitionToolRepair
*/

CraftDefinitionToolRepair::CraftDefinitionToolRepair(float additional_wear_):
	additional_wear(additional_wear_)
{
	priority = PRIORITY_TOOLREPAIR;
}

static ItemStack craftToolRepair(
		const ItemStack &item1,
		const ItemStack &item2,
		float additional_wear,
		IGameDef *gamedef)
{
	IItemDefManager *idef = gamedef->idef();
	if (item1.count != 1 || item2.count != 1 || item1.name != item2.name
			|| idef->get(item1.name).type != ITEM_TOOL
			|| itemgroup_get(idef->get(item1.name).groups, "disable_repair") == 1) {
		// Failure
		return ItemStack();
	}

	s32 item1_uses = 65536 - (u32) item1.wear;
	s32 item2_uses = 65536 - (u32) item2.wear;
	s32 new_uses = item1_uses + item2_uses;
	s32 new_wear = 65536 - new_uses + floor(additional_wear * 65536 + 0.5);
	if (new_wear >= 65536)
		return ItemStack();
	if (new_wear < 0)
		new_wear = 0;

	ItemStack repaired = item1;
	repaired.wear = new_wear;
	return repaired;
}

std::string CraftDefinitionToolRepair::getName() const
{
	return "toolrepair";
}

bool CraftDefinitionToolRepair::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	ItemStack item1;
	ItemStack item2;
	for (const auto &item : input.items) {
		if (!item.empty()) {
			if (item1.empty())
				item1 = item;
			else if (item2.empty())
				item2 = item;
			else
				return false;
		}
	}
	ItemStack repaired = craftToolRepair(item1, item2, additional_wear, gamedef);
	return !repaired.empty();
}

CraftOutput CraftDefinitionToolRepair::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	ItemStack item1;
	ItemStack item2;
	for (const auto &item : input.items) {
		if (!item.empty()) {
			if (item1.empty())
				item1 = item;
			else if (item2.empty())
				item2 = item;
		}
	}
	ItemStack repaired = craftToolRepair(item1, item2, additional_wear, gamedef);
	return CraftOutput(repaired.getItemString(), 0);
}

CraftInput CraftDefinitionToolRepair::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	std::vector<ItemStack> stack;
	stack.emplace_back();
	return CraftInput(CRAFT_METHOD_COOKING, additional_wear, stack);
}

void CraftDefinitionToolRepair::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementInput(input, gamedef);
}

std::string CraftDefinitionToolRepair::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(toolrepair, additional_wear=" << additional_wear << ")";
	return os.str();
}

/*
	CraftDefinitionCooking
*/

CraftDefinitionCooking::CraftDefinitionCooking(
		const std::string &output_,
		const std::string &recipe_,
		float cooktime_,
		const CraftReplacements &replacements_):
	output(output_), recipe(recipe_), cooktime(cooktime_), replacements(replacements_)
{
	if (isGroupRecipeStr(recipe))
		priority = PRIORITY_SHAPELESS_AND_GROUPS;
	else
		priority = PRIORITY_SHAPELESS;
}

std::string CraftDefinitionCooking::getName() const
{
	return "cooking";
}

bool CraftDefinitionCooking::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_COOKING)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for (const auto &item : input.items) {
		const std::string &name = item.name;
		if (!name.empty())
			input_filtered.push_back(name);
	}

	// If there is a wrong number of items in input, no match
	if (input_filtered.size() != 1) {
		/*dstream<<"Number of input items ("<<input_filtered.size()
				<<") does not match recipe size (1) "
				<<"of cooking recipe with output="<<output<<std::endl;*/
		return false;
	}

	// Check the single input item
	std::string rec_name = craftGetItemName(recipe, gamedef);
	return inputItemMatchesRecipe(input_filtered[0], rec_name, gamedef->idef());
}

CraftOutput CraftDefinitionCooking::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput(output, cooktime);
}

CraftInput CraftDefinitionCooking::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	std::vector<std::string> rec;
	rec.push_back(recipe);
	return CraftInput(CRAFT_METHOD_COOKING,cooktime,craftGetItems(rec,gamedef));
}

void CraftDefinitionCooking::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

u64 CraftDefinitionCooking::getHash(CraftHashType type) const
{
	if (type == CRAFT_HASH_TYPE_ITEM_NAMES) {
		return getHashForString(recipe_name);
	}

	if (type == CRAFT_HASH_TYPE_COUNT) {
		return 1;
	}

	// illegal hash type for this CraftDefinition (pre-condition)
	assert(false);
	return 0;
}

void CraftDefinitionCooking::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_name = craftGetItemName(recipe, gamedef);

	if (isGroupRecipeStr(recipe_name))
		hash_type = CRAFT_HASH_TYPE_COUNT;
	else
		hash_type = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftDefinitionCooking::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(cooking, output=\"" << output
		<< "\", recipe=\"" << recipe
		<< "\", cooktime=" << cooktime << ")"
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	CraftDefinitionFuel
*/

CraftDefinitionFuel::CraftDefinitionFuel(
		const std::string &recipe_,
		float burntime_,
		const CraftReplacements &replacements_):
	recipe(recipe_), burntime(burntime_), replacements(replacements_)
{
	if (isGroupRecipeStr(recipe_name))
		priority = PRIORITY_SHAPELESS_AND_GROUPS;
	else
		priority = PRIORITY_SHAPELESS;
}

std::string CraftDefinitionFuel::getName() const
{
	return "fuel";
}

bool CraftDefinitionFuel::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_FUEL)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for (const auto &item : input.items) {
		const std::string &name = item.name;
		if (!name.empty())
			input_filtered.push_back(name);
	}

	// If there is a wrong number of items in input, no match
	if (input_filtered.size() != 1) {
		/*dstream<<"Number of input items ("<<input_filtered.size()
				<<") does not match recipe size (1) "
				<<"of fuel recipe with burntime="<<burntime<<std::endl;*/
		return false;
	}

	// Check the single input item
	std::string rec_name = craftGetItemName(recipe, gamedef);
	return inputItemMatchesRecipe(input_filtered[0], rec_name, gamedef->idef());
}

CraftOutput CraftDefinitionFuel::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput("", burntime);
}

CraftInput CraftDefinitionFuel::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	std::vector<std::string> rec;
	rec.push_back(recipe);
	return CraftInput(CRAFT_METHOD_COOKING,(int)burntime,craftGetItems(rec,gamedef));
}

void CraftDefinitionFuel::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

u64 CraftDefinitionFuel::getHash(CraftHashType type) const
{
	if (type == CRAFT_HASH_TYPE_ITEM_NAMES) {
		return getHashForString(recipe_name);
	}

	if (type == CRAFT_HASH_TYPE_COUNT) {
		return 1;
	}

	// illegal hash type for this CraftDefinition (pre-condition)
	assert(false);
	return 0;
}

void CraftDefinitionFuel::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_name = craftGetItemName(recipe, gamedef);

	if (isGroupRecipeStr(recipe_name))
		hash_type = CRAFT_HASH_TYPE_COUNT;
	else
		hash_type = CRAFT_HASH_TYPE_ITEM_NAMES;
}

std::string CraftDefinitionFuel::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(fuel, recipe=\"" << recipe
		<< "\", burntime=" << burntime << ")"
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	Craft definition manager
*/

class CCraftDefManager: public IWritableCraftDefManager
{
public:
	CCraftDefManager()
	{
		m_craft_defs.resize(craft_hash_type_max + 1);
	}

	virtual ~CCraftDefManager()
	{
		clear();
	}

	virtual bool getCraftResult(CraftInput &input, CraftOutput &output,
			std::vector<ItemStack> &output_replacement, bool decrementInput,
			IGameDef *gamedef) const
	{
		if (input.empty())
			return false;

		std::vector<std::string> input_names;
		input_names = craftGetItemNames(input.items, gamedef);
		std::sort(input_names.begin(), input_names.end());

		// Try hash types with increasing collision rate
		// while remembering the latest, highest priority recipe.
		CraftDefinition::RecipePriority priority_best =
			CraftDefinition::PRIORITY_NO_RECIPE;
		CraftDefinition *def_best = nullptr;
		for (int type = 0; type <= craft_hash_type_max; type++) {
			u64 hash = getHashForGrid((CraftHashType) type, input_names);

			/*errorstream << "Checking type " << type << " with hash " << hash << std::endl;*/

			// We'd like to do "const [...] hash_collisions = m_craft_defs[type][hash];"
			// but that doesn't compile for some reason. This does.
			auto col_iter = (m_craft_defs[type]).find(hash);

			if (col_iter == (m_craft_defs[type]).end())
				continue;

			const std::vector<CraftDefinition*> &hash_collisions = col_iter->second;
			// Walk crafting definitions from back to front, so that later
			// definitions can override earlier ones.
			for (std::vector<CraftDefinition*>::size_type
					i = hash_collisions.size(); i > 0; i--) {
				CraftDefinition *def = hash_collisions[i - 1];

				/*errorstream << "Checking " << input.dump() << std::endl
					<< " against " << def->dump() << std::endl;*/

				CraftDefinition::RecipePriority priority = def->getPriority();
				if (priority > priority_best
						&& def->check(input, gamedef)) {
					// Check if the crafted node/item exists
					CraftOutput out = def->getOutput(input, gamedef);
					ItemStack is;
					is.deSerialize(out.item, gamedef->idef());
					if (!is.isKnown(gamedef->idef())) {
						infostream << "trying to craft non-existent "
							<< out.item << ", ignoring recipe" << std::endl;
						continue;
					}

					output = out;
					priority_best = priority;
					def_best = def;
				}
			}
		}
		if (priority_best == CraftDefinition::PRIORITY_NO_RECIPE)
			return false;
		if (decrementInput)
			def_best->decrementInput(input, output_replacement, gamedef);
		return true;
	}

	virtual std::vector<CraftDefinition*> getCraftRecipes(CraftOutput &output,
			IGameDef *gamedef, unsigned limit=0) const
	{
		std::vector<CraftDefinition*> recipes;

		auto vec_iter = m_output_craft_definitions.find(output.item);

		if (vec_iter == m_output_craft_definitions.end())
			return recipes;

		const std::vector<CraftDefinition*> &vec = vec_iter->second;

		recipes.reserve(limit ? MYMIN(limit, vec.size()) : vec.size());

		for (std::vector<CraftDefinition*>::size_type i = vec.size();
				i > 0; i--) {
			CraftDefinition *def = vec[i - 1];
			if (limit && recipes.size() >= limit)
				break;
			recipes.push_back(def);
		}

		return recipes;
	}

	virtual bool clearCraftsByOutput(const CraftOutput &output, IGameDef *gamedef)
	{
		auto to_clear = m_output_craft_definitions.find(output.item);

		if (to_clear == m_output_craft_definitions.end())
			return false;

		for (auto def : to_clear->second) {
			// Recipes are not yet hashed at this point
			std::vector<CraftDefinition *> &defs = m_craft_defs[(int)CRAFT_HASH_TYPE_UNHASHED][0];
			defs.erase(std::remove(defs.begin(), defs.end(), def), defs.end());
			delete def;
		}
		m_output_craft_definitions.erase(to_clear);
		return true;
	}

	virtual bool clearCraftsByInput(const CraftInput &input, IGameDef *gamedef)
	{
		if (input.empty())
			return false;

		// Recipes are not yet hashed at this point
		std::vector<CraftDefinition *> &defs = m_craft_defs[(int)CRAFT_HASH_TYPE_UNHASHED][0];
		std::unordered_set<const CraftDefinition *> defs_to_remove;
		std::vector<CraftDefinition *> new_defs;

		for (auto def : defs) {
			if (def->check(input, gamedef))
				defs_to_remove.insert(def);
			else
				new_defs.push_back(def);
		}

		if (!defs_to_remove.empty()) {
			for (auto def : defs_to_remove)
				delete def;

			defs.swap(new_defs);

			for (auto &output : m_output_craft_definitions) {
				std::vector<CraftDefinition *> &outdefs = output.second;
				outdefs.erase(std::remove_if(outdefs.begin(), outdefs.end(), [&](const CraftDefinition* def) {
					return defs_to_remove.find(def) != defs_to_remove.end();
				}), outdefs.end());
			}
		}

		return !defs_to_remove.empty();
	}

	virtual std::string dump() const
	{
		std::ostringstream os(std::ios::binary);
		os << "Crafting definitions:\n";
		for (int type = 0; type <= craft_hash_type_max; ++type) {
			for (auto it = m_craft_defs[type].begin();
					it != m_craft_defs[type].end(); ++it) {
				for (std::vector<CraftDefinition*>::size_type i = 0;
						i < it->second.size(); i++) {
					os << "type " << type
						<< " hash " << it->first
						<< " def " << it->second[i]->dump()
						<< "\n";
				}
			}
		}
		return os.str();
	}
	virtual void registerCraft(CraftDefinition *def, IGameDef *gamedef)
	{
		TRACESTREAM(<< "registerCraft: registering craft definition: "
				<< def->dump() << std::endl);
		m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0].push_back(def);

		CraftInput input;
		std::string output_name = craftGetItemName(
				def->getOutput(input, gamedef).item, gamedef);
		m_output_craft_definitions[output_name].push_back(def);
	}
	virtual void clear()
	{
		for (int type = 0; type <= craft_hash_type_max; ++type) {
			for (auto &it : m_craft_defs[type]) {
				for (auto &iit : it.second) {
					delete iit;
				}
				it.second.clear();
			}
			m_craft_defs[type].clear();
		}
		m_output_craft_definitions.clear();
	}
	virtual void initHashes(IGameDef *gamedef)
	{
		// Move the CraftDefs from the unhashed layer into layers higher up.
		std::vector<CraftDefinition *> &unhashed =
			m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0];
		for (auto def : unhashed) {
			// Initialize and get the definition's hash
			def->initHash(gamedef);
			CraftHashType type = def->getHashType();
			u64 hash = def->getHash(type);

			// Enter the definition
			m_craft_defs[type][hash].push_back(def);
		}
		unhashed.clear();
	}
private:
	std::vector<std::unordered_map<u64, std::vector<CraftDefinition*> > >
		m_craft_defs;
	std::unordered_map<std::string, std::vector<CraftDefinition*> >
		m_output_craft_definitions;
};

IWritableCraftDefManager* createCraftDefManager()
{
	return new CCraftDefManager();
}
