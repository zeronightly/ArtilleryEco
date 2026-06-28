// Copyright Hedra Group.

#pragma once
#include "CoreMinimal.h"
#include "LCM_Config.h"
#include "Math/Quat.h"
#include "Misc/Optional.h"
#include "TurretRig.generated.h"

// TurretRig — rate-limited two-axis position controller plus optical pitch,
// with optional hull stabilization.
//
// Control loop per axis, per tick:
//   delta_wanted  = clamp(Input * Gains.InputToWantedGain, ±Gains.MaxWantedDeltaRad)
//   wanted        = wrap_to_arc(wanted + delta_wanted, limits)
//   commanded_vel = (wanted - current) * Gains.ConvergenceGain
//   speed        += clamp(commanded_vel - speed, ±AccelPerTick)
//   speed         = clamp(speed, ±MaxSpeed)
//   current      += speed * TickDt
//   current       = wrap_to_arc(current, limits)
//
// Drive via ApplyInput (stick), SetWantedAbsolute (direct write), or
// SetWantedPitchOverride + AutoFromHost (ballistic-computer hook). Call
// Tick() once per fixed tick. Stabilize(hull_quat) before Tick if the hull rotates.

// ── Modes ────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ETurretStabilizationAxes : uint8
{
	None,
	PitchOnly,
	YawOnly,
	PitchAndYaw,
};

UENUM(BlueprintType)
enum class ETurretElevationMode : uint8
{
	// Pitch follows WantedPitchOverride (host-set). Stick still drives yaw and CamPitch.
	AutoFromHost,
	// Pitch driven by stick input.
	Manual,
};

// ── Per-axis parameters ──────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FTurretAxisLimits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Limits")
	float MinRad = -PI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Limits")
	float MaxRad = PI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Limits")
	float InitialRad = 0.0f;

	// Anything narrower than 2π needs the wrap step.
	[[nodiscard]] UE_FORCEINLINE_HINT bool IsConstrainedArc() const { return (MaxRad - MinRad) < (2.0f * PI); }
};

USTRUCT(BlueprintType)
struct FTurretAxisRates
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Rates")
	float MaxSpeedRadPerSec = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Rates")
	float MaxAccelRadPerSec2 = 3.0f;
};

USTRUCT(BlueprintType)
struct FTurretControlGains
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Gains")
	float InputToWantedGain = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Gains")
	float MaxWantedDeltaRad = 0.45f * (PI * 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Gains")
	float ConvergenceGain = 4.0f;
};

// ── Rig config ───────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FTurretConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretAxisLimits PitchLimits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretAxisLimits YawLimits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretAxisLimits CamPitchLimits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretAxisRates PitchRates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretAxisRates YawRates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretAxisRates CamPitchRates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretControlGains PitchGains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretControlGains YawGains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	FTurretControlGains CamPitchGains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	ETurretStabilizationAxes Stabilization = ETurretStabilizationAxes::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	ETurretElevationMode ElevationMode = ETurretElevationMode::Manual;

	// Servo audio knobs. ServoVol is a slewed envelope in [0, 1] tracking
	// whether any axis is meaningfully moving; ServoActiveSpeed is the
	// commanded-speed threshold at which we call "any axis" any axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Servo")
	float ServoVolRatePerSec = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Servo")
	float ServoActiveSpeed = 0.01f;
};

// ── Runtime input + state ────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FTurretInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Turret|Input")
	float Pitch = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Turret|Input")
	float Yaw = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Turret|Input")
	float CamPitch = 0.0f;
};

USTRUCT(BlueprintType)
struct FTurretAxisState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Turret|State")
	float Current = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Turret|State")
	float Wanted = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Turret|State")
	float Speed = 0.0f;
};

// ── Rig ──────────────────────────────────────────────────────────────────
//
// FTurretRig is deliberately not a USTRUCT — it holds a TOptional<float> and
// a frame quaternion that does not want to survive serialization, and the
// pair would be ugly to reflect without rewriting the public surface. The
// configs above are USTRUCT; the rig that consumes them is a runtime object.

