// Copyright Hedra Group.

#pragma once
#include "CoreMinimal.h"
#include "LCM_Config.h"
#include "Curves/RichCurve.h"
#include "RecoilCurve.generated.h"

// RecoilCurve — piecewise recoil curves baked to a per-tick LUT, plus a
// per-weapon saturating sway envelope.
// Two FRichCurves per weapon (pitch velocity, muzzle-Z) bake into
// FRecoilSampleTable at load. FRecoilState walks the LUT after OnFire().
// FRecoilSwayEnvelope is the orthogonal cone-widening signal; host folds.

// ── Curve config ─────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FRecoilCurveConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	FName Name;

	// rad/s. Bake integrates across one tick for the per-tick pitch increment.
	UPROPERTY(EditAnywhere, Category = "Recoil")
	FRichCurve PitchVelocityOverTime;

	// World units, sampled directly by the bake; host adds to the view.
	UPROPERTY(EditAnywhere, Category = "Recoil")
	FRichCurve OffsetZOverTime;

	// Yaw kick per pitch kick. Negative — leans off the dominant-hand side.
	static constexpr float LateralKickRatio = -0.33f;
};

USTRUCT(BlueprintType)
struct FRecoilTickSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recoil")
	float PitchDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recoil")
	float YawDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recoil")
	float OffsetZ = 0.0f;
};

// ── Sway config ──────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FRecoilSwayConfig
{
	GENERATED_BODY()

	// Hard cap on accumulated sway.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float MaxSwayRad = 0.008f;

	// 1 / seconds-to-zero from MaxSwayRad. Linear decay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float DecaySpeed = 2.0f;

	// Knobs read by the host (locomotion, dispersion); the envelope itself does not look at them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float OneHandTimeToTraverse = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float TwoHandTimeToTraverse = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float DispersionBaseline = 0.0018f;
};

// ── Baked sample table ───────────────────────────────────────────────────

class FRecoilSampleTable
{
public:
	// Sample the curves at LocomoTick::Dt intervals out to the pitch curve's last keyframe.
	// Pitch stored as integrated per-tick increment; yaw derived from pitch via LateralKickRatio.
	void BakeFrom(const FRecoilCurveConfig& Config)
	{
		Name = Config.Name;
		Samples.Reset();

		float MinTime = 0.0f;
		float MaxTime = 0.0f;
		Config.PitchVelocityOverTime.GetTimeRange(MinTime, MaxTime);
		if (MaxTime <= 0.0f) { return; }

		const int32 NumTicksToBake = FMath::CeilToInt(MaxTime * static_cast<float>(UWorldAwareCommons::Hz));
		if (NumTicksToBake <= 0) { return; }

		Samples.Reserve(NumTicksToBake);
		for (int32 Tick = 0; Tick < NumTicksToBake; ++Tick)
		{
			const float SampleTime = static_cast<float>(Tick) * UWorldAwareCommons::Dt;

			FRecoilTickSample S;
			const float PitchVel = Config.PitchVelocityOverTime.Eval(SampleTime, 0.0f);
			S.PitchDelta = PitchVel * UWorldAwareCommons::Dt;
			S.YawDelta   = S.PitchDelta * FRecoilCurveConfig::LateralKickRatio;
			S.OffsetZ    = Config.OffsetZOverTime.Eval(SampleTime, 0.0f);
			Samples.Add(S);
		}
	}

	[[nodiscard]] UE_FORCEINLINE_HINT int32 NumTicks() const { return Samples.Num(); }
	[[nodiscard]] UE_FORCEINLINE_HINT const FName& GetName() const { return Name; }

	[[nodiscard]] UE_FORCEINLINE_HINT FRecoilTickSample SampleAt(int32 TickSinceFire) const
	{
		if (TickSinceFire < 0 || TickSinceFire >= Samples.Num())
		{
			return FRecoilTickSample{};
		}
		return Samples[TickSinceFire];
	}

private:
	FName Name;
	TArray<FRecoilTickSample> Samples;
};

// ── Per-fire runtime state ───────────────────────────────────────────────

// TSharedPtr so the table's owning container can reallocate without dangling.
class FRecoilState
{
public:
	void OnFire(TSharedPtr<const FRecoilSampleTable> Table)
	{
		Active = MoveTemp(Table);
		TicksSinceFire = 0;
	}

	FRecoilTickSample Tick(float GlobalCoef = 1.0f)
	{
		if (!Active.IsValid()) { return FRecoilTickSample{}; }

		FRecoilTickSample S = Active->SampleAt(TicksSinceFire);
		S.PitchDelta *= GlobalCoef;
		S.YawDelta   *= GlobalCoef;
		S.OffsetZ    *= GlobalCoef;

		++TicksSinceFire;
		if (TicksSinceFire >= Active->NumTicks())
		{
			Active.Reset();
		}
		return S;
	}

	[[nodiscard]] UE_FORCEINLINE_HINT bool IsActive() const { return Active.IsValid(); }
	[[nodiscard]] UE_FORCEINLINE_HINT int32 GetTicksSinceFire() const { return TicksSinceFire; }

private:
	TSharedPtr<const FRecoilSampleTable> Active;
	int32 TicksSinceFire = 0;
};

// ── Sway envelope ────────────────────────────────────────────────────────

class FRecoilSwayEnvelope
{
public:
	void Configure(const FRecoilSwayConfig& Config)
	{
		MaxSway      = Config.MaxSwayRad;
		DecayPerTick = Config.MaxSwayRad * Config.DecaySpeed * UWorldAwareCommons::Dt;
		// Clip live sway down if the ceiling has lowered.
		Sway = FMath::Min(Sway, MaxSway);
	}

	UE_FORCEINLINE_HINT void OnFire(float Amount) { Sway = FMath::Min(Sway + Amount, MaxSway); }
	UE_FORCEINLINE_HINT void Tick()               { Sway = FMath::Max(Sway - DecayPerTick, 0.0f); }

	[[nodiscard]] UE_FORCEINLINE_HINT float CurrentRad() const { return Sway; }
	void Reset() { Sway = 0.0f; }

private:
	float Sway = 0.0f;
	float MaxSway = 0.008f;
	float DecayPerTick = 0.0f;
};
