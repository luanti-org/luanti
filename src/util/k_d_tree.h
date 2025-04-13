// Copyright (C) 2024 Lars Müller
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>

/*
This implements a dynamic forest of static k-d-trees.

A k-d-tree is a k-dimensional binary search tree.
On the i-th level of the tree, you split by the (i mod k)-th coordinate.

Building a balanced k-d-tree for n points is done in O(n log n) time:
Points are stored in a matrix, identified by indices.
These indices are presorted by all k axes.
To split, you simply pick the pivot index in the appropriate index array,
and mark all points left to it by index in a bitset.
This lets you then split the indices sorted by the other axes,
while preserving the sorted order.

This however only gives us a static spatial index.
To make it dynamic, we keep a "forest" of k-d-trees of sizes of successive powers of two.
When we insert a new tree, we check whether there already is a k-d-tree of the same size.
If this is the case, we merge with that tree, giving us a tree of twice the size,
and so on, until we find a free size.

This means our "forest" corresponds to a bit pattern,
where a set bit means a non-empty tree.
Inserting a point is equivalent to incrementing this bit pattern.

To handle deletions, we simply mark the appropriate point as deleted using another bitset.
When more than half the points have been deleted,
we shrink the structure by removing all deleted points.
This is equivalent to shifting down the "bit pattern" by one.

There are plenty variations that could be explored:

* Keeping a small amount of points in a small pool to make updates faster -
  avoid building and querying small k-d-trees.
  This might be useful if the overhead for small sizes hurts performance.
* Keeping fewer trees to make queries faster, at the expense of updates.
* More eagerly removing entries marked as deleted (for example, on merge).
* Replacing the array-backed structure with a structure of dynamically allocated nodes.
  This would make it possible to "let trees get out of shape".
* Shrinking the structure currently sorts the live points by all axes,
  not leveraging the existing presorting of the subsets.
  Cleverly done filtering followed by sorted merges should enable linear time.
* A special ray proximity query could be implemented. This is tricky however.
*/

namespace k_d_tree
{

using Idx = uint16_t;

// We use size_t for sizes (but not for indices)
// to make sure there are no wraparounds when we approach the limit.
// This hardly affects performance or memory usage;
// the core arrays still only store indices.

template<uint8_t Dim, typename Component>
class Points
{
public:
	using Point = std::array<Component, Dim>;
	//! Empty
	Points() : n(0), coords(nullptr) {}
	//! Allocating constructor; leaves coords uninitialized!
	Points(size_t n) : n(n), coords(new Component[Dim * n]) {}
	//! Copying constructor
	Points(size_t n, const std::array<Component const *, Dim> &coords)
		: Points(n)
	{
		for (uint8_t d = 0; d < Dim; ++d)
			std::copy(coords[d], coords[d] + n, begin(d));
	}

	size_t size() const { return n; }

	void assign(Idx start, const Points &from)
	{
		for (uint8_t d = 0; d < Dim; ++d)
			std::copy(from.begin(d), from.end(d), begin(d) + start);
	}

	Point getPoint(Idx i) const
	{
		Point point;
		for (uint8_t d = 0; d < Dim; ++d)
			point[d] = begin(d)[i];
		return point;
	}

	void setPoint(Idx i, const Point &point)
	{
		for (uint8_t d = 0; d < Dim; ++d)
			begin(d)[i] = point[d];
	}

	Component *begin(uint8_t d) { return coords.get() + d * n; }
	Component *end(uint8_t d) { return begin(d) + n; }
	const Component *begin(uint8_t d) const { return coords.get() + d * n; }
	const Component *end(uint8_t d) const { return begin(d) + n; }

private:
	size_t n;
	std::unique_ptr<Component[]> coords;
};

template<uint8_t Dim>
class SortedIndices
{
public:
	//! empty
	SortedIndices() : indices() {}

	//! uninitialized indices
	static SortedIndices newUninitialized(size_t n)
	{
		return SortedIndices(Points<Dim, Idx>(n));
	}

