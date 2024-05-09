// Copyright (C) 2024 Lars Müller
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>

using Idx = uint16_t;

// TODO docs and explanation

// TODO profile and tweak knobs

// TODO cleanup (split up in header and impl among other things)

template<uint8_t Dim, typename Component>
class Points {
public:
	using Point = std::array<Component, Dim>;
	//! Empty
	Points() : n(0), coords(nullptr) {}
	//! Leaves coords uninitialized!
	// TODO we want make_unique_for_overwrite here...
	Points(Idx n) : n(n), coords(std::make_unique<Component[]>(Dim * n)) {}
	//! Copying constructor
	Points(Idx n, const std::array<Component const *, Dim> &coords) : Points(n) {
		for (uint8_t d = 0; d < Dim; ++d)
			std::copy(coords[d], coords[d] + n, begin(d));
	}
	Idx size() const {
		return n;
	}
	void assign(Idx start, const Points &from) {
		for (uint8_t d = 0; d < Dim; ++d)
			std::copy(from.begin(d), from.end(d), begin(d) + start);
	}
	Point getPoint(Idx i) const {
		Point point;
		for (uint8_t d = 0; d < Dim; ++d)
			point[d] = begin(d)[i];
		return point;
	}
	void setPoint(Idx i, const Point &point) {
		for (uint8_t d = 0; d < Dim; ++d)
			begin(d)[i] = point[d];
	}
	// HACK interior mutability...
	Component *begin(uint8_t d) const {
	 	return coords.get() + d * n;
	}
	Component *end(uint8_t d) const {
	 	return begin(d) + n;
	}
private:
	Idx n;
	std::unique_ptr<Component[]> coords;
};

template<uint8_t Dim>
class SortedIndices {
public:
	//! empty
	SortedIndices() : indices() {}

	//! uninitialized indices
	static SortedIndices newUninitialized(Idx n) {
		return SortedIndices(n); // HACK can't be arsed to fix rn Points<Dim, Idx>(n));
	}

	// Identity permutation on all axes
	SortedIndices(Idx n) : indices(n) {
		for (uint8_t d = 0; d < Dim; ++d)
			for (Idx i = 0; i < n; ++i)
				indices.begin(d)[i] = i;
	}

	Idx size() const {
		return indices.size();
	}

	bool empty() const {
		return size() == 0;
	}

	struct SplitResult {
		SortedIndices left, right;
		Idx pivot;
	};

	//! Splits the sorted indices in the middle along the specified axis,
	//! partitioning them into left (<=), the pivot, and right (>=).
	SplitResult split(uint8_t axis, std::vector<bool> &markers) const {
		const auto begin = indices.begin(axis);
		Idx left_n = indices.size() / 2;
		const auto mid = begin + left_n;

		// Mark all points to be partitioned left
		for (auto it = begin; it != mid; ++it)
			markers[*it] = true;

		SortedIndices left(left_n);
		std::copy(begin, mid, left.indices.begin(axis));
		SortedIndices right(indices.size() - left_n - 1);
		std::copy(mid + 1, indices.end(axis), right.indices.begin(axis));

		for (uint8_t d = 0; d < Dim; ++d) {
			if (d == axis)
				continue;
			auto left_ptr = left.indices.begin(d);
			auto right_ptr = right.indices.begin(d);
			for (auto it = indices.begin(d); it != indices.end(d); ++it) {
				if (*it != *mid) { // ignore pivot
					if (markers[*it])
						*(left_ptr++) = *it;
					else
						*(right_ptr++) = *it;
				}
			}
		}

		// Unmark points, since we want to reuse the storage for markers
		for (auto it = begin; it != mid; ++it)
			markers[*it] = false;

		return SplitResult{std::move(left), std::move(right), *mid};
	}

	Idx *begin(uint8_t d) const {
		return indices.begin(d);
	}

	Idx *end(uint8_t d) const {
		return indices.end(d);
	}
private:
	Points<Dim, Idx> indices;
};

template<uint8_t Dim, class Component>
class SortedPoints {
public:
	SortedPoints() : points(), indices() {}

	//! Single point
	// TODO remove this
	SortedPoints(const std::array<Component, Dim> &point) : points(1), indices(1) {
		points.setPoint(0, point);
	}

	//! Sort points
	SortedPoints(Idx n, const std::array<Component const *, Dim> ptrs)
		: points(n, ptrs), indices(n)
	{
		for (uint8_t d = 0; d < Dim; ++d) {
			const auto coord = points.begin(d);
			std::sort(indices.begin(d), indices.end(d), [&](auto i, auto j) {
				return coord[i] < coord[j];
			});
		}
	}

