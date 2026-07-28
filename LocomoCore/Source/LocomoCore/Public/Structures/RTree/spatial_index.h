// Booster — SpatialIndex facade.
//
// A deterministic 2D spatial index (R*-tree) behind a Boost-free, Jolt-free, template-free
// interface. Consumers include ONLY this header: no Boost avalanche, no template errors,
// no Jolt setup — just link the compiled Booster library. All the heavy machinery
// (Boost.Geometry rtree + Jolt's deterministic sort) lives in spatial_index.cpp.
//
// Determinism: given the same sequence of operations and the same inputs, results are
// identical across platforms (the underlying rtree uses fallback-free deterministic
// select/sort — see thirdparty/DETERMINISM.md). Query results come back in the tree's
// traversal order, which is itself deterministic (same ops -> same tree -> same order),
// so they can be folded directly into a lockstep/rollback state hash. They are NOT sorted
// by Id — if you need a canonical, tree-structure-independent order, sort the result
// yourself (it's cheap and off the query hot path). Coordinates are IEEE-754 double; build
// the library with /fp:precise (no contraction) and no fast-math on every target for
// cross-platform bit-identical results.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace booster {

using Id    = std::uint64_t;   // caller-owned stable entity id
using Coord = double;          // single typedef — switch here to match another scalar

/// Axis-aligned bounding box (2D). min must be <= max on each axis.
struct AABB {
    Coord min_x, min_y, max_x, max_y;
};

class SpatialIndex {
public:
    SpatialIndex();
    ~SpatialIndex();

    // Move-only (owns a pimpl); copying a spatial index is almost never intended.
    SpatialIndex(SpatialIndex&&) noexcept;
    SpatialIndex& operator=(SpatialIndex&&) noexcept;
    SpatialIndex(const SpatialIndex&) = delete;
    SpatialIndex& operator=(const SpatialIndex&) = delete;

    /// Build in one shot from parallel arrays (packing load — faster, better-shaped tree
    /// than repeated insert). boxes[i] is stored under ids[i].
    static SpatialIndex bulk_build(const AABB* boxes, const Id* ids, std::size_t n);

    void        insert(const AABB& box, Id id);
    /// Removes one entry equal to (box, id). Returns true if one was removed.
    bool        remove(const AABB& box, Id id);
    void        clear();
    std::size_t size() const;
    bool        empty() const;

    // --- queries (out is cleared then filled; results sorted ascending by Id) ---

    /// Ids whose box intersects `region`.
    void query_intersects(const AABB& region, std::vector<Id>& out) const;
    /// Ids whose box lies entirely within `region`.
    void query_within(const AABB& region, std::vector<Id>& out) const;

    /// The k ids whose box is nearest to point (x, y), in ascending distance order
    /// (NOT id-sorted — distance order is the point of the query, and is deterministic).
    void nearest(Coord x, Coord y, int k, std::vector<Id>& out) const;

    /// AABB enclosing all entries. Returns {0,0,0,0} if empty.
    AABB bounds() const;

private:
    struct Impl;
    Impl* p_;
};

} // namespace booster
