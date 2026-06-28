// Copyright Hedra Group.

#pragma once
#include "CoreMinimal.h"
#include "Curves/RichCurve.h"
#include "AimResponse.h"
#include <initializer_list>

// SampleAimProfiles — three starter profiles 
//
// Accumulated from our time in Arma via our own tinkering and OSS tools, these are:
//   - BuildSampleAimXProfile  — the headline aim-X LinRampZoom, three zooms
//   - BuildSampleAimYProfile  — companion aim-Y, flatter response
//   - BuildSampleMoveYProfile — a clean Gamma + deadzone example
//
// Some conventions did end up carrying over from the source data:
//   - Zoom 1.0 is the no-optic baseline. Smaller zoom values mean a longer
//     focal length (4x scope ≈ zoom 0.25). Less zoom needs less sensitivity
//     to feel right on screen, so the curves attenuate as zoom drops.
//   - LinRampZoom entries are authored descending by Zoom and selected
//     nearest-neighbor at runtime — see FAimCurveLinRampZoom.
//   - Each authored key uses linear interpolation. We set it explicitly
//     because FRichCurve defaults to cubic, which would put a wobble in
//     places the designer did not author one.
//   - Every ramp starts with an explicit (0, 0) key. The old format lerped
//     implicitly from (0, 0) up to the first stop; the new format wants you
//     to say so out loud.

namespace SampleAimProfiles
{

// Build an FRichCurve from a list of (X, Y) stops, prepending (0, 0) and
// setting every key to linear interpolation. The helper exists once so the
// per-profile builders below can read like the original .dat tables —
// the X-Y pairs match the source rows one to one.
[[nodiscard]] inline FRichCurve BuildLinearRamp(std::initializer_list<FVector2f> Stops)
{
	FRichCurve R;
	R.Keys.Reserve(static_cast<int32>(Stops.size()) + 1);

	FRichCurveKey ZeroKey;
	ZeroKey.Time = 0.0f;
	ZeroKey.Value = 0.0f;
	ZeroKey.InterpMode = RCIM_Linear;
	R.Keys.Add(ZeroKey);

	for (const FVector2f& Stop : Stops)
	{
		FRichCurveKey K;
		K.Time = Stop.X;
		K.Value = Stop.Y;
		K.InterpMode = RCIM_Linear;
		R.Keys.Add(K);
	}
	return R;
}

// ── Stock aim X ──────────────────────────────────────────────────────────
//
// Forgiving up to ~40% deflection (a quarter input gets you a tenth output),
// gentle middle from 40% to 80%, then the top fifth accelerates hard and
// the last percent punches. Three zoom entries (1.0, 0.7, 0.3). All three
// regimes (Low / Medium / High) get the same curve — the regime switch on
// this axis was authored as a no-op in the source data and we are honoring
// that, on the grounds that a designer who authored it three times meant it.
[[nodiscard]] inline FAimAxisProfile BuildSampleAimXProfile()
{
	FAimCurve Curve;
	Curve.Kind = EAimCurveKind::LinRampZoom;
	Curve.LinRampZoom.Entries =
	{
		{ 1.0f, BuildLinearRamp({ {0.40f, 0.10f}, {0.80f, 1.44f}, {0.99f, 2.88f}, {1.00f, 3.90f} }) },
		{ 0.7f, BuildLinearRamp({ {0.40f, 0.10f}, {0.80f, 1.20f}, {0.99f, 2.40f}, {1.00f, 3.25f} }) },
		{ 0.3f, BuildLinearRamp({ {0.40f, 0.10f}, {0.80f, 0.96f}, {0.99f, 1.44f}, {1.00f, 1.56f} }) },
	};

	FAimAxisProfile Profile;
	Profile.DeadZone        = 0.0f;
	Profile.CurveLow        = Curve;
	Profile.CurveMedium     = Curve;
	Profile.CurveHigh       = Curve;
	Profile.SmoothingLength = AimResponseTick::DefaultMouseFilterLength;
	return Profile;
}

// ── Stock aim Y ──────────────────────────────────────────────────────────
//
// Companion to the X profile. Three stops per zoom instead of four, and the
// whole response is flatter — vertical aim is for fine adjustment, lateral
// aim is for swinging onto target, and most shooters want them to feel like
// two different actions.
[[nodiscard]] inline FAimAxisProfile BuildSampleAimYProfile()
{
	FAimCurve Curve;
	Curve.Kind = EAimCurveKind::LinRampZoom;
	Curve.LinRampZoom.Entries =
	{
		{ 1.0f, BuildLinearRamp({ {0.20f, 0.05f}, {0.99f, 0.80f}, {1.00f, 1.80f} }) },
		{ 0.7f, BuildLinearRamp({ {0.20f, 0.03f}, {0.99f, 0.60f}, {1.00f, 1.60f} }) },
		{ 0.3f, BuildLinearRamp({ {0.20f, 0.02f}, {0.99f, 0.40f}, {1.00f, 1.00f} }) },
	};

	FAimAxisProfile Profile;
	Profile.DeadZone        = 0.0f;
	Profile.CurveLow        = Curve;
	Profile.CurveMedium     = Curve;
	Profile.CurveHigh       = Curve;
	Profile.SmoothingLength = AimResponseTick::DefaultMouseFilterLength;
	return Profile;
}

// ── Locomotion Y (forward / back) ────────────────────────────────────────
//
// A clean Gamma example — no ramps, no zoom. Gamma 4 means the lower half
// of stick deflection contributes very little to output, which is the right
// shape for walking: you do not break into a run because a thumb twitched.
// DeadZone 0.1 squashes the noise floor of cheap analog hardware before
// the gamma curve gets a chance to amplify it.
[[nodiscard]] inline FAimAxisProfile BuildSampleMoveYProfile()
{
	FAimCurve Curve;
	Curve.Kind = EAimCurveKind::Gamma;
	Curve.Gamma.MaxOutput = 1.0f;
	Curve.Gamma.Gamma     = 4.0f;

	FAimAxisProfile Profile;
	Profile.DeadZone        = 0.1f;
	Profile.CurveLow        = Curve;
	Profile.CurveMedium     = Curve;
	Profile.CurveHigh       = Curve;
	Profile.SmoothingLength = 0;
	return Profile;
}

} // namespace SampleAimProfiles
