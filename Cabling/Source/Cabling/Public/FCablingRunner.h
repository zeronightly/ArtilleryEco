// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <atomic>
#include "CablingCommonTypes.h"
#include "FStatefulPatternMatcher.h"
#include "HAL/Event.h"
#include "HAL/Runnable.h"
THIRD_PARTY_INCLUDES_START
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
// ReSharper disable once CppUnusedIncludeDirective - Required for above include
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#include <GameInput.h>
THIRD_PARTY_INCLUDES_END
#include "Containers/CircularQueue.h"

//why do it this way?
//well, unfortunately, input in UE runs through the event loop.
//at high sample rates, this can blow the event loop to hell, 
//and it's always going to be dependent on the gamethread.
//at best, it's a tidge slow and tick-rate dependent.
//at worst, it goes boom. 
//Cabling is NOT a general purpose or complete solution. It is a thin layer
//provided as a jumping off point for working in this space.
class FCabling : public FRunnable {
public:
	static constexpr uint64_t BlankKeyboard = ((((1000ULL << 11 | 1000ULL) << 11 | 1000ULL) << 11 | 1000ULL) << 20);
	using FlickBuffer = TSimpleInputRing<90>;
	
	FCabling();
	virtual ~FCabling() override;

	virtual bool Init() override;
	bool SendNew(bool sent,uint64_t priorReading, uint64_t currentRead);
	bool SendIfWindowEdge(bool sent, int seqNumber, uint64_t currentRead, uint32_t sendHertzFactor);
	uint64_t FromKeyboardAndMouseState(uint32_t keyCount, GameInputKeyState (&states)[16], const GameInputMouseButtons& mouseButtons, float MouseX, float MouseY);

	// Stateless mouse-look shaping: one poll's raw GameInput count delta -> normalized stick
	// deflection in [-1,1], linearly scaled by sensitivity. Holds no per-tick state. Angular
	// conversion, pitch clamping, and aim assist are intentionally downstream, not here.
	static float ShapeMouseAxisToDeflection(float Delta, double sensitivity);
	uint64_t CheckGamepadState(IGameInputReading* reading);

	uint64_t FromGamePadState(GameInputGamepadState state);
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;
	
	bool running;//cabling will let anyone unplug it. cabling is inanimate. cabling has no opinions on this.
	uint64_t GuessedInputCount;
	FlickBuffer RingForGamepadKeybinds;
	TSharedPtr<TCircularQueue<uint64_t>> GameThreadControlQueue;
	TSharedPtr<TCircularQueue<uint64_t>> CabledThreadControlQueue;
	FSharedEventRef WakeTransmitThread;
	
	bool   InvertMouseLookY     { false };
	bool MousePrimed = false;

private:
	void Cleanup();
};