class FTurretRig
{
public:
	void Configure(const FTurretConfig& InConfig)
	{
		Config = InConfig;

		Pitch    = { Config.PitchLimits.InitialRad,    Config.PitchLimits.InitialRad,    0.0f };
		Yaw      = { Config.YawLimits.InitialRad,      Config.YawLimits.InitialRad,      0.0f };
		CamPitch = { Config.CamPitchLimits.InitialRad, Config.CamPitchLimits.InitialRad, 0.0f };

		PitchAccelPerTick    = Config.PitchRates.MaxAccelRadPerSec2    * UWorldAwareCommons::Dt;
		YawAccelPerTick      = Config.YawRates.MaxAccelRadPerSec2      * UWorldAwareCommons::Dt;
		CamPitchAccelPerTick = Config.CamPitchRates.MaxAccelRadPerSec2 * UWorldAwareCommons::Dt;

		ServoVol = 0.0f;
		ServoVolDeltaPerTick = Config.ServoVolRatePerSec * UWorldAwareCommons::Dt;

		StabilizeFrame = FQuat::Identity;
		bStabilizeFrameSeeded = false;
		WantedPitchOverride.Reset();
	}

	void ApplyInput(const FTurretInput& Input)
	{
		const auto Drive = [](FTurretAxisState& S, const FTurretAxisLimits& L, const FTurretControlGains& G, float Stick)
		{
			const float Raw = Stick * G.InputToWantedGain;
			const float Delta = FMath::Clamp(Raw, -G.MaxWantedDeltaRad, G.MaxWantedDeltaRad);
			S.Wanted = ClampToArc(S.Wanted + Delta, L);
		};

		// In AutoFromHost mode with an override set, pitch belongs to the
		// host for this tick — yaw and the optic still take their nudge.
		if (Config.ElevationMode == ETurretElevationMode::Manual || !WantedPitchOverride.IsSet())
		{
			Drive(Pitch, Config.PitchLimits, Config.PitchGains, Input.Pitch);
		}
		Drive(Yaw, Config.YawLimits, Config.YawGains, Input.Yaw);
		Drive(CamPitch, Config.CamPitchLimits, Config.CamPitchGains, Input.CamPitch);
	}

	void SetWantedPitchOverride(TOptional<float> WantedPitchRad)
	{
		WantedPitchOverride = WantedPitchRad;
	}

	void SetWantedAbsolute(float PitchRad, float YawRad, float CamPitchRad)
	{
		Pitch.Wanted    = ClampToArc(PitchRad,    Config.PitchLimits);
		Yaw.Wanted      = ClampToArc(YawRad,      Config.YawLimits);
		CamPitch.Wanted = ClampToArc(CamPitchRad, Config.CamPitchLimits);
	}

