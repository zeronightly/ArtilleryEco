// SeqRadixFast.hpp
// =============================================================================
// Additive accelerators for seq::radix_set<uint64_t>, friended into the set.
// Consolidated, reintegration-ready header. Contains the core mechanisms:
//
//   1. fast_iterator        -- cached forward iteration (~8x the stock iterator
//                              cache-hot; stock RadixConstIter re-derives the leaf
//                              every element). Use fast_for_each / fast_begin..fast_end.
//   2. branchless_lower_bound-- cmov binary search over a flat sorted u64 array.
//   3. prefix family        -- bit-prefix queries on the LIVE tree (prefix /
//                              prefix_range / prefix_auto) for fixed-width u64 keys.
//
// Companion seq patches shipped alongside (see seq/ tree):
//   * binary_search.hpp  -- cmov leaf lower_bound (arithmetic-key path). ~3-5x at
//                           leaf sizes on GCC/MSVC (Clang branches it; fence Clang
//                           upstream). Speeds every ordered seq op.
//   * bits.hpp           -- SEQ_NO_DEBUG opt-out token.
//   * radix_map.hpp      -- `friend class SeqU64PrefixAccel;` inside radix_set.
//
// DISABLING seq's internal asserts (SEQ_NO_DEBUG):
//   seq gates SEQ_ASSERT_DEBUG on SEQ_DEBUG, which it auto-defines unless NDEBUG.
//   Those asserts sit in hot paths (iterator ++/!=, lower_bound). To strip them in
//   a consuming UE project, add ONE line to that module's Build.cs:
//       PublicDefinitions.Add("SEQ_NO_DEBUG=1");
//   (or define SEQ_NO_DEBUG before the first seq include). bits.hpp honors it.
//
// All tooling here is READ-ONLY on the set: it never mutates seq state and never
// changes existing seq behaviour. Iterators/queries see the live tree.
// =============================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <vector>

#include "seq/radix_map.hpp"

// Global convenience alias; the bench uses it as a variable type throughout.
using SeqSet64 = seq::radix_set<std::uint64_t>;

namespace seq
{
    // Friended into seq::radix_set (see radix_map.hpp). Additive tooling only.
    class SeqU64PrefixAccel
    {
    public:
        using Set            = SeqSet64;
        using const_iterator = Set::const_iterator;

        // Friend-accessible tree internals (radix_tree_type is private in radix_set;
        // the friend grant exposes it, and RadixTree's own members are all public).
        using Tree  = Set::radix_tree_type;
        using Dir   = Tree::directory;
        using Node  = Tree::node;
        using VecTy = Tree::vector_type;
        using Data  = Tree::PrivateData;
        using TIter = Tree::const_iterator;   // RadixConstIter (find_next lives here)

        // -------------------------------------------------------------------
        // fast_iterator -- cached forward iterator.
        //
        // The stock RadixConstIter caches nothing: every operator++/operator*
        // reloads the tagged child pointer, masks the tag, branches on the rare
        // IsVector case, and recomputes values() = base + 8 + *capacity() (a
        // dependent load on the leaf header) + count(). This caches the current
        // leaf's value base + count, so within-leaf ++/* are a plain array read;
        // it only touches the tree (via find_next) to cross a leaf boundary.
        //
        // HONEST EXPECTATIONS: ~1.3x faster than the stock iterator for a real
        // per-element consume (return/process each key). It is NOT the ~10x that a
        // naive `for(x:s) sum+=x` microbench suggests -- that number is a
        // VECTORIZATION artifact: a trivial reduction over a cached contiguous walk
        // auto-vectorizes (AVX2), but it does not vectorize through any iterator,
        // and a real per-element consume can't vectorize either. For iteration-
        // bound loops the 1.3x is real; for work-heavy loops it's noise. If you
        // specifically need a *bulk reduction* (sum/count/min over the whole set),
        // write it over fast_for_each with a vectorizable body to get the ~10x.
        // -------------------------------------------------------------------
        class fast_iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type        = std::uint64_t;
            using difference_type   = std::ptrdiff_t;
            using reference         = std::uint64_t;
            using pointer           = const std::uint64_t*;

            fast_iterator() = default;

            SEQ_ALWAYS_INLINE std::uint64_t operator*() const noexcept
            {
                return vec_ ? vec_->at(pos_) : vals_[pos_];   // vec_ null on the leaf hot path
            }

            SEQ_ALWAYS_INLINE fast_iterator& operator++() noexcept
            {
                if (++pos_ < cnt_) return *this;   // hot path: stay in leaf
                cross();                           // leaf boundary
                return *this;
            }
            SEQ_ALWAYS_INLINE fast_iterator operator++(int) noexcept { fast_iterator t = *this; ++(*this); return t; }

