// NetworkDebugUI.h
#pragma once

#include "CoreMinimal.h"
#include "imgui.h"
#include "BristleconeCommonTypes.h"
#include "UBristleconeWorldSubsystem.h"
#include "UCablingWorldSubsystem.h"
#include "CanonicalInputStreamECS.h"
#include <deque>

struct FPacketDisplay
{
    uint64 Timestamp;
    uint32 Size;
    uint32 PlayerId;
    uint32 SequenceNumber;
    uint32 NetworkID;  // Added to track which network connection this packet belongs to
    float LatencyMs;
    bool IsIncoming;
};

// Structure to hold per-connection statistics
struct FConnectionStats
{
    uint64 TotalPacketsSent = 0;
    uint64 TotalPacketsReceived = 0;
    uint64 PacketsSentLastSecond = 0;
    uint64 PacketsReceivedLastSecond = 0;
    float AverageLatencyMs = 0.0f;
    float MinLatencyMs = 9999.0f;
    float MaxLatencyMs = 0.0f;
    uint64 DroppedPackets = 0;

    // Reset per-second counter stats
    void ResetSecondStats()
    {
        PacketsSentLastSecond = 0;
        PacketsReceivedLastSecond = 0;
        DroppedPackets = 0;
    }
};

// Statistics for overall network performance
struct FNetworkStats
{
    // Individual connection stats
    TMap<int32, FConnectionStats> ConnectionStats;

    // Overall stats
    uint64 TotalPacketsSent = 0;
    uint64 TotalPacketsReceived = 0;
    uint64 PacketsSentLastSecond = 0;
    uint64 PacketsReceivedLastSecond = 0;
    float AverageLatencyMs = 0.0f;
    float MinLatencyMs = 9999.0f;
    float MaxLatencyMs = 0.0f;
    uint64 DroppedPackets = 0;
    uint64 TotalInputsProcessed = 0;
    uint64 InputsProcessedLastSecond = 0;
    uint64 CurrentSequenceNumber = 0;

    // History for graphs
    std::deque<float> LatencyHistory;
    std::deque<float> PacketLossHistory;

    // Reset per-second counter stats
    void ResetSecondStats()
    {
        PacketsSentLastSecond = 0;
        PacketsReceivedLastSecond = 0;
        InputsProcessedLastSecond = 0;
        DroppedPackets = 0;

        // Reset per-connection stats
        for (auto& Pair : ConnectionStats)
        {
            Pair.Value.ResetSecondStats();
        }
    }

    // Update the overall stats from connection stats
    void UpdateFromConnectionStats()
    {
        TotalPacketsSent = 0;
        TotalPacketsReceived = 0;
        PacketsSentLastSecond = 0;
        PacketsReceivedLastSecond = 0;
        DroppedPackets = 0;

        // Sum up all connection stats
        for (const auto& Pair : ConnectionStats)
        {
            TotalPacketsSent += Pair.Value.TotalPacketsSent;
            TotalPacketsReceived += Pair.Value.TotalPacketsReceived;
            PacketsSentLastSecond += Pair.Value.PacketsSentLastSecond;
            PacketsReceivedLastSecond += Pair.Value.PacketsReceivedLastSecond;
            DroppedPackets += Pair.Value.DroppedPackets;
        }

        // Calculate weighted average for latency
        AverageLatencyMs = 0.0f;
        MinLatencyMs = 9999.0f;
        MaxLatencyMs = 0.0f;
        int connectionCount = 0;

        for (const auto& Pair : ConnectionStats)
        {
            if (Pair.Value.PacketsReceivedLastSecond > 0)
            {
                AverageLatencyMs += Pair.Value.AverageLatencyMs;
                MinLatencyMs = FMath::Min(MinLatencyMs, Pair.Value.MinLatencyMs);
                MaxLatencyMs = FMath::Max(MaxLatencyMs, Pair.Value.MaxLatencyMs);
                connectionCount++;
            }
        }

        if (connectionCount > 0)
        {
            AverageLatencyMs /= connectionCount;
        }
    }
};

class ArtilleryNetworkDebug
{
public:
    ArtilleryNetworkDebug();
    ~ArtilleryNetworkDebug();

    void Initialize(UBristleconeWorldSubsystem* InBristlecone, UCablingWorldSubsystem* InCabling, UCanonicalInputStreamECS* InInputECS);

    void Update(float DeltaTime);

    void Draw();

private:
    void DrawOverviewTab();
    void DrawPacketInspectorTab();
    void DrawLatencyGraphTab();
    void DrawStreamStateTab();
    void DrawPerPlayerStatsTab();

    void CollectPacketStats();
    void UpdateLatencyGraph();
    void CollectActiveStreams();

    ImVec4 GetHealthColor(float value, float good, float warning, float critical);

private:
    FNetworkStats Stats;
    float StatAccumTime = 0.0f;

    static const int32 MAX_PACKETS_DISPLAY = 100;
    TArray<FPacketDisplay> RecentPackets;

    TMap<int32, int32> StreamPacketCounts;
    TArray<FString> PlayerNames;
    TMap<int32, uint32> LastProcessedCounts; // Track per connection instead of a single value

    UBristleconeWorldSubsystem* Bristlecone = nullptr;
    UCablingWorldSubsystem* Cabling = nullptr;
    UCanonicalInputStreamECS* InputECS = nullptr;

    int32 CurrentTab = 0;
};
