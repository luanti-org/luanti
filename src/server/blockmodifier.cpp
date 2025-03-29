// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <algorithm>
#include "blockmodifier.h"
#include "serverenvironment.h"
#include "server.h"
#include "mapblock.h"
#include "nodedef.h"
#include "gamedef.h"

/*
	ABMs
*/

ABMWithState::ABMWithState(ActiveBlockModifier *abm_):
	abm(abm_)
{
	// Initialize timer to random value to spread processing
	float itv = abm->getTriggerInterval();
	itv = MYMAX(0.001f, itv); // No less than 1ms
	int minval = MYMAX(-0.51f * itv, -60); // Clamp to
	int maxval = MYMIN( 0.51f * itv,  60); // +-60 seconds
	timer = myrand_range(minval, maxval);
}

struct ActiveABM
{
	ActiveBlockModifier *abm;
	std::vector<content_t> required_neighbors;
	std::vector<content_t> without_neighbors;
	int chance;
	s16 min_y, max_y;
};

#define CONTENT_TYPE_CACHE_MAX 64

ABMHandler::ABMHandler(std::vector<ABMWithState> &abms,
	float dtime_s, ServerEnvironment *env,
	bool use_timers):
	m_env(env)
{
	if (dtime_s < 0.001f)
		return;
	const NodeDefManager *ndef = env->getGameDef()->ndef();
	for (ABMWithState &abmws : abms) {
		ActiveBlockModifier *abm = abmws.abm;
		float trigger_interval = abm->getTriggerInterval();
		if (trigger_interval < 0.001f)
			trigger_interval = 0.001f;
		float actual_interval = dtime_s;
		if (use_timers) {
			abmws.timer += dtime_s;
			if (abmws.timer < trigger_interval)
				continue;
			abmws.timer -= trigger_interval;
			actual_interval = trigger_interval;
		}
		float chance = abm->getTriggerChance();
		if (chance == 0)
			chance = 1;

		ActiveABM aabm;
		aabm.abm = abm;
		if (abm->getSimpleCatchUp()) {
			float intervals = actual_interval / trigger_interval;
			if (intervals == 0)
				continue;
			aabm.chance = chance / intervals;
			if (aabm.chance == 0)
				aabm.chance = 1;
		} else {
			aabm.chance = chance;
		}
		// y limits
		aabm.min_y = abm->getMinY();
		aabm.max_y = abm->getMaxY();

		// Trigger neighbors
		for (const auto &s : abm->getRequiredNeighbors())
			ndef->getIds(s, aabm.required_neighbors);
		SORT_AND_UNIQUE(aabm.required_neighbors);

		for (const auto &s : abm->getWithoutNeighbors())
			ndef->getIds(s, aabm.without_neighbors);
		SORT_AND_UNIQUE(aabm.without_neighbors);

		// Trigger contents
		std::vector<content_t> ids;
		for (const auto &s : abm->getTriggerContents())
			ndef->getIds(s, ids);
		SORT_AND_UNIQUE(ids);
		for (content_t c : ids) {
			if (c >= m_aabms.size())
				m_aabms.resize(c + 256, nullptr);
			if (!m_aabms[c])
				m_aabms[c] = new std::vector<ActiveABM>;
			m_aabms[c]->push_back(aabm);
		}
	}
}

ABMHandler::~ABMHandler()
{
	for (auto &aabms : m_aabms)
		delete aabms;
}

u32 ABMHandler::countObjects(MapBlock *block, ServerMap *map, u32 &wider)
{
	wider = 0;
	u32 wider_unknown_count = 0;
	for(s16 x=-1; x<=1; x++)
	for(s16 y=-1; y<=1; y++)
	for(s16 z=-1; z<=1; z++)
	{
		MapBlock *block2 = map->getBlockNoCreateNoEx(
			block->getPos() + v3s16(x,y,z));
		if (!block2) {
			wider_unknown_count++;
			continue;
		}
		wider += block2->m_static_objects.size();
	}
	// Extrapolate
	u32 active_object_count = block->m_static_objects.getActiveSize();
	u32 wider_known_count = 3 * 3 * 3 - wider_unknown_count;
	wider += wider_unknown_count * wider / wider_known_count;
	return active_object_count;
}