            SEQ_ALWAYS_INLINE bool operator==(const fast_iterator& o) const noexcept
            {
                return dir_ == o.dir_ && child_ == o.child_ && pos_ == o.pos_;
            }
            SEQ_ALWAYS_INLINE bool operator!=(const fast_iterator& o) const noexcept { return !(*this == o); }

        private:
            friend class SeqU64PrefixAccel;

            const Dir*           dir_     = nullptr;   // current leaf's directory (null == end)
            unsigned             child_   = 0;         // leaf slot within dir_
            std::size_t          bit_pos_ = 0;         // dir_'s bit position (for find_next)
            const std::uint64_t* vals_    = nullptr;   // cached leaf value base (leaf case)
            const VecTy*         vec_     = nullptr;   // cached vector node (rare; null == leaf)
            unsigned             cnt_     = 0;         // cached leaf/vector count
            unsigned             pos_     = 0;         // index within leaf

            // Cache the leaf at (dir_, child_); reset pos_. Trivially copyable.
            void load() noexcept
            {
                const auto c = dir_->const_child(child_);
                if (c.tag() == Dir::IsLeaf) {
                    const Node* n = static_cast<const Node*>(c.ptr());
                    vals_ = n->values();
                    cnt_  = n->count();
                    vec_  = nullptr;
                } else {   // IsVector (rare)
                    vec_  = static_cast<const VecTy*>(c.ptr());
                    cnt_  = static_cast<unsigned>(vec_->size());
                    vals_ = nullptr;
                }
                pos_ = 0;
            }

            // Advance to the next leaf, or become end().
            void cross() noexcept
            {
                const auto nx = TIter::find_next(dir_, child_ + 1, bit_pos_);
                if (!nx.dir) { dir_ = nullptr; child_ = 0; pos_ = 0; vals_ = nullptr; vec_ = nullptr; cnt_ = 0; return; }
                dir_ = nx.dir; child_ = nx.child; bit_pos_ = nx.bit_pos;
                load();
            }
        };

        // Cached-iterator range over the live set.
        static fast_iterator fast_begin(const SeqSet64& s)
        {
            fast_iterator it;
            const Data* data = s.d_tree.d_data;
            if (!data) return it;                                  // empty -> end
            const auto nx = TIter::find_next(s.d_tree.d_root, 0, 0);
            if (!nx.dir) return it;                                // empty -> end
            it.dir_ = nx.dir; it.child_ = nx.child; it.bit_pos_ = nx.bit_pos;
            it.load();
            return it;
        }
        static fast_iterator fast_end(const SeqSet64&) { return fast_iterator{}; }

        // Diagnostic only: first-leaf position, for building external iterators in
        // isolation tests (the test TU isn't a friend of radix_set).
        static TIter::PosInDir diag_first_leaf(const SeqSet64& s)
        {
            if (!s.d_tree.d_data) return TIter::PosInDir{ nullptr, 0, 0 };
            return TIter::find_next(s.d_tree.d_root, 0, 0);
        }

        // Cached for_each: streams each leaf's contiguous values, fn(key) per
        // element. The tightest consume path (no per-element branch, no iterator
        // object churn) -- the bulk-scan form of fast_iterator.
        template <typename Fn>
        static void fast_for_each(const SeqSet64& s, Fn fn)
        {
            const Data* data = s.d_tree.d_data;
            if (data) for_each_rec(s.d_tree.d_root, fn);
        }

        // -------------------------------------------------------------------
        // branchless_lower_bound -- cmov binary search over a sorted u64 array.
        // The window shrinks on a data-independent schedule (loop branch predicted)
        // and the only data-dependent op is one cmov. Branchless on GCC/MSVC; Clang
        // converts the select to a branch (X86CmovConversion) and can't be coaxed
        // portably -- fence Clang if this ships somewhere Clang-built.
        // -------------------------------------------------------------------
        static SEQ_ALWAYS_INLINE const std::uint64_t*
        branchless_lower_bound(const std::uint64_t* base, std::size_t n, std::uint64_t key)
        {
            while (n > 1) {
                const std::size_t half = n >> 1;
                base = (base[half] < key) ? base + half : base;   // cmov
                n -= half;
            }
            base += (n > 0 && *base < key);
            return base;
        }