	//! Merge two sets of sorted points
	SortedPoints(const SortedPoints &a, const SortedPoints &b)
		: points(a.size() + b.size())
	{
		const auto n = points.size();
		indices = SortedIndices<Dim>::newUninitialized(n);
		for (uint8_t d = 0; d < Dim; ++d) {
			points.assign(0, a.points);
			points.assign(a.points.size(), b.points);
			const auto coord = points.begin(d);
			auto a_ptr = a.indices.begin(d);
			auto b_ptr = b.indices.begin(d);
			auto dst_ptr = indices.begin(d);
			while (a_ptr != a.indices.end(d) && b_ptr != b.indices.end(d)) {
				const auto i = *a_ptr;
				const auto j = *b_ptr + a.size();
				if (coord[i] <= coord[j]) {
					*(dst_ptr++) = i;
					++a_ptr;
				} else {
					*(dst_ptr++) = j;
					++b_ptr;
				}
			}
			while (a_ptr != a.indices.end(d))
				*(dst_ptr++) = *(a_ptr++);
			while (b_ptr != b.indices.end(d))
				*(dst_ptr++) = a.size() + *(b_ptr++);
		}
	}

	Idx size() const {
		// technically redundant with indices.size(),
		// but that is irrelevant
		return points.size();
	}
	
// HACK private:
	Points<Dim, Component> points;
	SortedIndices<Dim> indices;
};

template<uint8_t Dim, class Component, class Id>
class KdTree {
public:
	using Point = std::array<Component, Dim>;

	//! Empty tree
	KdTree()
		: items()
		, ids(nullptr)
		, tree(nullptr)
		, deleted()
	{}

	//! Build a tree containing just a single point
	// TODO this will probably be obsolete soon (TM)
	KdTree(const Point &point, const Id &id)
		: items(point)
		, ids(std::make_unique<Id[]>(1))
		, tree(std::make_unique<Idx[]>(1))
		, deleted(1)
	{
		tree[0] = 0;
		ids[0] = id;
	}

	//! Build a tree
	KdTree(Idx n, Id const *ids, std::array<Component const *, Dim> pts)
		: items(n, pts)
		, ids(std::make_unique<Id[]>(n))
		, tree(std::make_unique<Idx[]>(n))
		, deleted(n)
	{
		std::copy(ids, ids + n, this->ids.get());
		init(0, 0, items.indices);
	}

	//! Merge two trees. Both trees are assumed to have a power of two size.
	KdTree(const KdTree &a, const KdTree &b)
		: items(a.items, b.items)
	{
		tree = std::make_unique<Idx[]>(cap());
		ids = std::make_unique<Id[]>(cap());
		std::copy(a.ids.get(), a.ids.get() + a.cap(), ids.get());
		std::copy(b.ids.get(), b.ids.get() + b.cap(), ids.get() + a.cap());
		deleted = std::vector<bool>(cap());
		init(0, 0, items.indices); // this does le dirty dirty hack so call it BEFORE we deal with deleted
		std::copy(a.deleted.begin(), a.deleted.end(), deleted.begin());
		std::copy(b.deleted.begin(), b.deleted.end(), deleted.begin() + a.items.size());
	}

	// TODO ray proximity query

	template<typename F>
	void rangeQuery(const Point &min, const Point &max,
			const F &cb) const {
		if (empty())
			return;
		rangeQuery(0, 0, min, max, cb);
	}

	void remove(Idx internalIdx) {
		assert(!deleted[internalIdx]);
		deleted[internalIdx] = true;
	}

	template<class F>
	void foreach(F cb) const {
		for (Idx i = 0; i < cap(); ++i)
			if (!deleted[i])
				cb(i, items.points.getPoint(i), ids[i]);
	}

	//! Capacity, not size, since some items may be marked as deleted
	Idx cap() const {
		return items.size();
	}

	//! "Empty" as in "never had anything"
	bool empty() const {
		return cap() == 0;
	}

private:
	void init(Idx root, uint8_t axis, const SortedIndices<Dim> &sorted) {
		// HACK abuse deleted marks as left/right marks
		const auto split = sorted.split(axis, deleted);
		tree[root] = split.pivot;
		const auto next_axis = (axis + 1) % Dim;
		if (!split.left.empty())
			init(2 * root + 1, next_axis, split.left);
		if (!split.right.empty())
			init(2 * root + 2, next_axis, split.right);
	}

