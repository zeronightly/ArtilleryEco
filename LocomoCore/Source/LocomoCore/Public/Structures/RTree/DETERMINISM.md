# Boost.Geometry rtree — Determinism Analysis & Plan

**Target requirement:** multi-platform rollback with partial lockstep, with a
**guaranteed operation order** (same insert/remove/query sequence on every peer).

**Vendored:** Boost 1.91.0, `boost/geometry/index/rtree.hpp` (cartesian).

---

## TL;DR

- **Same platform / same STL: fully deterministic.** Empirically proven — identical
  output across 5 separate processes with ASLR live, for both incremental `insert()`
  and the packing/bulk constructor. No RNG, no threads, **no hash/`unordered`
  containers on the rtree path**, and `nearest()` ties break by tree structure (not
  pointer/heap), stably.
- **Cross-platform: achievable, but NOT free.** Two hazards must be controlled:
  1. **STL sort tie-arrangement** (the real one) — the rtree uses `std::sort` /
     `std::partial_sort` / `std::nth_element`, whose arrangement of *equal-key*
     elements is implementation-defined. Different STLs (MSVC / libstdc++ / libc++)
     can build **different tree structure** when ties exist (coincident boxes, equal
     coordinates/distances — common in games).
  2. **Float rounding** — mitigated by the fact that the cartesian path has **no
     transcendentals** (nearest ranks by *squared* distance; packing shape is chosen
     by *integer* arithmetic), so only basic IEEE `+ - * min max` and comparisons are
     involved. Controllable with FP-flag discipline; not auto-safe for arbitrary data.

**Bottom line:** it is usable for your case, but you must (a) pin one STL across all
platforms and/or make the internal comparators total orders, (b) apply FP discipline,
and (c) treat query *result sets* — not raw traversal order — as the canonical state,
or over-fetch `nearest` ties and resolve by id. Fixed-point (integer) coordinates
remove hazard #2 entirely and are strongly recommended for lockstep.

---

## What was tested (evidence)

Harness: build an rtree with a fixed op sequence including **5 coincident boxes**
(deliberate zero-distance `nearest` ties), run a query/`nearest`/traversal battery,
FNV-hash all returned id-sequences into a single `DIGEST`. Run as **separate
processes** so ASLR randomizes every allocation; if any result depended on address or
hash order, digests would diverge.

| Test | Result |
|------|--------|
| `insert()` path, 5 processes (ASLR live) | **Identical** digest `96f0c760…` |
| Packing/bulk constructor, 5 processes | **Identical** digest `cd9ef44e…` |
| FP flags: `/fp:precise`, `/fp:fast`, `+/arch:AVX2` (FMA), `fast+AVX2` | **All identical** (this data) |
| `nearest` ties (5 co-located boxes, k=3) | **Stable** per algorithm across every run |

Notes:
- Tie *order* differs **between** algorithms and between insert-vs-packing (different
  tree shape) but is perfectly reproducible **within** a given config. Determinism ≠
  "sorted by id"; it means "same config → same output, every time."
- The all-flags-identical result is reassuring but **data-dependent**: it shows this
  data doesn't sit on a rounding boundary, not that no data ever will. Keep FP
  discipline regardless.

## Source audit (why it behaves this way)

- **No `unordered`/hash containers** anywhere on the rtree insert/query/nearest path
  (the `boost/unordered` in the vendored closure is umbrella ballast, never executed).
  This is why run-to-run is address-independent.
- **No transcendentals** on the cartesian path — `grep` finds `sqrt` only in comments.
  `nearest` uses `comparable_distance` = **squared** distance (no `sqrt`).
- **Packing tree shape is integer arithmetic**: subtree counts computed by integer
  division (`maxc /= max_elements`), not `pow`/`sqrt` (the `sqrt(count/max)` is a
  comment describing the concept only).
- **The tie hazard, located precisely:**
  - `boost/geometry/index/detail/rtree/rstar/insert.hpp` → `std::partial_sort` (forced reinsertion, by distance-to-center)
  - `boost/geometry/index/detail/rtree/rstar/redistribute_elements.hpp` → `std::sort` (split-axis choice)
  - `boost/geometry/index/detail/rtree/pack_create.hpp` → `std::nth_element` (bulk load)
  Boost's own source even comments on stdlibc++-version-specific behavior here — a red
  flag that equal-key arrangement is STL-defined.

---

## The plan (how to make it deterministic for multi-platform rollback)

Ordered from "do this always" to "belt-and-suspenders":

