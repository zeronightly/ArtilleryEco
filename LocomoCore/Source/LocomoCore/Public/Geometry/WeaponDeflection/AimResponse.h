// Copyright Hedra Group.

#pragma once
#include "CoreMinimal.h"
#include "LCM_Config.h"
#include "Curves/RichCurve.h"
#include "AimResponse.generated.h"

// AimResponse — per-axis aim-feel pipeline.
// raw → hysteresis → signed-deadzone normalize → regime curve → smoothing.
// Three regimes (Low/Medium/High) per axis; LinRampZoom additionally picks
// a per-zoom ramp at runtime. Curves own their own saturation; the pipeline
// does not clip magnitude. See SampleAimProfiles.h for authored examples.

// ── Tick ─────────────────────────────────────────────────────────────────

namespace AimResponseTick
{
	inline constexpr int32 DefaultMouseFilterLength = 5;
	inline constexpr int32 MaxSmoothingFilterLength = 32;
}

// ── Curves ───────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EAimCurveRegime : uint8
{
	Low,
	Medium,
	High,
};

UENUM(BlueprintType)
enum class EAimCurveKind : uint8
{
	Identity,
	Gamma,
	LinRamp,
	LinRampZoom,
};

USTRUCT(BlueprintType)
struct FAimCurveGamma
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Curve")
	float MaxOutput = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Curve")
	float Gamma = 1.0f;
};

// One ramp plus the zoom it was authored at. Selection is nearest-neighbor;
// the Zoom field is a label, not an interpolation target.
USTRUCT(BlueprintType)
struct FAimCurveLinRampZoomEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Curve")
	float Zoom = 1.0f;

	// Keys in [0, 1]. Add an explicit (0, 0) key for zero-start; FRichCurve's PreInfinity is constant.
	UPROPERTY(EditAnywhere, Category = "Aim|Curve")
	FRichCurve Ramp;
};

USTRUCT(BlueprintType)
struct FAimCurveLinRampZoom
{
	GENERATED_BODY()

	// Authored descending by Zoom; FBakedAimCurve::Eval selects nearest-neighbor at runtime.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Curve")
	TArray<FAimCurveLinRampZoomEntry> Entries;
};

USTRUCT(BlueprintType)
struct FAimCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Curve")
	EAimCurveKind Kind = EAimCurveKind::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Curve")
	FAimCurveGamma Gamma;

	// Add an explicit (0, 0) key for zero-start. Same convention as the per-zoom Ramp above.
	UPROPERTY(EditAnywhere, Category = "Aim|Curve")
	FRichCurve LinRamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Curve")
	FAimCurveLinRampZoom LinRampZoom;
};

// ── Axis profile ─────────────────────────────────────────────────────────

// Schmitt-trigger thresholds on raw magnitude. EngageAbove enters the engaged
// state; DisengageBelow leaves it. EngageAbove == 0 disables hysteresis.
USTRUCT(BlueprintType)
struct FAimHysteresisThresholds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Hysteresis")
	float EngageAbove = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Hysteresis")
	float DisengageBelow = 0.0f;
};

USTRUCT(BlueprintType)
struct FAimAxisProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	FAimHysteresisThresholds Hysteresis;

	// Magnitude below DeadZone squashes to zero; remainder rescaled so 1.0 at the boundary still maps to 1.0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	float DeadZone = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	FAimCurve CurveLow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	FAimCurve CurveMedium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	FAimCurve CurveHigh;

	// Rolling-average length on the curve output. 0 or 1 disables. Clamped to MaxSmoothingFilterLength.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	int32 SmoothingLength = 0;
};

// ── Baked runtime curve ──────────────────────────────────────────────────
// Authored FAimCurve bakes to flat LUT at Configure; Process reads the LUT.
// Same approach as FRecoilSampleTable.

struct FBakedAimRamp
{
	static constexpr int32 Resolution = 256;
	float Samples[Resolution] = {};

	void BakeFromRichCurve(const FRichCurve& Curve)
	{
		for (int32 I = 0; I < Resolution; ++I)
		{
			const float X = static_cast<float>(I) / static_cast<float>(Resolution - 1);
			Samples[I] = Curve.Eval(X, 0.0f);
		}
	}

	void BakeFromGamma(const FAimCurveGamma& G)
	{
		for (int32 I = 0; I < Resolution; ++I)
		{
			const float X = static_cast<float>(I) / static_cast<float>(Resolution - 1);
			const float Y = FMath::Pow(FMath::Max(X, 0.0f), G.Gamma) * G.MaxOutput;
			Samples[I] = FMath::Min(Y, G.MaxOutput);
		}
	}

	void BakeIdentity()
	{
		for (int32 I = 0; I < Resolution; ++I)
		{
			Samples[I] = static_cast<float>(I) / static_cast<float>(Resolution - 1);
		}
	}

	[[nodiscard]] UE_FORCEINLINE_HINT float Eval(float X) const
	{
		const float Clamped = FMath::Clamp(X, 0.0f, 1.0f);
		const float Scaled  = Clamped * static_cast<float>(Resolution - 1);
		const int32 I0      = static_cast<int32>(Scaled);
		const int32 I1      = FMath::Min(I0 + 1, Resolution - 1);
		const float Frac    = Scaled - static_cast<float>(I0);
		return FMath::Lerp(Samples[I0], Samples[I1], Frac);
	}
};

struct FBakedAimZoomEntry
{
	float Zoom = 1.0f;
	FBakedAimRamp Ramp;
};

struct FBakedAimCurve
{
	EAimCurveKind Kind = EAimCurveKind::Identity;
	FBakedAimRamp Ramp;
	TArray<FBakedAimZoomEntry> ZoomRamps;

