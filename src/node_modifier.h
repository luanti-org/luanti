#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "config.h"
#include "constants.h"
#include "irrlichttypes.h"
#include "mapnode.h"
#include "nameidmapping.h"
#include "tiledef.h"

class IdIdMapping;

#if CHECK_CLIENT_BUILD()
class NodeModifierVisuals;
struct NodeVisuals;
class IShaderSource;
struct TileSpec;
enum WorldAlignMode : u8;
#endif

static constexpr u16 MODIFIER_IGNORE = UINT16_MAX;

static constexpr u16 MAX_REGISTERED_NODE_MODIFIERS = UINT16_MAX - UINT16_MAX / 10;

enum ModifiedProperty : u64 {
	MP_LightSource = u64{1} << 0,
	// Six independent face properties, in the same order as ContentFeatures::tiledef.
	MP_TileFirst = u64{1} << 1,
	MP_Tiles = MP_TileFirst * 0x3f,
	MP_Alpha = MP_TileFirst << 6,
};

struct NodeModifier {
	NodeModifier(const std::string &name, bool is_dummy = false);
	~NodeModifier();
	NodeModifier(NodeModifier &&) noexcept;
	NodeModifier &operator=(NodeModifier &&) noexcept;

	std::string m_name;
	bool m_is_dummy;

	u8 m_light_source = 0;
	AlphaMode m_alpha = ALPHAMODE_OPAQUE;

	bool hasTiles() const { return (m_modified_props_mask & MP_Tiles) != 0; }
	const std::array<TileDef, 6> *getTiles() const { return m_tiles.get(); }
	void setTiles(std::array<TileDef, 6> tiles);

	u64 m_modified_props_mask = 0;

	void serialize(std::ostream &os, u16 protocol_version) const;
	void deSerialize(std::istream &is, u16 protocol_version);

#if CHECK_CLIENT_BUILD()
	std::unique_ptr<NodeModifierVisuals> visuals;
#endif

private:
	std::unique_ptr<const std::array<TileDef, 6>> m_tiles;
};

class NodeModifierManager {
public:
	NodeModifierManager() {};

	u16 add(NodeModifier &&nodeModifier);
	u16 getId(const std::string &name) const;
	const NodeModifier& get(u16 id) const;

	inline u16 size() const {
		return m_modifiers.size();
	}

	void serialize(std::ostream &os, u16 protocol_version) const;
	void deSerialize(std::istream &is, u16 protocol_version);

	void applyFunction(const std::function<void(NodeModifier &)> &function);

private:
	NameIdMapping m_name_id_mapping;
	std::vector<NodeModifier> m_modifiers;
	const inline static NodeModifier dummy{"<unknown>", true};
};

NodeModifierManager *createNodeModifierManager();

struct AppliedNodeModifier {
	v3s16 pos;
	u16 id;
};

// Non-owning view of the modifiers at one position. Invalidated when the
// underlying storage is modified; it must not outlive that storage.
class AppliedNodeModifiersView {
public:
	using ConstIterator = std::vector<AppliedNodeModifier>::const_iterator;

	AppliedNodeModifiersView(ConstIterator first, ConstIterator last) :
			m_begin(first), m_end(last) {}

	ConstIterator begin() const { return m_begin; }
	ConstIterator end() const { return m_end; }
	bool empty() const { return m_begin == m_end; }
	size_t size() const { return m_end - m_begin; }

	ContentLightingFlags resolveLighting(const NodeModifierManager &manager,
			ContentLightingFlags base = {}) const;

#if CHECK_CLIENT_BUILD()
	// Applies the last texture override for this face and the last alpha override
	// independently, preserving properties supplied by the original node visuals.
	void resolveTile(const NodeModifierManager &manager, const NodeVisuals &visuals,
			u8 face, TileSpec &tile, WorldAlignMode world_align,
			const IShaderSource *shaders) const;
#endif

private:
	ConstIterator m_begin;
	ConstIterator m_end;
};

class AppliedNodeModifiersStorage;

// Sorted storage shared by block leaves and mesh snapshots. Coordinates are not
// restricted to a block; per-node edits enforce the modifier limit, append does not.
class AppliedNodeModifiersVector {
public:
	using ConstIterator = std::vector<AppliedNodeModifier>::const_iterator;

	AppliedNodeModifiersVector() = default;
	explicit AppliedNodeModifiersVector(std::vector<AppliedNodeModifier> entries) :
			m_entries(std::move(entries)) {}
	AppliedNodeModifiersVector(ConstIterator first, ConstIterator last) : m_entries(first, last) {}

	bool empty() const { return m_entries.empty(); }
	void shrink_to_fit() { m_entries.shrink_to_fit(); }
	size_t size() const { return m_entries.size(); }
	ConstIterator begin() const { return m_entries.begin(); }
	ConstIterator end() const { return m_entries.end(); }
	AppliedNodeModifiersView getViewAtPos(v3s16 pos) const;

	// Snapshot-specific
	void append(const AppliedNodeModifiersStorage &other, v3s16 offset);

	// Leaf-specific
	bool add(v3s16 pos, u16 id, const NodeModifierManager *prune_manager);
	bool set(v3s16 pos, const std::vector<u16> &ids);
	bool remove(v3s16 pos, u16 id);
	bool remove(v3s16 pos);
	void remapIds(const std::function<u16(u16)> &map_id);

private:
	template <typename Entries>
	static auto findRangeInEntries(Entries &entries, v3s16 pos);
	std::vector<AppliedNodeModifier> m_entries;

