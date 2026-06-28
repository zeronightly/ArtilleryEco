#pragma once
// Aim — Center-distance raycast hit resolution
// MIT Licensed. copyright Hg 2026.
// Standalone C++20. No engine dependencies. This is an example, and will be
// defactored into the broader locomo stack as we bring it into the game.
//
//
// Center-distance ranking picks the sphere whose center
// is closest, which is the body part you're actually aiming at.  This
// is what makes headshots possible when the head sphere sits inside
// the torso sphere's volume.
//
// On top of this hit resolution layer, an aim-assist system can add:
//   - Target acquisition (magnetism angle: sticky reticle toward targets)
//   - Autoaim (bullet deviation toward acquired target)
//   - Friction (slow turn rate near targets)
// Those are controller-input concerns.  The hit resolution underneath
// is the same for mouse and gamepad.
//
// This implementation provides:
//   1. Ray-sphere intersection test
//   2. Multi-sphere hit resolution (center-distance ranking)
//   3. Hierarchical body query (character = array of body-part spheres)
//   4. Scene query (ray vs all characters, returns best hit)
//   5. Penetrating scene query (ranking window extends past first surface)
//   6. Aim assist (magnetism + friction, optional layer)

#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <optional>

#include "Math/Ray.h"
#include "Math/Sphere.h"