	void BakeFrom(const FAimCurve& Source)
	{
		Kind = Source.Kind;
		ZoomRamps.Reset();
		switch (Source.Kind)
		{
		case EAimCurveKind::Identity:    Ramp.BakeIdentity(); break;
		case EAimCurveKind::Gamma:       Ramp.BakeFromGamma(Source.Gamma); break;
		case EAimCurveKind::LinRamp:     Ramp.BakeFromRichCurve(Source.LinRamp); break;
		case EAimCurveKind::LinRampZoom:
			ZoomRamps.Reserve(Source.LinRampZoom.Entries.Num());
			for (const FAimCurveLinRampZoomEntry& E : Source.LinRampZoom.Entries)
			{
				FBakedAimZoomEntry B;
				B.Zoom = E.Zoom;
				B.Ramp.BakeFromRichCurve(E.Ramp);
				ZoomRamps.Add(MoveTemp(B));
			}
			break;
		}
	}

	// Nearest-neighbor on Zoom for LinRampZoom; single ramp otherwise.
	[[nodiscard]] UE_FORCEINLINE_HINT float Eval(float X, float Zoom) const
	{
		if (Kind == EAimCurveKind::LinRampZoom)
		{
			if (ZoomRamps.Num() == 0) { return X; }
			float MinDist = TNumericLimits<float>::Max();
			int32 BestI = 0;
			for (int32 I = 0; I < ZoomRamps.Num(); ++I)
			{
				const float Dist = FMath::Abs(ZoomRamps[I].Zoom - Zoom);
				if (Dist < MinDist) { MinDist = Dist; BestI = I; }
				if (ZoomRamps[I].Zoom < Zoom) { break; }
			}
			return ZoomRamps[BestI].Ramp.Eval(X);
		}
		return Ramp.Eval(X);
	}
};

// ── Smoothing filter ─────────────────────────────────────────────────────

class FAimSmoothingFilter
{
public:
	void Configure(int32 InLength)
	{
		const int32 Clamped = FMath::Clamp(InLength, 0, AimResponseTick::MaxSmoothingFilterLength);
		if (Clamped != Length)
		{
			Length = Clamped;
			Reset();
		}
	}

	UE_FORCEINLINE_HINT float Push(float Sample)
	{
		if (Length <= 1) { return Sample; }
		if (Count < Length)
		{
			Ring[Head] = Sample;
			Sum += Sample;
			++Count;
		}
		else
		{
			Sum -= Ring[Head];
			Ring[Head] = Sample;
			Sum += Sample;
		}
		Head = (Head + 1) % Length;
		return Sum / static_cast<float>(Count);
	}

	void Reset()
	{
		for (int32 I = 0; I < AimResponseTick::MaxSmoothingFilterLength; ++I)
		{
			Ring[I] = 0.0f;
		}
		Head = 0;
		Count = 0;
		Sum = 0.0f;
	}

	[[nodiscard]] UE_FORCEINLINE_HINT int32 GetLength() const { return Length; }

private:
	float Ring[AimResponseTick::MaxSmoothingFilterLength] = {};
	int32 Length = 0;
	int32 Head = 0;
	int32 Count = 0;
	float Sum = 0.0f;
};

// ── Axis pipeline ────────────────────────────────────────────────────────

class FAimAxisPipeline
{
public:
	void Configure(const FAimAxisProfile& InProfile)
	{
		Profile = InProfile;
		BakedLow.BakeFrom(Profile.CurveLow);
		BakedMedium.BakeFrom(Profile.CurveMedium);
		BakedHigh.BakeFrom(Profile.CurveHigh);
		Smoothing.Configure(Profile.SmoothingLength);
		bEngaged = false;
	}

	// Hysteresis because real input is noisy.
	float Process(float Raw, EAimCurveRegime Regime, float Zoom)
	{
		const float Magnitude = FMath::Abs(Raw);

		if (Profile.Hysteresis.EngageAbove > 0.0f)
		{
			if (!bEngaged && Magnitude > Profile.Hysteresis.EngageAbove)
			{
				bEngaged = true;
			}
			else if (bEngaged && Magnitude < Profile.Hysteresis.DisengageBelow)
			{
				bEngaged = false;
			}
		}
		else
		{
			bEngaged = Magnitude > 0.0f;
		}

		if (!bEngaged)
		{
			// Bleed the smoother toward rest after release.
			return Smoothing.Push(0.0f);
		}

		float Norm = Magnitude;
		if (Profile.DeadZone > 0.0f)
		{
			if (Norm <= Profile.DeadZone)
			{
				return Smoothing.Push(0.0f);
			}
			Norm = (Norm - Profile.DeadZone) / (1.0f - Profile.DeadZone);
		}

		const FBakedAimCurve& Baked = (Regime == EAimCurveRegime::Low)    ? BakedLow
		                            : (Regime == EAimCurveRegime::Medium) ? BakedMedium
		                            :                                       BakedHigh;
		const float Shaped = Baked.Eval(Norm, Zoom);
		const float Signed = FMath::Sign(Raw) * Shaped;
		return Smoothing.Push(Signed);
	}

	void Reset()
	{
		Smoothing.Reset();
		bEngaged = false;
	}

	[[nodiscard]] UE_FORCEINLINE_HINT bool IsEngaged() const { return bEngaged; }
	[[nodiscard]] UE_FORCEINLINE_HINT const FAimAxisProfile& GetProfile() const { return Profile; }

private:
	FAimAxisProfile Profile;
	FAimSmoothingFilter Smoothing;
	FBakedAimCurve BakedLow;
	FBakedAimCurve BakedMedium;
	FBakedAimCurve BakedHigh;
	bool bEngaged = false;
};