void ABMHandler::apply(MapBlock *block, int &blocks_scanned, int &abms_run, int &blocks_cached)
{
	if (m_aabms.empty())
		return;

	// Check the content type cache first
	// to see whether there are any ABMs
	// to be run at all for this block.
	if (!block->contents.empty()) {
		assert(!block->do_not_cache_contents); // invariant
		blocks_cached++;
		bool run_abms = false;
		for (content_t c : block->contents) {
			if (c < m_aabms.size() && m_aabms[c]) {
				run_abms = true;
				break;
			}
		}
		if (!run_abms)
			return;
	}
	blocks_scanned++;

	ServerMap *map = &m_env->getServerMap();

	u32 active_object_count_wider;
	u32 active_object_count = countObjects(block, map, active_object_count_wider);
	m_env->m_added_objects = 0;

	bool want_contents_cached = block->contents.empty() && !block->do_not_cache_contents;

	v3s16 p0;
	for(p0.Z=0; p0.Z<MAP_BLOCKSIZE; p0.Z++)
	for(p0.Y=0; p0.Y<MAP_BLOCKSIZE; p0.Y++)
	for(p0.X=0; p0.X<MAP_BLOCKSIZE; p0.X++)
	{
		MapNode n = block->getNodeNoCheck(p0);
		content_t c = n.getContent();

		// Cache content types as we go
		if (want_contents_cached && !CONTAINS(block->contents, c)) {
			if (block->contents.size() >= CONTENT_TYPE_CACHE_MAX) {
				// Too many different nodes... don't try to cache
				want_contents_cached = false;
				block->do_not_cache_contents = true;
				decltype(block->contents) empty;
				std::swap(block->contents, empty);
			} else {
				block->contents.push_back(c);
			}
		}

		if (c >= m_aabms.size() || !m_aabms[c])
			continue;

		v3s16 p = p0 + block->getPosRelative();
		for (ActiveABM &aabm : *m_aabms[c]) {
			if (p.Y < aabm.min_y || p.Y > aabm.max_y)
				continue;

			if (myrand() % aabm.chance != 0)
				continue;

			// Check neighbors
			const bool check_required_neighbors = !aabm.required_neighbors.empty();
			const bool check_without_neighbors = !aabm.without_neighbors.empty();
			if (check_required_neighbors || check_without_neighbors) {
				v3s16 p1;
				bool have_required = false;
				for(p1.X = p0.X-1; p1.X <= p0.X+1; p1.X++)
				for(p1.Y = p0.Y-1; p1.Y <= p0.Y+1; p1.Y++)
				for(p1.Z = p0.Z-1; p1.Z <= p0.Z+1; p1.Z++)
				{
					if (p1 == p0)
						continue;
					content_t c;
					if (block->isValidPosition(p1)) {
						// if the neighbor is found on the same map block
						// get it straight from there
						const MapNode &n = block->getNodeNoCheck(p1);
						c = n.getContent();
					} else {
						// otherwise consult the map
						MapNode n = map->getNode(p1 + block->getPosRelative());
						c = n.getContent();
					}
					if (check_required_neighbors && !have_required) {
						if (CONTAINS(aabm.required_neighbors, c)) {
							if (!check_without_neighbors)
								goto neighbor_found;
							have_required = true;
						}
					}
					if (check_without_neighbors) {
						if (CONTAINS(aabm.without_neighbors, c))
							goto neighbor_invalid;
					}
				}
				if (have_required || !check_required_neighbors)
					goto neighbor_found;
				// No required neighbor found
neighbor_invalid:
				continue;
			}

neighbor_found:

			abms_run++;
			// Call all the trigger variations
			aabm.abm->trigger(m_env, p, n);
			aabm.abm->trigger(m_env, p, n,
				active_object_count, active_object_count_wider);

			if (block->isOrphan())
				return;

			// Count surrounding objects again if the abms added any
			if (m_env->m_added_objects > 0) {
				active_object_count = countObjects(block, map, active_object_count_wider);
				m_env->m_added_objects = 0;
			}

			// Update and check node after possible modification
			n = block->getNodeNoCheck(p0);
			if (n.getContent() != c)
				break;
		}
	}
}

/*
	LBMs
*/

LBMContentMapping::~LBMContentMapping()
{
	map.clear();
	for (auto &it : lbm_list)
		delete it;
}

