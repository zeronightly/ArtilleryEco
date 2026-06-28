#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <unordered_map>
#include <memory>

#include "ArtilleryShell.h"
#include "SkeletonTypes.h"

// Forward declarations
class UBarrageDispatch;
class UArtilleryDispatch;

struct FArtilleryDataBuffer
{
    //relies on the JPH::StateRecorderImpl operator= added in
    //RollbackBundle/JoltPatches/StateRecorderImpl.h.patch.
    FArtilleryDataBuffer() = default;
    FArtilleryDataBuffer(const FArtilleryDataBuffer& Other) noexcept
        : SequenceNumber(Other.SequenceNumber),
          bIsValid(Other.bIsValid),
          Inputs(Other.Inputs),
          TimeStamp(Other.TimeStamp),
          bIsVerified(Other.bIsVerified)
    {
        PhysicsData.Clear();
        /* TODO(#3, DEFERRED per JMK 2026-06-25): copy the physics snapshot via Jolt's public
		   StateRecorderImpl read/restore stream API, NOT operator= (modern Jolt deletes it). Stubbed
		   to compile while #1/#2 land; PhysicsData is NOT yet copied here. We test both ways later. */
    }

    FArtilleryDataBuffer& operator=(const FArtilleryDataBuffer& Other) noexcept
    {
        if (this != &Other)
        {
            SequenceNumber = Other.SequenceNumber;
            bIsValid = Other.bIsValid;
            Inputs = Other.Inputs;
            /* TODO(#3, DEFERRED per JMK 2026-06-25): copy the physics snapshot via Jolt's public
		   StateRecorderImpl read/restore stream API, NOT operator= (modern Jolt deletes it). Stubbed
		   to compile while #1/#2 land; PhysicsData is NOT yet copied here. We test both ways later. */
            TimeStamp = Other.TimeStamp;
            bIsVerified = Other.bIsVerified;
        }
        return *this;
    }

    uint32_t SequenceNumber = 0;
    bool bIsValid = false;
    bool bIsVerified = false;
    ArtilleryTime TimeStamp = 0;
    TMap<PlayerKey, FArtilleryShell> Inputs;
    JPH::StateRecorderImpl PhysicsData;
    // Game-state snapshot lived here as a TArray<uint8> blob, never populated or
    // consumed. Removed; re-add a typed hook when an actual owner exists.
};

class FArtilleryStateManager
{
public:
    FArtilleryStateManager(uint32 InBufferSize = 40)
    {
        BufferSize = InBufferSize;
        Frames.Init(FArtilleryDataBuffer(), InBufferSize);
    };
    void Initialize(UWorld* World)
    {

    };
    void StoreFrame(uint32 FrameNumber, const FArtilleryDataBuffer& Data, bool bIsVerified = false)
    {
        if (FrameNumber >= OldestSequence + BufferSize) {
            OldestSequence = FrameNumber - BufferSize + 1;
        }

        uint32 Index = FrameNumber % BufferSize;
        Frames[Index] = Data;
        Frames[Index].SequenceNumber = FrameNumber;
        Frames[Index].bIsValid = true;
        Frames[Index].bIsVerified = bIsVerified;
        SequenceRange = FMath::Max(SequenceRange, FrameNumber - OldestSequence + 1);

        if (bIsVerified)
        {
            VerifiedFrame = Frames[Index];
            LastVerifiedFrameNumber = FrameNumber;
        }
        //do not auto-promote the first frame stored; "verified" means the server said so,
        //and until that happens RollbackToVerified should no-op.
    }

    FArtilleryDataBuffer* GetFrame(uint32 FrameNumber)
    {
        if (VerifiedFrame.bIsValid && VerifiedFrame.SequenceNumber == FrameNumber)
        {
            return &VerifiedFrame;
        }

        if (FrameNumber < OldestSequence || FrameNumber >= OldestSequence + SequenceRange)
        {
            return nullptr;
        }
        uint32 Index = FrameNumber % BufferSize;
        FArtilleryDataBuffer* Frame = &Frames[Index];
        return (Frame->bIsValid && Frame->SequenceNumber == FrameNumber) ? Frame : nullptr;
    }

    void StoreVerified(uint32 FrameNumber)
    {
        FArtilleryDataBuffer* VerifiedFramePtr = GetFrame(FrameNumber);
        if (ensure(VerifiedFramePtr))
        {
            VerifiedFramePtr->bIsVerified = true;
            VerifiedFramePtr->bIsValid = true;
            VerifiedFrame = *VerifiedFramePtr;
            LastVerifiedFrameNumber = FrameNumber;
        }
    }

    void Shutdown()
    {
        Frames.Empty();
        VerifiedFrame = FArtilleryDataBuffer();
        LastVerifiedFrameNumber = 0;
    }

    bool HasVerifiedFrame() const { return VerifiedFrame.bIsValid; }
    uint32 GetLastVerifiedSequence() const { return LastVerifiedFrameNumber; }

private:
    TArray<FArtilleryDataBuffer> Frames;
    FArtilleryDataBuffer VerifiedFrame;
    uint32 BufferSize = 20;
    uint32 OldestSequence = 0;
    uint32 SequenceRange = 0;
    uint32 LastVerifiedFrameNumber = 0;
};
