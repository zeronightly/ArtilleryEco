// Booster — SpatialIndex implementation. All Boost machinery is confined here; the rtree's
// deterministic sort (Jolt's QuickSort) is vendored, so this TU has no external dependency.

#include "Structures/RTree/spatial_index.h"


#include "CoreMinimal.h"
THIRD_PARTY_INCLUDES_START

#pragma push_macro("check")
#pragma push_macro("dynamic_cast") //in bird person culture, redefining dynamic cast is widely regarded as a dick move. christ, c'mon guys.
#undef dynamic_cast
#undef check // Boost defines its own version of check, so undef UE's to prevent conflicts
 
#include <boost/geometry/index/rtree.hpp>       // shadows in our 3 patched headers automatically
#include <boost/geometry/geometries/point.hpp>  
  
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry.hpp>
#include <boost/geometry/geometry.hpp>



#pragma pop_macro("dynamic_cast")
#pragma pop_macro("check")
THIRD_PARTY_INCLUDES_END

#include <algorithm>
#include <iterator>
#include <utility>

namespace booster {

namespace bg  = boost::geometry;
namespace bgi = boost::geometry::index;

using point = bg::model::point<Coord, 2, bg::cs::cartesian>;
using box   = bg::model::box<point>;
using value = std::pair<box, Id>;

inline box to_box(const AABB& a) {
    return box(point(a.min_x, a.min_y), point(a.max_x, a.max_y));
}

// Output iterator that appends each result value's Id straight into the caller's vector,
// so the rtree's fast callback-form query writes through with no intermediate container
// and no per-query sort. Forwarding operator= moves the (trivially-copyable) Id through.
struct id_sink {
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    std::vector<Id>* out;
    id_sink& operator*()      { return *this; }
    id_sink& operator++()     { return *this; }
    id_sink  operator++(int)  { return *this; }
    template <class V>
    id_sink& operator=(V&& v) { out->push_back(std::forward<V>(v).second); return *this; }
};

// R*-tree tuning: OverlapCostThreshold = 4 caps the O(n^2) sibling-overlap check during
// subtree selection to the 4 best candidates. Measured (n=10, 1M boxes): ~1.27x faster
// insert with query time unchanged -- strictly better than the default (32). Min/Reins are
// kept at their defaults so only the threshold changes.
using params  = bgi::rstar<16,
                           bgi::detail::default_min_elements_s<16>::value,
                           bgi::detail::default_rstar_reinserted_elements_s<16>::value,
                           4>;
using rtree_t = bgi::rtree<value, params>;


struct SpatialIndex::Impl {
    rtree_t tree;
};

SpatialIndex::SpatialIndex() : p_(new Impl) {}
SpatialIndex::~SpatialIndex() { delete p_; }

SpatialIndex::SpatialIndex(SpatialIndex&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
SpatialIndex& SpatialIndex::operator=(SpatialIndex&& o) noexcept {
    if (this != &o) { delete p_; p_ = o.p_; o.p_ = nullptr; }
    return *this;
}

SpatialIndex SpatialIndex::bulk_build(const AABB* boxes, const Id* ids, std::size_t n) {
    std::vector<value> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        v.emplace_back(to_box(boxes[i]), ids[i]);
    SpatialIndex si;
    si.p_->tree = rtree_t(v.begin(), v.end()); // packing load
    return si;
}

void SpatialIndex::insert(const AABB& b, Id id) { p_->tree.insert(std::make_pair(to_box(b), id)); }
bool SpatialIndex::remove(const AABB& b, Id id) { return p_->tree.remove(std::make_pair(to_box(b), id)) > 0; }
void SpatialIndex::clear() { p_->tree.clear(); }
std::size_t SpatialIndex::size() const { return p_->tree.size(); }
bool SpatialIndex::empty() const { return p_->tree.empty(); }

void SpatialIndex::query_intersects(const AABB& region, std::vector<Id>& out) const {
    out.clear();
    p_->tree.query(bgi::intersects(to_box(region)), id_sink{&out}); // callback form: tight traversal
}

void SpatialIndex::query_within(const AABB& region, std::vector<Id>& out) const {
    out.clear();
    p_->tree.query(bgi::within(to_box(region)), id_sink{&out});
}

void SpatialIndex::nearest(Coord x, Coord y, int k, std::vector<Id>& out) const {
    out.clear();
    if (k <= 0) return;
    const point q(x, y);
    // Collect the k nearest, then order by (distance, id). Ids are unique so the pair is a
    // strict total order -> std::sort is deterministic across platforms, and callers get
    // nearest-first. comparable_distance is Boost's own (squared) metric — no hand-rolled math.
    std::vector<std::pair<Coord, Id>> hits;
    rtree_t::const_query_iterator it = p_->tree.qbegin(bgi::nearest(q, k));
    for (; it != p_->tree.qend(); ++it)
        hits.emplace_back(bg::comparable_distance(q, it->first), it->second);
    std::sort(hits.begin(), hits.end(),
              [](const std::pair<Coord, Id>& a, const std::pair<Coord, Id>& b) {
                  if (a.first != b.first) return a.first < b.first;
                  return a.second < b.second;
              });
    out.reserve(hits.size());
    for (const auto& h : hits) out.push_back(h.second);
}

AABB SpatialIndex::bounds() const {
    if (p_->tree.empty()) return AABB{0, 0, 0, 0};
    const box b = p_->tree.bounds();
    return AABB{ bg::get<bg::min_corner, 0>(b), bg::get<bg::min_corner, 1>(b),
                 bg::get<bg::max_corner, 0>(b), bg::get<bg::max_corner, 1>(b) };
}

} // namespace booster