	//! Identity permutation on all axes
	SortedIndices(size_t n)
		: indices(n)
	{
		for (uint8_t d = 0; d < Dim; ++d) {
			for (Idx i = 0; i < n; ++i) {
				indices.begin(d)[i] = i;
			}
		}
	}

	size_t size() const { return indices.size(); }
	bool empty() const { return size() == 0; }

	struct SplitResult {
		SortedIndices left, right;
		Idx pivot;
	};

	//! Splits the sorted indices in the middle along the specified axis,
	//! partitioning them into left (<=), the pivot, and right (>=).
	SplitResult split(uint8_t axis, std::vector<bool> &markers) const
	{
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

	Idx *begin(uint8_t d) { return indices.begin(d); }
	Idx *end(uint8_t d) { return indices.end(d); }
	const Idx *begin(uint8_t d) const { return indices.begin(d); }
	const Idx *end(uint8_t d) const { return indices.end(d); }
private:
	SortedIndices(Points<Dim, Idx> &&indices) : indices(std::move(indices)) {}
	Points<Dim, Idx> indices;
};

template<uint8_t Dim, class Component>
class SortedPoints
{
public:
	SortedPoints() : points(), indices() {}

	//! Single point
	SortedPoints(const std::array<Component, Dim> &point)
		: points(1), indices(1)
	{
		points.setPoint(0, point);
	}

	//! Sort points
	SortedPoints(size_t n, const std::array<Component const *, Dim> ptrs)
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

	size_t size() const
	{
		// technically redundant with indices.size(),
		// but that is irrelevant
		return points.size();
	}

	Points<Dim, Component> points;
	SortedIndices<Dim> indices;
};

template<uint8_t Dim, class Component, class Id>
class KdTree
{
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
	KdTree(size_t n, Id const *ids, std::array<Component const *, Dim> pts)
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
		// Note: Initialize `deleted` *before* calling `init`,
		// since `init` abuses the `deleted` marks as left/right marks.
		deleted = std::vector<bool>(cap());
		init(0, 0, items.indices);
		std::copy(a.deleted.begin(), a.deleted.end(), deleted.begin());
		std::copy(b.deleted.begin(), b.deleted.end(), deleted.begin() + a.items.size());
	}

	template<typename F>
	void rangeQuery(const Point &min, const Point &max,
			const F &cb) const
	{
		rangeQuery(0, 0, min, max, cb);
	}

	void remove(Idx internalIdx)
	{
		assert(!deleted[internalIdx]);
		deleted[internalIdx] = true;
	}

	template<class F>
	void foreach(F cb) const
	{
		for (Idx i = 0; i < cap(); ++i) {
			if (!deleted[i]) {
				cb(i, items.points.getPoint(i), ids[i]);
			}
		}
	}

	//! Capacity, not size, since some items may be marked as deleted
	size_t cap() const { return items.size(); }

private:
	void init(Idx root, uint8_t axis, const SortedIndices<Dim> &sorted)
	{
		// Temporarily abuse "deleted" marks as left/right marks
		const auto split = sorted.split(axis, deleted);
		tree[root] = split.pivot;
		const auto next_axis = (axis + 1) % Dim;
		if (!split.left.empty())
			init(2 * root + 1, next_axis, split.left);
		if (!split.right.empty())
			init(2 * root + 2, next_axis, split.right);
	}

	template<typename F>
	// Note: root is of type size_t to avoid issues with wraparound
	void rangeQuery(size_t root, uint8_t split,
			const Point &min, const Point &max,
			const F &cb) const
	{
		if (root >= cap())
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
			cb(point, ids[ptid]);
		}
	}
	SortedPoints<Dim, Component> items;
	std::unique_ptr<Id[]> ids;
	std::unique_ptr<Idx[]> tree;
	std::vector<bool> deleted;
};