	bool addWithPrune(v3s16 pos, u16 id, const NodeModifierManager *prune_manager);
};

/* Storage of applied node modifiers specific to a separate mapblock.
 * Node modifiers are stored as a tree of vectors. Each tree node points
 * to 16 child elements. When the number of modifiers in one vector exceeds
 * LEAF_TARGET, it's transformed into a node, and the data about modifiers
 * are moved to its child vectors. */
class AppliedNodeModifiersStorage {
	using Leaf = AppliedNodeModifiersVector;

	// Corresponds to one element of the tree (either tree node or leaf)
	struct Bucket {
		using TreeNode = std::unique_ptr<std::array<Bucket, MAP_BLOCKSIZE>>;

		std::variant<Leaf, TreeNode> data;

		u8 coordinate(v3s16 pos, u8 depth) const {
			u8 coord = depth == 0 ? pos.X : depth == 1 ? pos.Y : pos.Z;
			assert(depth < 3 && coord >= 0 && coord < MAP_BLOCKSIZE);
			return coord;
		}

		Bucket *try_get_children() {
			auto *tree = std::get_if<TreeNode>(&data);
			return tree ? (*tree)->data() : nullptr;
		}
		const Bucket *try_get_children() const {
			const auto *tree = std::get_if<TreeNode>(&data);
			return tree ? (*tree)->data() : nullptr;
		}
		Leaf &get_leaf() { return std::get<Leaf>(data); }
		const Leaf &get_leaf() const { return std::get<Leaf>(data); }

		Bucket() = default;
		Bucket(Bucket &&) noexcept = default;
		Bucket &operator=(Bucket &&) noexcept = default;

		// Split only overflowing leaves; deletion never coalesces branches.
		void split(u8 depth);
		void assignRange(Leaf::ConstIterator first, Leaf::ConstIterator last, u8 depth);
		const Bucket &findLeaf(v3s16 pos, u8 &depth) const;

		bool add(v3s16 pos, u16 id, size_t &total_size,
				const NodeModifierManager *prune_manager, u8 depth = 0);
		bool set(v3s16 pos, const std::vector<u16> &ids, size_t &total_size, u8 depth = 0);
		bool remove(v3s16 pos, u16 id, size_t &total_size, u8 depth = 0);
		bool remove(v3s16 pos, size_t &total_size, u8 depth = 0);

		template <typename F>
		void forEachLeaf(F &&callback) const {
			if (const Bucket *branch = try_get_children()) {
				for (size_t i = 0; i < MAP_BLOCKSIZE; ++i)
					branch[i].forEachLeaf(callback);
			} else {
				callback(get_leaf());
			}
		}
		template <typename F>
		void forEachLeaf(F &&callback) {
			if (auto *branch = try_get_children()) {
				for (size_t i = 0; i < MAP_BLOCKSIZE; ++i)
					branch[i].forEachLeaf(callback);
			} else {
				callback(get_leaf());
			}
		}
	};

public:
	AppliedNodeModifiersStorage() = default;
	AppliedNodeModifiersStorage(const AppliedNodeModifiersStorage &) = delete;
	void clear() noexcept {
		m_root.data.emplace<Leaf>();
		m_size = 0;
	}

	using ConstIterator = Leaf::ConstIterator;
	AppliedNodeModifiersView getViewAtPos(v3s16 pos) const;

	size_t size() const { return m_size; }
	bool empty() const { return m_size == 0; }

	// Visit entries in X/Y/Z order
	template <typename F>
	void forEach(F &&callback) const {
		m_root.forEachLeaf([&](const Leaf &leaf) {
			for (const auto &entry : leaf)
				callback(entry);
		});
	}

	// Only block-local coordinates (0..15 on each axis) can be stored.
	// Supplying a manager opts into removing fully overridden modifiers.
	bool add(v3s16 pos, u16 id, const NodeModifierManager *prune_manager = nullptr);
	bool set(v3s16 pos, const std::vector<u16> &ids);
	bool remove(v3s16 pos, u16 id);
	bool remove(v3s16 pos);

	// Disk entries must already use the local IDs described by nimap.
	void serialize(std::ostream &os, u16 version, bool disk, const NameIdMapping &nimap,
			const IdIdMapping *id_mapping = nullptr) const;
	// Disk entries retain local IDs; nimap receives their names.
	void deSerialize(std::istream &is, u16 version, bool disk, NameIdMapping &nimap);
	void remapIds(const std::function<u16(u16)> &map_id);

private:
	// Split above this entry count; deletion does not merge branches.
	static constexpr size_t LEAF_TARGET = 128;
	Bucket m_root;
	size_t m_size = 0;
};

// Flat, independently owned data for mesh workers. Positions are relative to the
// mesh origin and may lie outside a single block. Build by appending block lists;
// appending invalidates ranges obtained from this snapshot.
class AppliedNodeModifiersSnapshot {
public:
	using ConstIterator = AppliedNodeModifiersVector::ConstIterator;

	void append(const AppliedNodeModifiersStorage &other, v3s16 offset);
	size_t size() const { return m_modifiers.size(); }
	bool empty() const { return m_modifiers.empty(); }
	void shrink_to_fit() { m_modifiers.shrink_to_fit(); }
	AppliedNodeModifiersView getViewAtPos(v3s16 pos) const;

private:
	AppliedNodeModifiersVector m_modifiers;
};