void LBMContentMapping::addLBM(LoadingBlockModifierDef *lbm_def, IGameDef *gamedef)
{
	// Add the lbm_def to the LBMContentMapping.
	// Unknown names get added to the global NameIdMapping.
	const NodeDefManager *nodedef = gamedef->ndef();

	FATAL_ERROR_IF(CONTAINS(lbm_list, lbm_def), "Same LBM registered twice");
	lbm_list.push_back(lbm_def);

	std::vector<content_t> c_ids;

	for (const auto &node : lbm_def->trigger_contents) {
		bool found = nodedef->getIds(node, c_ids);
		if (!found) {
			content_t c_id = gamedef->allocateUnknownNodeId(node);
			if (c_id == CONTENT_IGNORE) {
				// Seems it can't be allocated.
				warningstream << "Could not internalize node name \"" << node
					<< "\" while loading LBM \"" << lbm_def->name << "\"." << std::endl;
				continue;
			}
			c_ids.push_back(c_id);
		}
	}

	SORT_AND_UNIQUE(c_ids);

	for (content_t c_id : c_ids)
		map[c_id].push_back(lbm_def);
}

const LBMContentMapping::lbm_vector *
LBMContentMapping::lookup(content_t c) const
{
	lbm_map::const_iterator it = map.find(c);
	if (it == map.end())
		return nullptr;
	return &(it->second);
}

LBMManager::~LBMManager()
{
	for (auto &m_lbm_def : m_lbm_defs)
		delete m_lbm_def.second;

	m_lbm_lookup.clear();
}

void LBMManager::addLBMDef(LoadingBlockModifierDef *lbm_def)
{
	// Precondition, in query mode the map isn't used anymore
	FATAL_ERROR_IF(m_query_mode,
		"attempted to modify LBMManager in query mode");

	if (str_starts_with(lbm_def->name, ":"))
		lbm_def->name.erase(0, 1);

	if (lbm_def->name.empty() ||
		!string_allowed(lbm_def->name, LBM_NAME_ALLOWED_CHARS)) {
		throw ModError("Error adding LBM \"" + lbm_def->name +
			"\": Does not follow naming conventions: "
				"Only characters [a-z0-9_:] are allowed.");
	}

	m_lbm_defs[lbm_def->name] = lbm_def;
}

void LBMManager::loadIntroductionTimes(const std::string &times,
	IGameDef *gamedef, u32 now)
{
	m_query_mode = true;

	auto introduction_times = parseIntroductionTimesString(times);

	// Put stuff from introduction_times into m_lbm_lookup
	for (auto &[name, time] : introduction_times) {
		auto def_it = m_lbm_defs.find(name);
		if (def_it == m_lbm_defs.end()) {
			infostream << "LBMManager: LBM " << name << " is not registered. "
				"Discarding it." << std::endl;
			continue;
		}
		auto *lbm_def = def_it->second;
		if (lbm_def->run_at_every_load) {
			continue; // These are handled below
		}
		if (time > now) {
			warningstream << "LBMManager: LBM " << name << " was introduced in "
				"the future. Pretending it's new." << std::endl;
			// By skipping here it will be added as newly introduced.
			continue;
		}

		m_lbm_lookup[time].addLBM(lbm_def, gamedef);

		// Erase the entry so that we know later
		// which elements didn't get put into m_lbm_lookup
		m_lbm_defs.erase(def_it);
	}

	// Now also add the elements from m_lbm_defs to m_lbm_lookup
	// that weren't added in the previous step.
	// They are introduced first time to this world,
	// or are run at every load (introduction time hardcoded to U32_MAX).

	auto &lbms_we_introduce_now = m_lbm_lookup[now];
	auto &lbms_running_always = m_lbm_lookup[U32_MAX];
	for (auto &it : m_lbm_defs) {
		if (it.second->run_at_every_load)
			lbms_running_always.addLBM(it.second, gamedef);
		else
			lbms_we_introduce_now.addLBM(it.second, gamedef);
	}

	// All pointer ownership now moved to LBMContentMapping
	m_lbm_defs.clear();

	// If these are empty delete them again to avoid pointless iteration.
	if (lbms_we_introduce_now.empty())
		m_lbm_lookup.erase(now);
	if (lbms_running_always.empty())
		m_lbm_lookup.erase(U32_MAX);

	infostream << "LBMManager: " << m_lbm_lookup.size() <<
		" unique times in lookup table" << std::endl;
}