### 1. Replace the 3 STL sort/select sites with SELF-CONTAINED deterministic ones ← primary fix
This is the surgical fix for the tie-arrangement hazard, and it does not require
pinning the whole toolchain (often impossible across consoles). Substitute a single
*vendored* implementation at each call site so the equal-key arrangement is fixed by
that one implementation on every platform:

| rtree site | current | replace with | status |
|---|---|---|---|
| `detail/algorithms/nth_element.hpp` (bulk-load wrapper, large range) | `std::nth_element` | **`miniselect::median_of_ninthers_select`** | ✅ **DONE** — self-contained deterministic select, no std fallback |
| `rstar/insert.hpp` (reinsert, node-bounded) | `std::partial_sort` | **`booster_sort::QuickSort`** | ✅ **DONE** — Jolt's sort, vendored (MIT) |
| `rstar/redistribute_elements.hpp` (node-bounded) | `std::sort` | **`booster_sort::QuickSort`** | ✅ **DONE** — same |

> **Why Jolt's `QuickSort`, not a hand-rolled or pdq sort.** It was written specifically to fix
> this problem — its own comment: *"The STL version implementation is not consistent across
> platforms."* Battle-tested in a shipping cross-platform-deterministic physics engine,
> comparator-based (drop-in for `std::sort`; the reinsert site full-sorts its node-bounded list,
> whose leading `reinserted_elements_count` elements match the old `partial_sort`), **no `std::`
> fallback** (falls back to its own `InsertionSort` ≤32), no RNG, deterministic ninther pivot →
> deterministic at **any** node size (no pdqsort `insertion_sort_threshold`=24 footgun). Verified
> correct + identical digest at `rstar<16/32/64/128>`.
>
> **Vendored** (not referenced) at `thirdparty/boost-rtree/booster_sort/{quick_sort.h,
> insertion_sort.h}` — Jolt's algorithm byte-for-byte, only the `JPH` namespace/assert macros
> adapted (→ namespace `booster_sort`, `assert`). This drops ALL external Jolt dependency: the
> rtree and the facade `.cpp` no longer include or link Jolt.

> **Do NOT use locomo's `IntelSimdSorts`** for anything on the deterministic path. They are
> CPU/ISA-dispatched (`#if __AVX512…`) and SIMD sorting networks arrange equal keys differently
> from the scalar path — so results diverge *by target*, the opposite of what we need.

**Implemented & verified (all 3 sites):**
- Bulk-load select (`nth_element.hpp`) → vendored `thirdparty/boost-rtree/miniselect/`
  (`median_of_ninthers.h`, `private/median_common.h`; Boost SW License). Only vendored addition.
- rstar reinsert + split (`insert.hpp`, `redistribute_elements.hpp`) → `booster_sort::QuickSort`,
  Jolt's sort vendored verbatim (MIT) under `booster_sort/` — no external Jolt dependency.

