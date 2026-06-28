# LocomoCore
LocomoCore collects fragments from a number of movement modes from empirically observed designs, samizdat, or whiteboard doodles and distills them into novel, non-derivative pure math. LocomoCore DOES NOT provide complete movement systems, locomotion state machines, or anything comparable. The goal is not to steal or otherwise copy product identity. Instead, small mathematical tricks or bits of cleverness that would normally be lost are preserved here. 

# Relationship With SkeletonKey
The skeleton key lib owns certain dependencies that are used throughout the plugin ecosystem. I'll list the contents here because there's a fair bit of overlap. They are separate primarily for historical and sanity reasons, as well as compile speed reasons.

## LocomoCore

Low-level algorithms and data structures for game development.
Path: `Plugins/LocomoCore/Source/LocomoCore/`

### Radix Sort — HRadix (`Public/HRadix.h`)
6-pass LSD radix sort on 64-bit keys, 11-bit slices (2048-bucket histograms).
Single-pass parallel histogramming, SSE prefetch on scatter. Derived from Herf
Consulting LLC 2001 (MIT Licensed), adapted by JMK circa 2008. Already extracted
into standalone form in `lib/gks_hull/gks_hull.h`.

### Convex Hull — GKS Hull 
Original Graham-Kurzer Scan. Convex hull in linear time, faster than most implementations by quite a bit even after constant factors.

### Arc Shots (`Public/FArcShot.h`)
Projectile arc calculations. USTRUCT wrapper.

### Deterministic RNG (`Public/FakeRandom.h`)
Deterministic pseudo-random number generator. UE-integrated USTRUCT. Relevant to
determinism lib design.

### Float Bit Manipulation (`Public/Distances/AtypicalDistances.h`)
A huge pile of really weird distance measures that are fairly commonly used in games, many existing only as largely undocumented cultural knowledge.

### Spatial Encoding — Distances (`Public/Distances/`)
- `hilbert_curves.h` — Hilbert curve encoding/decoding.
- `MortonMachinery.h` — Morton code (Z-order) utilities.
- `ZOrderDistances.h` — Z-order curve distance functions, spatial hashing.

### Morton Library (`Public/LibMorton/`)
Third-party Morton encoding. 2D and 3D variants, LUT-based and BMI instruction
paths, AVX-512 BITALG support.

### Sorts (`Public/Sorts/`)
- `pdqselect.h` — Pattern-defeating quickselect (nth_element alternative).
- `median_common.h` — Median selection algorithms.
- `IntelSimdSorts/` — Intel AVX2/AVX-512 SIMD sort. Insanely fast.

### R-Tree (`Public/Structures/RTree/`)
Permissively licensed R-Tree spatial index. Bounding box queries, predicates.
Largely untested per JMK notes.

### Soft Heap (`Public/Structures/softheap.h`, `Private/softheap.cpp`)
Approximate priority queue. Constant-time extract-min with controlled corruption
rate. Interesting alternative to binary heap for pathfinding open lists.

### Sparse Sets (`Public/Structures/SuperSparseSets/`)
Sparse set implementation with benchmarks. O(1) insert/delete/membership, O(n)
iteration over live elements. Useful for entity tracking.

### Parallel Hashmap (`Public/Structures/parallel_hashmap/`)
Abseil-derived parallel flat hashmap (third-party). Lock-free concurrent reads,
sharded for concurrent writes. Largely superseded for us by the Seq lib, but it
is faster in many cases.

### Approximate Membership Queries (`Public/Structures/ApproximateMembership/`)
- `simd-block.h` — SIMD block Bloom filter. Highly optimized, wide range of sizes.
- `SmallGate.h` — For sets up to ~200 elements.
- `FLargeGate.h` — Larger gate variant.

### Probabilistic Counting (`Public/Structures/probabilisticcount.h`)
HyperLogLog-adjacent cardinality estimation.

### Ring Buffers (`Public/Structures/`)
- `FixedWidthCircularBuffer.h` — Fixed-width ring buffer.
- `PascalCircularBuffer.h` — Pascal-style circular buffer.
- `InterpolationTable.h` — Interpolation lookup table.

### Concurrency Primitives (`Public/Structures/ConcurrencyTypes/`)
- `ParallelFixedQueueTypes.h` — Lock-free fixed-size queues.
- `FixedGCounter.h` — CRDT grow-only counter.
- `FixedMVReg.h` / `FixedMVDReg.h` — Multi-value registers.
- `AtomicCoheredReadHead.h` — Atomic coherence for reader threads.
- `TimeCoheredReadHead.h` — Time-based coherence for reader threads.