template<uint8_t Dim, class Component, class Id>
class DynamicKdTrees
{
	using Tree = KdTree<Dim, Component, Id>;

public:
	using Point = typename Tree::Point;

	void insert(const std::array<Component, Dim> &point, Id id)
	{
		Tree tree(point, id);
		for (uint8_t tree_idx = 0;; ++tree_idx) {
			if (tree_idx == trees.size()) {
				trees.push_back(std::move(tree));
				updateDelEntries(tree_idx);
				break;
			}
			// Can we use a free slot to "plant" the tree?
			if (trees[tree_idx].cap() == 0) {
				trees[tree_idx] = std::move(tree);
				updateDelEntries(tree_idx);
				break;
			}
			tree = Tree(tree, trees[tree_idx]);
			trees[tree_idx] = std::move(Tree());
		}
		++n_entries;
	}

	void remove(Id id)
	{
		const auto it = del_entries.find(id);
		assert(it != del_entries.end());
		trees.at(it->second.tree_idx).remove(it->second.in_tree);
		del_entries.erase(it);
		++deleted;
		if (deleted >= (n_entries+1)/2) // "shift out" the last tree
			shrink_to_half();
	}

	void update(const Point &newPos, Id id)
	{
		remove(id);
		insert(newPos, id);
	}

	template<typename F>
	void rangeQuery(const Point &min, const Point &max,
			const F &cb) const
	{
		for (const auto &tree : trees)
			tree.rangeQuery(min, max, cb);
	}

	size_t size() const
	{
		return n_entries - deleted;
	}

private:

	void updateDelEntries(uint8_t tree_idx)
	{
		trees[tree_idx].foreach([&](Idx in_tree_idx, auto _, Id id) {
			del_entries[id] = {tree_idx, in_tree_idx};
		});
	}

	// Shrink to half the size, equivalent to shifting down the "bit pattern".
	void shrink_to_half()
	{
		assert(n_entries >= deleted);
		assert(n_entries - deleted == (n_entries >> 1));
		n_entries -= deleted;
		deleted = 0;
		// Reset map, freeing memory (instead of clearing)
		del_entries = std::unordered_map<Id, DelEntry>();

		// Collect all live points and corresponding IDs.
		const auto live_ids = std::make_unique<Id[]>(n_entries);
		Points<Dim, Component> live_points(n_entries);
		size_t i = 0;
		for (const auto &tree : trees) {
			tree.foreach([&](Idx _, auto point, Id id) {
				assert(i < n_entries);
				live_points.setPoint(static_cast<Idx>(i), point);
				live_ids[i] = id;
				++i;
			});
		}
		assert(i == n_entries);

		// Construct a new forest.
		// The "tree pattern" will effectively just be shifted down by one.
		auto id_ptr = live_ids.get();
		std::array<Component const *, Dim> point_ptrs;
		size_t n = 1;
		for (uint8_t d = 0; d < Dim; ++d)
			point_ptrs[d] = live_points.begin(d);
		for (uint8_t tree_idx = 0; tree_idx < trees.size() - 1; ++tree_idx, n *= 2) {
			Tree tree;
			// If there was a tree at the next position, there should be
			// a tree at this position after shifting the pattern.
			if (trees[tree_idx+1].cap() > 0) {
				tree = std::move(Tree(n, id_ptr, point_ptrs));
				id_ptr += n;
				for (uint8_t d = 0; d < Dim; ++d)
					point_ptrs[d] += n;
			}
			trees[tree_idx] = std::move(tree);
			updateDelEntries(tree_idx);
		}
		trees.pop_back(); // "shift out" tree with the most elements
	}
	// This could even use an array instead of a vector,
	// since the number of trees is guaranteed to be logarithmic in the max of Idx
	std::vector<Tree> trees;
	struct DelEntry {
		uint8_t tree_idx;
		Idx in_tree;
	};
	std::unordered_map<Id, DelEntry> del_entries;
	size_t n_entries = 0;
	size_t deleted = 0;
};

} // end namespace k_d_tree