	// Hold the gun on its current world heading while the hull rotates under
	// it. We do not stabilize on the first call — that just seeds the frame
	// — because at t=0 there is no previous hull to be relative to.
	void Stabilize(const FQuat& HullOrientation)
	{
		if (!bStabilizeFrameSeeded)
		{
			StabilizeFrame = HullOrientation;
			bStabilizeFrameSeeded = true;
			return;
		}

		const bool bStabPitch =
			Config.Stabilization == ETurretStabilizationAxes::PitchOnly ||
			Config.Stabilization == ETurretStabilizationAxes::PitchAndYaw;
		const bool bStabYaw =
			Config.Stabilization == ETurretStabilizationAxes::YawOnly ||
			Config.Stabilization == ETurretStabilizationAxes::PitchAndYaw;

		if (!bStabPitch && !bStabYaw)
		{
			// Stabilization is off, but the frame still needs to track the
			// hull for whenever someone turns it back on.
			StabilizeFrame = HullOrientation;
			return;
		}

		// Express the gun's current local direction in world via the old
		// hull, then re-express in the new hull and recover the (pitch, yaw)
		// that keep the gun on the same world point. Both axes shift in
		// step because the heading is the joint product — adjust only one
		// and you aim somewhere new entirely.
		const FRotator LocalRot(FMath::RadiansToDegrees(Pitch.Current),
		                        FMath::RadiansToDegrees(Yaw.Current),
		                        0.0f);
		const FVector LocalDir = LocalRot.Vector();
		const FVector WorldDir = StabilizeFrame.RotateVector(LocalDir);
		const FVector NewLocalDir = HullOrientation.Inverse().RotateVector(WorldDir);
		const FRotator NewLocal = NewLocalDir.Rotation();
		const float NewPitchRad = FMath::DegreesToRadians(NewLocal.Pitch);
		const float NewYawRad   = FMath::DegreesToRadians(NewLocal.Yaw);

		if (bStabPitch)
		{
			const float DeltaPitch = FMath::UnwindRadians(NewPitchRad - Pitch.Current);
			Pitch.Current = ClampToArc(Pitch.Current + DeltaPitch, Config.PitchLimits);
			Pitch.Wanted  = ClampToArc(Pitch.Wanted  + DeltaPitch, Config.PitchLimits);
		}
		if (bStabYaw)
		{
			const float DeltaYaw = FMath::UnwindRadians(NewYawRad - Yaw.Current);
			Yaw.Current = ClampToArc(Yaw.Current + DeltaYaw, Config.YawLimits);
			Yaw.Wanted  = ClampToArc(Yaw.Wanted  + DeltaYaw, Config.YawLimits);
		}

		StabilizeFrame = HullOrientation;
	}

	// Returns true if any axis moved past the servo-active threshold this tick.
	bool Tick()
	{
		if (Config.ElevationMode == ETurretElevationMode::AutoFromHost && WantedPitchOverride.IsSet())
		{
			Pitch.Wanted = ClampToArc(*WantedPitchOverride, Config.PitchLimits);
		}

		float MaxCmd = 0.0f;
		MaxCmd = FMath::Max(MaxCmd, StepAxis(Pitch,    Config.PitchLimits,    Config.PitchRates,    Config.PitchGains,    PitchAccelPerTick,    false));
		MaxCmd = FMath::Max(MaxCmd, StepAxis(Yaw,      Config.YawLimits,      Config.YawRates,      Config.YawGains,      YawAccelPerTick,      true));
		MaxCmd = FMath::Max(MaxCmd, StepAxis(CamPitch, Config.CamPitchLimits, Config.CamPitchRates, Config.CamPitchGains, CamPitchAccelPerTick, false));

		// Slew the servo audio envelope toward 1 while at least one axis is
		// asking for non-trivial motion, and back toward 0 when nothing is.
		// Audio reads the smooth envelope and never has to know which way
		// the gun moved this tick — only that it did.
		const float ServoTarget = MaxCmd > Config.ServoActiveSpeed ? 1.0f : 0.0f;
		const float ServoDelta = FMath::Clamp(ServoTarget - ServoVol, -ServoVolDeltaPerTick, ServoVolDeltaPerTick);
		ServoVol += ServoDelta;

		return MaxCmd > Config.ServoActiveSpeed;
	}

	void ResetToInitial()
	{
		Pitch.Current = Pitch.Wanted = Config.PitchLimits.InitialRad;
		Yaw.Current = Yaw.Wanted = Config.YawLimits.InitialRad;
		CamPitch.Current = CamPitch.Wanted = Config.CamPitchLimits.InitialRad;
		Pitch.Speed = Yaw.Speed = CamPitch.Speed = 0.0f;
		ServoVol = 0.0f;
		StabilizeFrame = FQuat::Identity;
		bStabilizeFrameSeeded = false;
		WantedPitchOverride.Reset();
	}