std::string LBMManager::createIntroductionTimesString()
{
	// Precondition, we must be in query mode
	FATAL_ERROR_IF(!m_query_mode,
		"attempted to query on non fully set up LBMManager");

	std::ostringstream oss;
	for (const auto &it : m_lbm_lookup) {
		u32 time = it.first;
		auto &lbm_list = it.second.getList();
		for (const auto &lbm_def : lbm_list) {
			// Don't add if the LBM runs at every load,
			// then introduction time is hardcoded and doesn't need to be stored.
			if (lbm_def->run_at_every_load)
				continue;
			oss << lbm_def->name << "~" << time << ";";
		}
	}
	return oss.str();
}

std::unordered_map<std::string, u32>
	LBMManager::parseIntroductionTimesString(const std::string &times)
{
	std::unordered_map<std::string, u32> ret;

	size_t idx = 0;
	size_t idx_new;
	while ((idx_new = times.find(';', idx)) != std::string::npos) {
		std::string entry = times.substr(idx, idx_new - idx);
		idx = idx_new + 1;

		std::vector<std::string> components = str_split(entry, '~');
		if (components.size() != 2)
			throw SerializationError("Introduction times entry \""
				+ entry + "\" requires exactly one '~'!");
		if (components[0].empty())
			throw SerializationError("LBM name is empty");
		std::string name = std::move(components[0]);
		if (name.front() == ':') // old versions didn't strip this
			name.erase(0, 1);
		u32 time = from_string<u32>(components[1]);
		ret[std::move(name)] = time;
	}

	return ret;
}

namespace {
	struct LBMToRun {
		std::unordered_set<v3s16> p; // node positions
		std::vector<LoadingBlockModifierDef*> l; // ordered list of LBMs

		template <typename C>
		void insertLBMs(const C &container) {
			for (auto &it : container) {
				if (!CONTAINS(l, it))
					l.push_back(it);
			}
		}
	};
}

void LBMManager::applyLBMs(ServerEnvironment *env, MapBlock *block,
		const u32 stamp, const float dtime_s)
{
	// Precondition, we need m_lbm_lookup to be initialized
	FATAL_ERROR_IF(!m_query_mode,
		"attempted to query on non fully set up LBMManager");

	// Collect a list of all LBMs and associated positions
	std::unordered_map<content_t, LBMToRun> to_run;

	// Note: the iteration count of this outer loop is typically very low, so it's ok.
	for (auto it = getLBMsIntroducedAfter(stamp); it != m_lbm_lookup.end(); ++it) {
		v3s16 pos;
		content_t c;

		// Cache previous lookups since it has a high performance penalty.
		content_t previous_c = CONTENT_IGNORE;
		const LBMContentMapping::lbm_vector *lbm_list = nullptr;
		LBMToRun *batch = nullptr;

		for (pos.Z = 0; pos.Z < MAP_BLOCKSIZE; pos.Z++)
		for (pos.Y = 0; pos.Y < MAP_BLOCKSIZE; pos.Y++)
		for (pos.X = 0; pos.X < MAP_BLOCKSIZE; pos.X++) {
			c = block->getNodeNoCheck(pos).getContent();

			bool c_changed = false;
			if (previous_c != c) {
				c_changed = true;
				lbm_list = it->second.lookup(c);
				if (lbm_list)
					batch = &to_run[c]; // creates entry
				previous_c = c;
			}

			if (!lbm_list)
				continue;
			batch->p.insert(pos);
			if (c_changed) {
				batch->insertLBMs(*lbm_list);
			} else {
				// we were here before so the list must be filled
				assert(!batch->l.empty());
			}
		}
	}

	// Actually run them
	bool first = true;
	for (auto &[c, batch] : to_run) {
		if (tracestream) {
			tracestream << "Running " << batch.l.size() << " LBMs for node "
				<< env->getGameDef()->ndef()->get(c).name << " ("
				<< batch.p.size() << "x) in block " << block->getPos() << std::endl;
		}
		for (auto &lbm_def : batch.l) {
			if (!first) {
				// The fun part: since any LBM call can change the nodes inside of he
				// block, we have to recheck the positions to see if the wanted node
				// is still there.
				// Note that we don't rescan the whole block, we don't want to include new changes.
				for (auto it2 = batch.p.begin(); it2 != batch.p.end(); ) {
					if (block->getNodeNoCheck(*it2).getContent() != c)
						it2 = batch.p.erase(it2);
					else
						++it2;
				}
			} else {
				assert(!batch.p.empty());
			}
			first = false;

			if (batch.p.empty())
				break;
			lbm_def->trigger(env, block, batch.p, dtime_s);
			if (block->isOrphan())
				return;
		}
	}
}