	template<typename F>
	void rangeQuery(Idx root, uint8_t split,
			const Point &min, const Point &max,
			const F &cb) const {
		if (root >= cap()) // TODO what if we overflow earlier?
			return;
		const auto ptid = tree[root];
		const auto coord = items.points.begin(split)[ptid];
		const auto leftChild = 2*root + 1;
		const auto rightChild = 2*root + 2;
		const auto nextSplit = (split + 1) % Dim;
		if (min[split] > coord) {
			rangeQuery(rightChild, nextSplit, min, max, cb);
		} else if (max[split] < coord) {
			rangeQuery(leftChild, nextSplit, min, max, cb);
		} else {
			rangeQuery(rightChild, nextSplit, min, max, cb);
			rangeQuery(leftChild, nextSplit, min, max, cb);
			if (deleted[ptid])
				return;
			const auto point = items.points.getPoint(ptid);
			for (uint8_t d = 0; d < Dim; ++d)
				if (point[d] < min[d] || point[d] > max[d])
					return;
			cb(items.points.getPoint(ptid), ids[ptid]);
		}
	}
	SortedPoints<Dim, Component> items;
	std::unique_ptr<Id[]> ids;
	std::unique_ptr<Idx[]> tree;
	// vector because this has the template specialization we want
	// and i'm too lazy to implement bitsets myself right now
	// just to shave off 16 redundant bytes (len + cap)
	std::vector<bool> deleted;
};

// TODO abstract dynamic spatial index superclass
template<uint8_t Dim, class Component, class Id>
class DynamicKdTrees {
	using Tree = KdTree<Dim, Component, Id>;
public:
	using Point = typename Tree::Point;
	void insert(const std::array<Component, Dim> &point, const Id id) {
		Tree tree(point, id);
		for (uint8_t tree_idx = 0;; ++tree_idx) {
			if (tree_idx >= trees.size()) {
				tree.foreach([&](Idx in_tree_idx, auto _, Id id) {
					del_entries[id] = {tree_idx, in_tree_idx};
				});
				trees.push_back(std::move(tree));
				break;
			}
			if (trees[tree_idx].empty()) {
				// TODO deduplicate
				tree.foreach([&](Idx in_tree_idx, auto _, Id id) {
					del_entries[id] = {tree_idx, in_tree_idx};
				});
				trees[tree_idx] = std::move(tree);
				break;
			}
			tree = Tree(tree, trees[tree_idx]);
			trees[tree_idx] = std::move(Tree());
		}
		++n_entries;
	}
	void remove(Id id) {
		const auto del_entry = del_entries.at(id);
		trees.at(del_entry.treeIdx).remove(del_entry.inTree);
		del_entries.erase(id); // TODO use iterator right away...
		++deleted;
		if (deleted > n_entries/2) // we want to shift out the one!
			compactify();
	}
	void update(const Point &newPos, Id id) {
		remove(id);
		insert(newPos, id);
	}
	template<typename F>
	void rangeQuery(const Point &min, const Point &max,
			const F &cb) const {
		for (const auto &tree : trees)
			tree.rangeQuery(min, max, cb);
	}
private:
	void compactify() {
		assert(n_entries >= deleted);
		n_entries -= deleted; // note: this should be exactly n_entries/2
		deleted = 0;
		// reset map, freeing memory (instead of clearing)
		del_entries = std::unordered_map<Id, DelEntry>();


		// Collect all live points and corresponding IDs.
		const auto live_ids = std::make_unique<Id[]>(n_entries);
		Points<Dim, Component> live_points(n_entries);
		Idx i = 0;
		for (const auto &tree : trees) {
			tree.foreach([&](Idx _, auto point, Id id) {
				assert(i < n_entries);
				live_points.setPoint(i, point);
				live_ids[i] = id;
				++i;
			});
		}
		assert(i == n_entries);

		// Construct a new forest.
		// The "tree pattern" will effectively just be shifted down by one.
		auto id_ptr = live_ids.get();
		std::array<Component const *, Dim> point_ptrs;
		Idx n = 1;
		for (uint8_t d = 0; d < Dim; ++d)
			point_ptrs[d] = live_points.begin(d);
		for (uint8_t treeIdx = 0; treeIdx < trees.size() - 1; ++treeIdx, n *= 2) {
			Tree tree;
			if (!trees[treeIdx+1].empty()) {
				// TODO maybe optimize from log² -> log?
				// This could be achieved by doing a sorted merge of live points, then doing a radix sort.
				tree = std::move(Tree(n, id_ptr, point_ptrs));
				id_ptr += n;
				for (uint8_t d = 0; d < Dim; ++d)
					point_ptrs[d] += n;
				// TODO dedupe
				tree.foreach([&](Idx objIdx, auto _, Id id) {
					del_entries[id] = {treeIdx, objIdx};
				});
			}
			trees[treeIdx] = std::move(tree);
		}
		trees.pop_back(); // "shift out" tree with the most elements
	}
	// could use an array (rather than a vector) here since we've got a good bound on the size ahead of time but meh
	std::vector<Tree> trees;
	struct DelEntry {
		uint8_t treeIdx;
		Idx inTree;
	};
	std::unordered_map<Id, DelEntry> del_entries;
	Idx n_entries = 0;
	Idx deleted = 0;
};