	[[nodiscard]] UE_FORCEINLINE_HINT float GetPitch() const    { return Pitch.Current; }
	[[nodiscard]] UE_FORCEINLINE_HINT float GetYaw() const      { return Yaw.Current; }
	[[nodiscard]] UE_FORCEINLINE_HINT float GetCamPitch() const { return CamPitch.Current; }

	[[nodiscard]] UE_FORCEINLINE_HINT float GetPitchSpeed() const    { return Pitch.Speed; }
	[[nodiscard]] UE_FORCEINLINE_HINT float GetYawSpeed() const      { return Yaw.Speed; }
	[[nodiscard]] UE_FORCEINLINE_HINT float GetCamPitchSpeed() const { return CamPitch.Speed; }

	[[nodiscard]] UE_FORCEINLINE_HINT float GetWantedPitch() const    { return Pitch.Wanted; }
	[[nodiscard]] UE_FORCEINLINE_HINT float GetWantedYaw() const      { return Yaw.Wanted; }
	[[nodiscard]] UE_FORCEINLINE_HINT float GetWantedCamPitch() const { return CamPitch.Wanted; }

	[[nodiscard]] UE_FORCEINLINE_HINT float GetServoVolume() const { return ServoVol; }

	[[nodiscard]] UE_FORCEINLINE_HINT const FTurretConfig& GetConfig() const { return Config; }

private:
	// Wrap an angle into the arc midpoint first when the arc is constrained,
	// then clamp to [min, max]. Wrapping around the midpoint means a half-
	// turret can sweep across its dead zone without doing a full revolution
	// — turrets that need to spin five thousand degrees to reach the
	// opposite quadrant are the kind you see in cartoons.
	static UE_FORCEINLINE_HINT float ClampToArc(float Angle, const FTurretAxisLimits& Limits)
	{
		if (Limits.IsConstrainedArc())
		{
			const float Mid = (Limits.MinRad + Limits.MaxRad) * 0.5f;
			Angle = Mid + FMath::UnwindRadians(Angle - Mid);
		}
		return FMath::Clamp(Angle, Limits.MinRad, Limits.MaxRad);
	}

	// One acceleration-limited convergence step. Returns |commanded_speed| so
	// the caller can drive the servo signal off the loudest axis.
	// This is also pretty directly inspired by Arma turrets and taping a protractor to my screen,
	// but also by this video of the B27
	//https://www.youtube.com/watch?v=QKRszjV07ZQ 
	static float StepAxis(FTurretAxisState& S,
	                      const FTurretAxisLimits& L,
	                      const FTurretAxisRates& R,
	                      const FTurretControlGains& G,
	                      float AccelPerTick,
	                      bool bUseAngleDifference)
	{
		const float Error = bUseAngleDifference
			? FMath::UnwindRadians(S.Wanted - S.Current)
			: (S.Wanted - S.Current);

		const float Cmd = Error * G.ConvergenceGain;
		const float dSpeed = FMath::Clamp(Cmd - S.Speed, -AccelPerTick, AccelPerTick);
		S.Speed = FMath::Clamp(S.Speed + dSpeed, -R.MaxSpeedRadPerSec, R.MaxSpeedRadPerSec);
		S.Current += S.Speed * UWorldAwareCommons::Dt;
		S.Current = ClampToArc(S.Current, L);
		return FMath::Abs(Cmd);
	}

	FTurretConfig Config;

	FTurretAxisState Pitch;
	FTurretAxisState Yaw;
	FTurretAxisState CamPitch;

	float PitchAccelPerTick = 0.0f;
	float YawAccelPerTick = 0.0f;
	float CamPitchAccelPerTick = 0.0f;

	float ServoVol = 0.0f;
	float ServoVolDeltaPerTick = 0.0f;

	FQuat StabilizeFrame = FQuat::Identity;
	bool bStabilizeFrameSeeded = false;

	TOptional<float> WantedPitchOverride;
};