        // -------------------------------------------------------------------
        // prefix family -- bit-prefix queries on the LIVE tree (no snapshot).
        // A "k-bit prefix of val" is its top k bits; matching keys form a
        // contiguous sorted block [lower_bound(lo), lower_bound(hi)), with
        // shift = 64-k, lo = (val>>shift)<<shift, hi = lo + (1<<shift).
        //   prefix        -> first matching key, or end()
        //   prefix_range  -> [first,last) subrange (two descents; best for BROAD)
        //   prefix_for_each-> one descent + stop-scan (best for SELECTIVE)
        //   prefix_auto   -> dispatch on estimated match count
        // k==0: whole set; k>=64: exact match on val.
        // -------------------------------------------------------------------
        static SEQ_ALWAYS_INLINE const_iterator
        prefix(const Set& s, std::uint64_t val, unsigned k_bits)
        {
            if (s.empty())    return s.end();
            if (k_bits == 0)  return s.begin();
            if (k_bits >= 64) { auto it = s.lower_bound(val); return (it != s.end() && *it == val) ? it : s.end(); }
            const unsigned      shift = 64u - k_bits;
            const std::uint64_t pre   = val >> shift;
            const std::uint64_t lo    = pre << shift;
            auto it = s.lower_bound(lo);
            return (it != s.end() && (*it >> shift) == pre) ? it : s.end();
        }

        static SEQ_ALWAYS_INLINE std::ranges::subrange<const_iterator>
        prefix_range(const Set& s, std::uint64_t val, unsigned k_bits)
        {
            if (k_bits == 0)  return {s.begin(), s.end()};
            if (k_bits >= 64) return {s.lower_bound(val), s.upper_bound(val)};
            const unsigned      shift = 64u - k_bits;
            const std::uint64_t lo    = (val >> shift) << shift;
            const std::uint64_t hi    = lo + (std::uint64_t{1} << shift);
            const auto first = s.lower_bound(lo);
            const auto last  = (hi == 0) ? s.end() : s.lower_bound(hi);   // hi==0: top block, clamp
            return {first, last};
        }

        template <typename Fn>
        static SEQ_ALWAYS_INLINE void
        prefix_for_each(const Set& s, std::uint64_t val, unsigned k_bits, Fn fn)
        {
            if (k_bits == 0) { for (std::uint64_t key : s) fn(key); return; }
            const unsigned      shift = (k_bits >= 64) ? 0u : (64u - k_bits);
            const std::uint64_t pre   = val >> shift;
            const std::uint64_t lo    = pre << shift;
            for (auto it = s.lower_bound(lo); it != s.end(); ++it) {
                const std::uint64_t key = *it;
                if ((key >> shift) != pre) break;
                fn(key);
            }
        }

        // Adaptive dispatch (inlined scan; pass s.end() hoisted). Broad prefixes ->
        // clean prefix_range scan; selective -> single descent + stop-scan.
        template <typename Fn, std::size_t kBroadMatches = 16>
        static SEQ_ALWAYS_INLINE void
        prefix_auto(const Set& s, std::uint64_t val, unsigned k_bits, Fn fn, const const_iterator& end_hint)
        {
            const std::size_t est = (k_bits == 0) ? s.size() : (k_bits >= 64) ? std::size_t{0} : (s.size() >> k_bits);
            if (est > kBroadMatches) {
                for (std::uint64_t key : prefix_range(s, val, k_bits)) fn(key);
            } else {
                const unsigned      shift = (k_bits >= 64) ? 0u : (64u - k_bits);
                const std::uint64_t lo    = (val >> shift) << shift;
                const std::uint64_t pre   = val >> shift;
                for (auto it = s.lower_bound(lo); it != end_hint; ++it) {
                    const std::uint64_t key = *it;
                    if ((key >> shift) != pre) break;
                    fn(key);
                }
            }
        }

    private:
        template <typename Fn>
        static void for_each_rec(const Dir* d, Fn& fn)
        {
            const unsigned sz = d->size();
            for (unsigned i = 0; i < sz; ++i) {
                const auto c   = d->const_child(i);
                const auto tag = c.tag();
                if (tag == Dir::IsDir) {
                    for_each_rec(c.to_dir(), fn);
                } else if (tag == Dir::IsLeaf) {
                    const Node*          n = static_cast<const Node*>(c.ptr());
                    const std::uint64_t* v = n->values();      // base + count read ONCE per leaf
                    const unsigned       cnt = n->count();
                    for (unsigned j = 0; j < cnt; ++j) fn(v[j]);   // contiguous stream
                } else if (tag == Dir::IsVector) {
                    const VecTy*      vec = static_cast<const VecTy*>(c.ptr());
                    const std::size_t vs  = vec->size();
                    for (std::size_t j = 0; j < vs; ++j) fn(vec->at(j));
                }
            }
        }
    };

} // namespace seq
