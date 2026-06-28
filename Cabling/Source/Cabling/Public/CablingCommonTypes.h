// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/CircularQueue.h"
#include <cstdint>

//centralizing the typedefs to avoid circularized header includes
//and further ease swapping over between 8 and 16 byte modes. IWYU!
namespace Cabling
{
	typedef uint64_t PacketElement;
	typedef std::pair<uint32_t, long> CycleTimestamp;
	typedef TCircularQueue<CycleTimestamp> TimestampQ;
	typedef TSharedPtr<TimestampQ, ESPMode::ThreadSafe> TimestampQueue;
	typedef TSharedPtr<TCircularQueue<PacketElement>, ESPMode::ThreadSafe> SendQueue; // note that the queues only support 1p1c mode.
	constexpr uint32_t LongboySendHertz = 128;
	constexpr uint32_t CablingSampleHertz = 512;
	constexpr uint32_t BristleconeSendHertz = 128;

	// Mouse-look tuning (see FCabling::ShapeMouseAxisToDeflection).
	// DefaultMouseSensitivity: linear multiplier on raw mouse counts; a player setting overrides it.
	// MouseCountsToSaturation: mouse counts in one poll that reach full stick deflection
	constexpr double DefaultMouseSensitivityX = .92;
	constexpr double DefaultMouseSensitivityY = .53;
	constexpr double MouseTicksToSaturation = 6.0;
}
