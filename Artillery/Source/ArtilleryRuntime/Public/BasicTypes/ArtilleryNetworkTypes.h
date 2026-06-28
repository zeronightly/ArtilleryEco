#pragma once
#include "ArtilleryCommonTypes.h"
#include "BristleconeCommonTypes.h"
#include "FBristleconePacket.h"

// FArtilleryPacket (+ its PacketRegistry RegisterType) was REMOVED in the Phase-1 merge.
// Modern transport uses the flat FBristleconePacket (no polymorphic serializer, no PacketRegistry),
// and the Longboy reflector handles relay. The modern flat-packet path lives in
// ArtilleryInputManager (ParseAuthorityData / SendInputsToServer). File kept with its includes so
// existing #includes resolve. -- midnight agent, JMK direction 2026-06-25 ("modernflatpack").
