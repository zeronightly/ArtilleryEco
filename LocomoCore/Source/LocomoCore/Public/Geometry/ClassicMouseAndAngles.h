#pragma once
// =============================================================================
// id-ish controls and angle handling.
//
#include <cmath>
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"

#include "ClassicMouseAndAngles.generated.h"


//in the original designs (quake, source, etc) you would decompose and clamp 
//your mouse and wasd inputs into actually "directionals" - we do this differently
//because that clamping is built into our fixed bit input\output format OR
//is done at the locomotion state machine level, far above this code.
USTRUCT()
struct  FClassicoControls
{
	GENERATED_BODY()
	
#define _NAY_SIR false
	using byte = unsigned char;
	using vec_t = float;
	using vec3_t = vec_t[3];

	//I have to check this every time.... I think this is wrong? But I'll know soon lol.
	enum { PITCH = 0, YAW = 1, ROLL = 2 };
	
	// for the full classic feel, you'll almost certainly want to first crunch everything through this
	// as the quantization actually is grainy enough to feel. we don't do it right now, but it is here.
	inline int ANGLE2SHORT(float x) { return (int)(x * 65536 / 360) & 65535; }
	inline double SHORT2ANGLE(int x) { return x * (360.0 / 65536); }


public: //data members
	
	
	UPROPERTY()
	bool m_filter = false; // m_filter (2-tap box smoothing)
	
	UPROPERTY()
	float NudgeAngle = 0.1;//this is why your angle measures need to be written in hertz form.
	
	UPROPERTY()
	float HighSpeedAngle = 1.5f; 
	
	//thiiiiiiiiiiis is kinda poisonous to how we think about some stuff, because it MUST be done before stuff is encoded into
	//the artillery input package. So it must happen in cabling. But cabling is kind of explicitly designed to not know about FOV
	//it's factored out here for the time being and fixed at 1.0
	//TODO: fov scaling
	UPROPERTY()
	float FOVSensitivityScaling = 1.0f;
	
	inline static const double HERTZ = 1/128;
	
	inline static const double framePeriodMs = 1000.0 * HERTZ; // fixed poll period
	
	UPROPERTY()
	double MouseBaseSensitivity = 1.1; // 1.0 == prior feel.
	
	UPROPERTY()
	 double MouseAccelCoeff      = 0.2; // set > 0 to enable acceleration.
	
	UPROPERTY()
	 int MouseCountsToSaturation = 6;


	double MouseAccelSensitivity(float countDeltaX, float countDeltaY)
	{
		const double rate = FMath::Sqrt(static_cast<double>(countDeltaX) * countDeltaX
									  + static_cast<double>(countDeltaY) * countDeltaY) / framePeriodMs;
		 
		return  (MouseBaseSensitivity + (rate * MouseAccelCoeff)) * FOVSensitivityScaling;
		
	}

	//Can be used to directly replace shaping in FCabling
	float ShapeMouseAxisToDeflection(float AxisDelta, float PriorAxisDelta, double perAxisSensitivity, double accelSensitivity)
	{
		
		if (AxisDelta == 0.0f || perAxisSensitivity <= 0.0)
		{
			return 0.0f;
		}
		// allow mouse smoothing. I believe we should probably transform via angle to short, but I'm hoping for an isometry.
		if (m_filter)
		{
			AxisDelta = (AxisDelta + PriorAxisDelta) * 0.5;
		}
		
		const double magnitude = FMath::Min(
	   FMath::Abs(static_cast<double>(AxisDelta)) * accelSensitivity * perAxisSensitivity / MouseCountsToSaturation,
			1.0);
		return static_cast<float>(AxisDelta > 0.0f ? magnitude : -magnitude);
	}

	//Probable call pattern in cabling.
	//					const double accelSensitivity = MouseAccelSensitivity(delX, delY);
	//					MouseXDelta = ShapeMouseAxisToDeflection(delX,  Cabling::DefaultMouseSensitivityX, accelSensitivity);
	//					MouseYDelta = -ShapeMouseAxisToDeflection(delY, Cabling::DefaultMouseSensitivityY, accelSensitivity);
	// if (DostThouEvenStrafe)
	// {
	// 	//so we use the nudge
	// 	MouseXDelta -=  (MouseXDelta+MouseYDelta) * 0.5 * NudgeAngle; //a real speedster would half the nudge angle rate instead of doing two muls.
	// 	//but I'm just a home cook now.
	// }

};