Vendored surface is now two small self-contained pieces: `miniselect/` (select) and
`booster_sort/` (Jolt's QuickSort/InsertionSort). Tests: bulk-load path (median_of_ninthers)
compiles `/W4 /WX` clean and correct on heavy ties; patched rtree compiles with **no external
dependencies** and is correct vs brute force with heavy coincident-key data; identical digest
across 5 ASLR-live processes; correctness holds for node capacities 16/32/64/128. Cross-platform
agreement still to be gated in CI (single arch here).

## Delivery: the SpatialIndex facade (`include/booster/spatial_index.h`, `src/spatial_index.cpp`)

The pimpl facade is where determinism is *packaged*: it hides Boost + Jolt entirely (consumers
include only the Boost/Jolt-free header and link `booster.lib`), and it is the single place the
FP-flag discipline lands — **build `spatial_index.cpp` with `/fp:precise` (no contraction) and no
fast-math on every target** (see step 2 above). Query results come back in the tree's **traversal
order**, which is deterministic (same ops → same tree → same order), so they fold straight into a
lockstep/rollback hash; they are **not** id-sorted (that per-query `std::sort` was pure overhead —
~2.4× the whole query — and redundant given the deterministic tree). Callers who need a canonical,
structure-independent order sort the small result themselves. `nearest` returns distance-ordered
with `(distance, id)` as a total order (platform-independent). Verified: a consumer TU compiling
with **no** Boost/Jolt include paths, correct vs brute force, identical digest across 5 processes;
query overhead over a raw rtree query is within measurement noise (RDTSC).

**Remaining open item:** the cross-arch CI digest gate (build the digest harness on each target
platform, assert equality). Everything is deterministic by construction; this makes it *verified*.

**CRITICAL — not every "deterministic" selector is safe.** Audited against source:
- `pdqselect` (what we currently vendor) is deterministic on its fast path **but falls
  back to `std::nth_element`** when bad-partition count exceeds a threshold — i.e. on
  adversarial input with **many coincident keys, which games produce constantly**
  (grid-aligned / stacked entities). That fallback re-imports the cross-STL divergence.
  **Do not** use bare `pdqselect` for the pack site. Either use `median_of_ninthers`
  (no fallback), or patch pdqselect's fallback from `std::nth_element` to its own
  `heap_select`/median-of-medians.
- `pdqsort` / `pdqpartial_sort` fall back to heapsort (`std::make_heap`/`std::sort_heap`),
  also not guaranteed identical across STLs for equal keys. Prefer a self-contained sort
  for the two node-bounded sites (they're tiny, so a stable insertion sort is ideal).
- `median_of_ninthers` **select** is clean (self-contained). Its **partial_sort** variant
  calls `std::sort` on the selected prefix — so for the node-bounded partial-sort site,
  use a small self-contained sort, not that variant.

We already vendor `pdqselect`; bringing over `median_of_ninthers` (and a tiny
deterministic insertion sort, or `pdqpartial_sort` with its fallback removed) closes all
three sites. All are `miniselect::` header-only (`median_of_ninthers.h`, etc.).
These become vendored-Boost patches at 3 call sites — document them for re-extraction.

### 1b. (Alternative to 1) Pin ONE standard library across all platforms
If every target can compile against the **same** STL (e.g. Clang + libc++ everywhere),
`std::sort`/`partial_sort`/`nth_element` arrange ties identically → tree structure
matches. Simpler if achievable, but console toolchains often make this impossible, which
is why the self-contained substitution (step 1) is the more portable fix.

> Note on total-order comparators: making the rtree's comparators tie-break by entity id
> would also make any conforming sort deterministic — but it only works at **leaf** level.
> **Internal**-node entries are (box, child-pointer) with no entity id and address-based
> pointers, so that approach can't tie-break internal-node sorts deterministically.
> Prefer the self-contained-algorithm substitution above, which needs no unique key.

### 2. FP discipline (per compiler)
- MSVC: `/fp:precise` (default) — **do not** use `/fp:fast`.
- GCC/Clang: `-ffp-contract=off -fexcess-precision=standard`, **no** `-ffast-math`.
- IEEE-754 `double` on all targets (all modern x64/ARM64 qualify). Avoid 32-bit x87.
- Rationale: the only FP ops are `+ - * min max` + compares; with contraction off and
  no fast-math, these are bit-identical across IEEE-754 platforms.

### 3. Prefer fixed-point (integer) coordinates  ← removes FP hazard entirely
Use `bg::model::point<int64_t, 2, cartesian>`. Box predicates become **exact integer
compares**; `nearest` squared distance is exact integer (use `int64` coords / mind
overflow for large extents). This deletes hazard #2 outright and is the standard
lockstep approach. (Validated to compile & run deterministically — see harness
`det_int.cpp`.)

### 4. Canonicalize what you feed into the lockstep hash
Don't hash raw traversal/query order. Sort query results by a **total key (entity id)**
before folding into simulation state. This neutralizes any residual ordering
divergence and costs almost nothing. For `nearest` with ties at the k-boundary,
**over-fetch to the tie distance and pick by id**, so which tied element "wins" is not
left to tree structure.

### 5. (Superseded — see note in step 1) Total-order comparators
Tie-breaking comparators by value id was considered but only works at leaf level;
internal-node entries have no entity id (box + child-pointer). Use the self-contained
algorithm substitution in step 1 instead — it needs no unique key and covers internal
nodes too.

### 6. Verify it in CI — this is how you actually *prove* it per release
Ship the digest harness (`det_test.cpp` / `det_pack.cpp`). Build & run it on **every**
target platform in CI and assert all digests match a golden value. Cross-platform
agreement can only be proven by running on each arch; this makes it a gate, not a hope.
(This report's same-platform proofs were run on MSVC 14.51 / x64 only.)

---

## Residual risk / what is NOT yet proven

- Cross-platform bit-identical agreement has **not** been executed here (single arch
  available). Steps 1–4 make it *sound*; step 6 makes it *verified*. Do not ship
  lockstep without the CI cross-arch digest gate.
- The FP-flag-insensitivity observed is for one dataset. Real data with near-boundary
  coordinates could still flip a compare under a divergent FP config — another reason
  to prefer fixed-point (step 3).