namespace aim {



// ── Ray-sphere intersection ─────────────────────────────────────────────
//
// Returns the parametric t of the nearest entry point, or empty if miss.
// Standard geometric method: project sphere center onto ray, check
// discriminant.

    
inline std::optional<float> ray_sphere_intersect(const FRay3f ray, const FSphere3f sphere) {
    FVector3f oc = ray.Origin - sphere.Center;
    float b = oc.Dot(ray.Direction.GetSafeNormal());
    float c = oc.SquaredLength() - sphere.W * sphere.W;

    float disc = b * b - c;
    if (disc < 0.0f) return std::nullopt;

    float sqrt_disc = std::sqrt(disc);
    float t = -b - sqrt_disc;

    // If t < 0, ray origin is inside sphere — use far intersection
    if (t < 0.0f) {
        t = -b + sqrt_disc;
        if (t < 0.0f) return std::nullopt;  // sphere entirely behind ray
    }

    return t;
}

// ── Hit result ──────────────────────────────────────────────────────────
// this needs to be replaced with however you want to wire it up.
struct HitResult {
    uint32_t character_id = 0;       // which character was hit
    uint32_t part_index = 0;         // which body part sphere
    FVector3f intersection;               // point where ray enters the sphere
    float ray_dist_sq = 0.0f;        // distance² from ray line to sphere center
    float intersection_t = 0.0f;     // parametric t along ray
};

// ── Body: a character is an array of bounding spheres ────────────────────
// likewise, this is gonna be your set of spheres that represent the hit spaces of a body
// almost certainly, this will look quite different during wire up.
// Part 0 is conventionally the torso (largest), but the ranking doesn't
// care about order — it's determined entirely by center distance.

struct Body {
    uint32_t id = 0;
    const FSphere3f* parts = nullptr;
    uint32_t part_count = 0;
};

// ── Perpendicular distance from ray line to point ────────────────────────
//
// Project the point onto the ray, measure the rejection.

inline float ray_point_dist(const FRay3f& ray, const FVector3f& point) {
    FVector3f op = point - ray.Origin;
    float proj = op.Dot(ray.Direction.GetSafeNormal());
    FVector3f closest = ray.Origin + ray.Direction.GetSafeNormal() * proj;
    return (point - closest).Length();
}

// ── Hit ranking modes ────────────────────────────────────────────────────

enum class HitRank {
    // Perpendicular distance from ray line to sphere CENTER.
    // Size-neutral: doesn't care about sphere radius.
    // Picks the part whose center is most aligned with the aim.
    Center,
    
};

// ── Resolve: ray vs single body ──────────────────────────────────────────
//
// Tests every part sphere.  Among all intersected spheres, returns the
// one that ranks best by the chosen metric.
//
// Both modes are size-aware improvements over the naive approach of
// ranking by intersection-point distance (which has a size bias toward
// LARGE spheres — the opposite problem).
//
// Center mode:  rank by perpendicular distance to sphere center.
//               Size-neutral.  Picks the part you're aimed at.
//
// Surface mode: rank by (radius - center_perp_dist).  Anti-size-biased.
//               Smaller parts win close calls.  Headshots are stickier.
//
// Earlier versions used distance from ray origin to sphere center
// (point-to-point, not perpendicular), which works across objects at
// different depths but collapses for body parts at the same depth.
// Both modes here are clean generalizations of that insight.

inline std::optional<HitResult> resolve_body(const FRay3f& ray,
                                              const Body& body) {
    std::optional<HitResult> best;
    float best_score = std::numeric_limits<float>::max();

    for (uint32_t i = 0; i < body.part_count; ++i) {
        auto t = ray_sphere_intersect(ray, body.parts[i]);
        if (!t) continue;

        float score = ray_point_dist(ray, body.parts[i].Center);


        if (score < best_score) {
            best_score = score;
            HitResult hit;
            hit.character_id = body.id;
            hit.part_index = i;
            hit.intersection = ray.Origin + ray.Direction * (*t);
            hit.ray_dist_sq = score * score;
            hit.intersection_t = *t;
            best = hit;
        }
    }

    return best;
}

// ── Scene query: ray vs all bodies ───────────────────────────────────────
// this is a sketch and should be replaced during integration!
// Two-level resolution:
//   1. For each body, find the best-hit part (center-distance within body)
//   2. Among all bodies that were hit, pick the nearest by intersection
//      distance (parametric t) — you shoot the thing that's physically
//      in front, but you hit the right part ON that thing.
//
// This separation matters: within a single character, center-distance
// picks the aimed-at body part.  Across characters, intersection
// distance picks the one that's actually in the way.  You don't shoot
// through a closer enemy to headshot a farther one.

inline std::optional<HitResult> query_scene(const FRay3f& ray,
                                             const Body* bodies,
                                             uint32_t body_count) {
    std::optional<HitResult> best;
    float best_t = std::numeric_limits<float>::max();

    for (uint32_t i = 0; i < body_count; ++i) {
        auto hit = resolve_body(ray, bodies[i]);
        if (!hit) continue;

        if (hit->intersection_t < best_t) {
            best_t = hit->intersection_t;
            best = hit;
        }
    }

    return best;
}

// ── Penetrating scene query: ray vs all bodies with depth window ─────────
//
// When penetration_depth > 0, the hit scan doesn't stop at the first
// surface.  After finding the earliest intersection at t₀, ALL sphere
// hits within [t₀, t₀ + penetration_depth] are candidates.  Among those
// candidates, the best is chosen by center/surface ranking — same metric,
// wider window.
//
// This has two effects:
//
//   1. Shots that graze an obstacle can "see through" to body parts
//      behind it, because the obstacle's center is far from a grazing
//      ray.  The obstacle only fully wins when you're aimed at its
//      center.  This turns hard collision edges into soft ones.
//
//   2. Simple collision geometry (spheres) can approximate complex meshes.
//      A sphere overestimates at the edges; penetration papers over the
//      overshoot.  The sphere's rounded corners become passable.
//
// When penetration_depth == 0 this reduces to query_scene (first body
// wins by intersection_t, body-part ranking within that body only).
//
// The return value is the single best-ranked hit within the penetration
// window.  For multi-hit damage (hitting multiple targets with one
// bullet) things get more complex.

struct PenCandidate {
    float t;              // intersection parametric
    float score;          // ranking score (lower = better)
    HitResult hit;        // full hit details
};

inline std::optional<HitResult> query_scene_pen(const FRay3f& ray,
                                                 const Body* bodies,
                                                 uint32_t body_count,
                                                 float penetration_depth) {
    // Fast path: no penetration — fall back to two-level query
    if (penetration_depth <= 0.0f)
        return query_scene(ray, bodies, body_count);

    // Collect all sphere intersections across all bodies
    // Fixed-capacity: avoids allocation.  64 candidates covers any
    // reasonable scene query (8 bodies × 8 parts).
    constexpr uint32_t MAX_CANDIDATES = 64;
    PenCandidate candidates[MAX_CANDIDATES];
    uint32_t count = 0;
    float t_min = std::numeric_limits<float>::max();

    for (uint32_t bi = 0; bi < body_count; ++bi) {
        const Body& body = bodies[bi];
        for (uint32_t pi = 0; pi < body.part_count; ++pi) {
            auto t = ray_sphere_intersect(ray, body.parts[pi]);
            if (!t) continue;

            float score = ray_point_dist(ray, body.parts[pi].Center);
            HitResult hit;
            hit.character_id = body.id;
            hit.part_index = pi;
            hit.intersection = ray.Origin + ray.Direction * (*t);
            hit.ray_dist_sq = score * score;
            hit.intersection_t = *t;

            if (count < MAX_CANDIDATES) {
                candidates[count++] = { *t, score, hit };
            }
            if (*t < t_min) t_min = *t;
        }
    }

    if (count == 0) return std::nullopt;

    // Rank within penetration window [t_min, t_min + penetration_depth]
    float t_max = t_min + penetration_depth;
    std::optional<HitResult> best;
    float best_score = std::numeric_limits<float>::max();

    for (uint32_t i = 0; i < count; ++i) {
        if (candidates[i].t > t_max) continue;
        if (candidates[i].score < best_score) {
            best_score = candidates[i].score;
            best = candidates[i].hit;
        }
    }

    return best;
}

// ── Aim assist: magnetism + friction ─────────────────────────────────────
//
// This is loosely based on public info about a game you've probably heard of.
// It operates on the INPUT side — adjusting where the player is aiming —
// not on the hit resolution side.
//
// Magnetism: if a valid target's center is within magnetism_angle of the
// aim direction, bend the aim direction toward it by magnetism_strength.
//
// Friction: if a valid target's center is within friction_angle, scale
// the turn rate by friction_scale (< 1 = slower near targets).
//
// These are per-frame operations applied to the aim vector before the
// ray is cast.  The hit resolution (center-distance ranking) is
// unchanged.

struct AimAssistParams {
    float magnetism_angle = 0.0f;     // radians — cone half-angle for pull
    float magnetism_strength = 0.0f;  // 0..1 — interpolation factor toward target
    float friction_angle = 0.0f;      // radians — cone half-angle for slowdown
    float friction_scale = 1.0f;      // multiplier on turn rate (< 1 = sticky)
};

struct AimAssistTarget {
    FVector3f center;         // target center (e.g. torso or head)
    float radius = 0.0f; // target radius for acquisition
};

struct AimAssistResult {
    FVector3f aim_direction;     // adjusted aim direction (magnetism applied)
    float turn_scale;       // multiplier for turn rate this frame (friction)
    bool has_target;        // whether a target was acquired
    uint32_t target_index;  // index of acquired target
};

// Angle between two normalized vectors
inline float vec_angle(const FVector3f& a, const FVector3f& b) {
    float d = a.Dot(b);
    // Clamp for numerical safety
    if (d > 1.0f) d = 1.0f;
    if (d < -1.0f) d = -1.0f;
    return std::acos(d);
}

// ── Rotate-toward (slerp on the unit sphere) ─────────────────────────────
//
// Rotate `from` toward `to` by fraction t of the angle between them.
// Replaces the old component-wise lerp+normalize, which had two real
// failure modes: anti-parallel inputs at t=0.5 lerp through the origin and
// normalize to a ZERO vector (a dead aim direction), and t>1 overshot into
// undefined territory. Slerp gives constant angular rate, an always-unit
// result, and t is clamped to [0,1] so t=1 lands exactly on `to`.
//
// Anti-parallel inputs have no unique great circle. We rotate through a
// deterministic perpendicular — at exactly 180° every arc is equally
// correct, so picking one beats returning garbage.
//
// Both inputs must be unit length (same contract as vec_angle).

inline FVector3f rotate_toward(const FVector3f& from, const FVector3f& to, float t) {
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;

    const float theta = vec_angle(from, to);
    if (theta < 1.e-5f) return to;  // already aligned, nothing to rotate

    const float sin_theta = std::sin(theta);
    if (sin_theta > 1.e-4f) {
        const float wa = std::sin((1.0f - t) * theta) / sin_theta;
        const float wb = std::sin(t * theta) / sin_theta;
        // renormalize for float hygiene; inputs are unit so this is near-1 already
        return (from * wa + to * wb).GetSafeNormal();
    }

    // Anti-parallel: build a deterministic perpendicular to `from` and walk
    // the arc through it. The cardinal least aligned with `from` keeps the
    // rejection well-conditioned.
    const FVector3f cardinal = (std::abs(from.X) < 0.9f)
        ? FVector3f(1.0f, 0.0f, 0.0f)
        : FVector3f(0.0f, 1.0f, 0.0f);
    const FVector3f perp = (cardinal - from * from.Dot(cardinal)).GetSafeNormal();
    const float ang = t * theta;
    return (from * std::cos(ang) + perp * std::sin(ang)).GetSafeNormal();
}

inline AimAssistResult apply_aim_assist(const FVector3f& aim_dir,
                                         const FVector3f& aim_origin,
                                         const AimAssistTarget* targets,
                                         uint32_t target_count,
                                         const AimAssistParams& params) {
    AimAssistResult result;
    result.aim_direction = aim_dir;
    result.turn_scale = 1.0f;
    result.has_target = false;
    result.target_index = 0;

    if (target_count == 0) return result;

    // Find the closest target within the magnetism cone
    float best_angle = std::numeric_limits<float>::max();
    uint32_t best_idx = 0;
    FVector3f best_to_target = {};

    for (uint32_t i = 0; i < target_count; ++i) {
        FVector3f to_target = (targets[i].center - aim_origin).GetSafeNormal();
        float angle = vec_angle(aim_dir, to_target);

        // Use the wider of the two cones for acquisition
        float acquire_angle = std::max(params.magnetism_angle, params.friction_angle);
        if (angle < acquire_angle && angle < best_angle) {
            best_angle = angle;
            best_idx = i;
            best_to_target = to_target;
        }
    }

    if (best_angle >= std::max(params.magnetism_angle, params.friction_angle))
        return result;

    result.has_target = true;
    result.target_index = best_idx;

    // Friction: slow turn rate when target is in friction cone
    if (best_angle < params.friction_angle) {
        result.turn_scale = params.friction_scale;
    }

    // Magnetism: bend aim toward target center.
    // Slerp, not lerp — constant angular rate, unit-length result, no
    // zero-vector degeneracy on anti-parallel targets, and strength is
    // clamped inside rotate_toward so >1 cannot overshoot past the target.
    if (best_angle < params.magnetism_angle && params.magnetism_strength > 0.0f) {
        result.aim_direction = rotate_toward(aim_dir, best_to_target, params.magnetism_strength);
    }

    return result;
}

} // namespace aim