### Memory Allocators (`Public/Memory/`)
- `tlsf.h` / `Private/tlsf.cpp` — Two-Level Segregated Fit allocator. Production-tested.
  O(1) malloc/free, good fragmentation behavior. Good candidate for path node allocation.
- `arena.h` — Arena allocator (untested).
- `IntraTickThreadblindAlloc.h` — Per-tick thread-blind allocator. Designed for
  allocation patterns where everything allocated in one tick is freed before the next.
- `aligned_allocator.h` — Aligned memory allocation.
- `sseutil.h` — SSE memory utilities.

### Utilities
- `LocomoUtil.h` — General utility functions.
- `CompileTimeStrings.h` — Compile-time string hashing.
- `LowLogTimeAndRate.h` — Rate-limited logging.
- `LCM_Config.h` — Plugin configuration.
- `hedley.h` — Compiler portability macros (third-party, Evan Nemerson).

---

## SkeletonKey

Entity identity and transform dispatch system. An ECS-adjacent keying layer.
Path: `Plugins/SkeletonKey/Source/SkeletonKey/`

### Skeleton Key System (`Public/Skeletonize.h`, `Public/SkeletonTypes.h`)
64-bit entity keys with typed prefix nibble: 4 bits type, 28 bits metadata, 32
bits hash. Type tags cover actors, projectiles, items, archetypes, instances,
sockets, etc. Keys are immutable once written. Notch-based subtyping allows
hierarchical key composition for Bloom and radix filtering compatibility.

### Key Carrier (`Public/KeyCarry.h`)
UActorComponent that auto-wires an actor's skeleton key. Retries on init failure,
delegate notification on success.

### Kines — Transform Binding (`Public/Kines.h`)
Kines connect skeleton keys to UE transforms. Abstract interface for actor kines,
scene component kines, etc. The transform dispatch treats kines as ECS-style
function objects tracked per key.

### Swarm Kine (`Public/SwarmKine.h`)
Manages many instanced meshes via skeleton keys. Uses seq::concurrent_map for
key-to-mesh-instance mapping. Bulk transform management.

### Transform Dispatch (`Public/TransformDispatch.h`)
World subsystem that owns all kine lookups. Central registry mapping skeleton
keys to transforms. Uses a 1P1C ring buffer (ConservedStream) for lock-free
transform update streaming.

### ORDIN — Deterministic Init Ordering (`Public/ORDIN.h`)
Deterministic initialization sequencing for ECS pillars, key carriers, and player
objects. Essential for lockstep determinism — ensures remote and local instances
initialize in the same order.

### Conserved Stream (`Public/ConservedStream.hpp`)
1-producer-1-consumer lock-free ring buffer. Templated on window size and value
type. Uses circular indexing with a safety gap between reader and writer.
Extremely fast, minimal atomics.

### MPSC Key Queue (`Public/MPSCKeyQueue.h`, `Private/MPSCKeyQueue.cpp`)
Multi-producer single-consumer lock-free queue for skeleton keys. Unfair FILO
design, max 255 producers. Uses intrusive linked nodes with key batching (7 keys
per node). Somewhat untested. BLK from barrage ended up used instead.

### Quaternion Quantizer (`Public/QuaternionQuantizer.h`)
Bit-budget quaternion compression (templated on bit count). Based on Marc
Reynolds' work (public domain). Configurable precision for network/storage.

### Hash Functions (`Public/MashFunctions.h`)
"Mighty Micro Masher" — Thomas Wang 64-bit and 32-bit integer hash functions.
64-to-32 bit hash fold. Used throughout for skeleton key hashing.

### Modular Gameplay Tags (`Public/ModularGameplayTags.h`)
UE gameplay tag integration for keyed entities.

### Seq Library (`LibSeq/seq/`)
Third-party high-performance container library. Key components:
- `concurrent_map.hpp` — Lock-free concurrent ordered map. Used throughout
  SkeletonKey for key-to-kine lookups.
- `radix_map.hpp` / `radix_hash_map.hpp` — Radix-tree-based maps.
- `flat_map.hpp` — Sorted flat map (cache-friendly).
- `ordered_map.hpp` — Insertion-ordered map.
- `sequence.hpp` — Deque-like sequence container.
- `tiered_vector.hpp` — Tiered vector (stable references on growth).
- `devector.hpp` — Double-ended vector.
- `tiny_string.hpp` — Small-string-optimized string.
- `tagged_pointer.hpp` — Tagged pointer utilities.
- `hash.hpp` — Hash function utilities.
- Internal: `concurrent_hash_table.hpp`, `radix_tree.hpp`, SIMD helpers.
