#include "NetworkDebugger.h"

#include "ImGuiContext.h"
#include "NetImgui_Api.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"

ArtilleryNetworkDebug::ArtilleryNetworkDebug()
{
    for (int i = 0; i < 120; ++i)
    {
        Stats.LatencyHistory.push_back(0);
        Stats.PacketLossHistory.push_back(0);
    }

    PlayerNames.Add("CABLE");
    PlayerNames.Add("ECHO");
    PlayerNames.Add("TWO");
    PlayerNames.Add("THREE");
    PlayerNames.Add("FOUR");
    PlayerNames.Add("FIVE");
    PlayerNames.Add("SIX");
}

ArtilleryNetworkDebug::~ArtilleryNetworkDebug()
{
}

void ArtilleryNetworkDebug::Initialize(UBristleconeWorldSubsystem* InBristlecone, UCablingWorldSubsystem* InCabling, UCanonicalInputStreamECS* InInputECS)
{
    Bristlecone = InBristlecone;
    Cabling = InCabling;
    InputECS = InInputECS;
}

void ArtilleryNetworkDebug::Update(float DeltaTime)
{
    // Update statistics
    CollectPacketStats();

    // Update per-second stats
    StatAccumTime += DeltaTime;
    if (StatAccumTime >= 1.0f)
    {
        // Add to history
        UpdateLatencyGraph();

        // Reset per-second stats
        Stats.ResetSecondStats();
        StatAccumTime = 0.0f;
    }

    // Collect information about active streams
    CollectActiveStreams();
}

void ArtilleryNetworkDebug::Draw()
{
    ImGui::FScopedContext ScopedContext;

    if (!ScopedContext)
        return;

    if (Cabling && Cabling->GetWorld()->GetGameInstance() && Cabling->GetWorld()->GetGameInstance()->IsDedicatedServerInstance())
    {
        if (NetImgui::IsConnected())
        {
            ScopedContext->Connect("127.0.0.1", 8888);
        }
    }

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Network Debug"))
    {
        if (ImGui::BeginTabBar("NetworkTabs"))
        {
            if (ImGui::BeginTabItem("Overview"))
            {
                DrawOverviewTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Packet Inspector"))
            {
                DrawPacketInspectorTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Latency Graph"))
            {
                DrawLatencyGraphTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Stream State"))
            {
                DrawStreamStateTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Per-Player Stats"))
            {
                DrawPerPlayerStatsTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void ArtilleryNetworkDebug::DrawOverviewTab()
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 5));

    if (ImGui::BeginTable("OverviewTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Statistic", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "BRISTLECONE");
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Network Layer");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Total Packets Sent");
        ImGui::TableNextColumn();
        ImGui::Text("%llu", Stats.TotalPacketsSent);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Total Packets Received");
        ImGui::TableNextColumn();
        ImGui::Text("%llu", Stats.TotalPacketsReceived);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Packets/Second (Send)");
        ImGui::TableNextColumn();
        ImGui::Text("%llu", Stats.PacketsSentLastSecond);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Packets/Second (Receive)");
        ImGui::TableNextColumn();
        ImGui::Text("%llu", Stats.PacketsReceivedLastSecond);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Average Latency");
        ImGui::TableNextColumn();
        ImGui::TextColored(
            GetHealthColor(Stats.AverageLatencyMs, 50.0f, 100.0f, 150.0f),
            "%.2f ms",
            Stats.AverageLatencyMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Min/Max Latency");
        ImGui::TableNextColumn();
        ImGui::Text("%.2f / %.2f ms", Stats.MinLatencyMs, Stats.MaxLatencyMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Estimated Dropped Packets");
        ImGui::TableNextColumn();
        ImGui::TextColored(
            GetHealthColor(Stats.DroppedPackets, 0, 5, 10),
            "%llu",
            Stats.DroppedPackets);

        // Cabling section
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "CABLING");
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Input Layer");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Total Inputs Processed");
        ImGui::TableNextColumn();
        ImGui::Text("%llu", Stats.TotalInputsProcessed);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Inputs/Second");
        ImGui::TableNextColumn();
        ImGui::Text("%llu", Stats.InputsProcessedLastSecond);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Current Sequence Number");
        ImGui::TableNextColumn();
        ImGui::Text("%llu", Stats.CurrentSequenceNumber);

        // Connection section
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), "CONNECTION STATUS");
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), "Network Health");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Network Status");
        ImGui::TableNextColumn();

        // Determine connection health
        float dropRate = Stats.PacketsReceivedLastSecond > 0 ?
            (float)Stats.DroppedPackets / Stats.PacketsReceivedLastSecond : 0;

        const char* statusText = "Unknown";
        ImVec4 statusColor(1.0f, 1.0f, 1.0f, 1.0f);

        if (Stats.PacketsReceivedLastSecond == 0)
        {
            statusText = "No Packets Received";
            statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        }
        else if (dropRate > 0.1f || Stats.AverageLatencyMs > 150.0f)
        {
            statusText = "Poor Connection";
            statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        }
        else if (dropRate > 0.05f || Stats.AverageLatencyMs > 100.0f)
        {
            statusText = "Unstable Connection";
            statusColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
        }
        else if (Stats.PacketsReceivedLastSecond > 0)
        {
            statusText = "Healthy Connection";
            statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        }

        ImGui::TextColored(statusColor, "%s", statusText);

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

void ArtilleryNetworkDebug::DrawPacketInspectorTab()
{
    ImGui::TextWrapped("Recent network packets (newest first):");

    if (ImGui::BeginTable("PacketsTable", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Seq #", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Latency (ms)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < RecentPackets.Num(); i++)
        {
            const FPacketDisplay& packet = RecentPackets[i];

            ImGui::TableNextRow();

            // Direction
            ImGui::TableNextColumn();
            if (packet.IsIncoming)
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "RECV");
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "SEND");
            }

            // Time
            ImGui::TableNextColumn();
            ImGui::Text("%llu", packet.Timestamp);

            // Player ID
            ImGui::TableNextColumn();
            if (packet.PlayerId < (uint32)PlayerNames.Num())
            {
                ImGui::Text("%s", TCHAR_TO_ANSI(*PlayerNames[packet.PlayerId]));
            }
            else
            {
                ImGui::Text("%u", packet.PlayerId);
            }

            // Sequence #
            ImGui::TableNextColumn();
            ImGui::Text("%u", packet.SequenceNumber);

            // Size
            ImGui::TableNextColumn();
            ImGui::Text("%u B", packet.Size);

            // Latency
            ImGui::TableNextColumn();
            if (packet.IsIncoming && packet.LatencyMs > 0)
            {
                ImGui::TextColored(
                    GetHealthColor(packet.LatencyMs, 50.0f, 100.0f, 150.0f),
                    "%.2f",
                    packet.LatencyMs);
            }
            else
            {
                ImGui::Text("-");
            }
        }

        ImGui::EndTable();
    }
}

void ArtilleryNetworkDebug::DrawLatencyGraphTab()
{
    static bool showPacketLoss = true;
    ImGui::Checkbox("Show Packet Loss", &showPacketLoss);

    ImGui::Separator();

    // Calculate graph dimensions
    const float graphHeight = 250.0f;
    const float width = ImGui::GetContentRegionAvail().x;

    // Calculate scale for both graphs
    float maxLatency = 200.0f; // Cap at 200ms for better visibility
    for (const auto& latency : Stats.LatencyHistory)
    {
        maxLatency = FMath::Max(maxLatency, latency);
    }

    // Latency graph
    if (ImGui::BeginChild("LatencyGraph", ImVec2(width, graphHeight), true))
    {
        ImGui::Text("Network Latency (last 2 minutes)");
        ImGui::SameLine(width - 100);
        ImGui::Text("Max: %.1f ms", maxLatency);

        // Latency plotting
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 0.8f, 0.8f, 1.0f));

        const int numPoints = Stats.LatencyHistory.size();
        float* points = new float[numPoints];
        for (int i = 0; i < numPoints; i++)
        {
            points[i] = Stats.LatencyHistory[i];
        }

        ImGui::PlotLines("##LatencyGraph", points, numPoints, 0, NULL, 0.0f, maxLatency, ImVec2(width - 20, graphHeight - 50));

        delete[] points;
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    // Packet loss graph
    if (showPacketLoss)
    {
        if (ImGui::BeginChild("PacketLossGraph", ImVec2(width, graphHeight), true))
        {
            // Graph title and scale
            ImGui::Text("Packet Loss (last 2 minutes)");

            // Find max for scale
            float maxLoss = 10.0f; // Default to 10%
            for (const auto& loss : Stats.PacketLossHistory)
            {
                maxLoss = FMath::Max(maxLoss, loss);
            }

            ImGui::SameLine(width - 100);
            ImGui::Text("Max: %.1f%%", maxLoss);

            // Loss plotting
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

            const int numPoints = Stats.PacketLossHistory.size();
            float* points = new float[numPoints];
            for (int i = 0; i < numPoints; i++)
            {
                points[i] = Stats.PacketLossHistory[i];
            }

            ImGui::PlotLines("##PacketLossGraph", points, numPoints, 0, NULL, 0.0f, maxLoss, ImVec2(width - 20, graphHeight - 50));

            delete[] points;
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }
}

void ArtilleryNetworkDebug::DrawStreamStateTab()
{
    if (!InputECS)
        return;

    ImGui::TextWrapped("Input stream states from UCanonicalInputStreamECS:");

    if (ImGui::BeginTable("StreamsTable", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Stream ID", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Highest Input", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Received Packets", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // Iterate through all player streams
        for (auto It = InputECS->SessionPlayerToStreamMapping.CreateConstIterator(); It; ++It)
        {
            PlayerKey playerKey = It.Key();
            InputStreamKey streamKey = It.Value();

            auto stream = InputECS->GetStream(streamKey);
            if (!stream.IsValid())
                continue;

            ImGui::TableNextRow();

            // Player
            ImGui::TableNextColumn();
            int playerIndex = (int)playerKey;
            if (playerIndex >= 0 && playerIndex < PlayerNames.Num())
            {
                ImGui::Text("%s", TCHAR_TO_ANSI(*PlayerNames[playerIndex]));
            }
            else
            {
                ImGui::Text("Player %d", playerIndex);
            }

            // Stream ID
            ImGui::TableNextColumn();
            ImGui::Text("%d", (int)streamKey);

            // Highest Input
            ImGui::TableNextColumn();
            ImGui::Text("%llu", stream->highestInput);

            // Received Packets
            ImGui::TableNextColumn();
            int packetCount = StreamPacketCounts.FindRef((int)streamKey);

            // Create a simple visual bar
            float fraction = FMath::Min(packetCount / 100.0f, 1.0f); // Scale to max of 100 packets
            ImGui::Text("%d", packetCount);
            ImGui::SameLine();

            // Draw bar
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size(ImGui::GetContentRegionAvail().x * 0.8f, 10);
            ImVec4 color = GetHealthColor(packetCount, 30, 10, 0);

            ImGui::GetWindowDrawList()->AddRectFilled(
                pos,
                ImVec2(pos.x + size.x * fraction, pos.y + size.y),
                ImGui::ColorConvertFloat4ToU32(color)
            );

            ImGui::GetWindowDrawList()->AddRect(
                pos,
                ImVec2(pos.x + size.x, pos.y + size.y),
                IM_COL32(128, 128, 128, 255)
            );

            ImGui::Dummy(size);
        }

        ImGui::EndTable();
    }
}

void ArtilleryNetworkDebug::DrawPerPlayerStatsTab()
{
    if (!InputECS)
        return;

    ImGui::TextWrapped("Per-player network statistics:");

    static int selectedPlayer = 0;

    // Player selection
    if (ImGui::BeginCombo("Player",
        selectedPlayer < PlayerNames.Num() ?
            TCHAR_TO_ANSI(*PlayerNames[selectedPlayer]) : "Unknown"))
    {
        for (int i = 0; i < PlayerNames.Num(); i++)
        {
            bool isSelected = (selectedPlayer == i);
            if (ImGui::Selectable(TCHAR_TO_ANSI(*PlayerNames[i]), isSelected))
                selectedPlayer = i;

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    PlayerKey playerKey = static_cast<PlayerKey>(selectedPlayer);

    InputStreamKey streamKey;
    bool hasStream = InputECS->SessionPlayerToStreamMapping.Contains(playerKey);

    if (hasStream)
    {
        streamKey = InputECS->SessionPlayerToStreamMapping.FindChecked(playerKey);
        auto stream = InputECS->GetStream(streamKey);

        if (stream.IsValid())
        {
            if (ImGui::BeginTable("PlayerStatsTable", 2, ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Statistic", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Stream ID");
                ImGui::TableNextColumn();
                ImGui::Text("%d", (int)streamKey);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Highest Input");
                ImGui::TableNextColumn();
                ImGui::Text("%llu", stream->highestInput);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Actor Associated");
                ImGui::TableNextColumn();
                ActorKey actorKey = stream->GetActorByInputStream();
                ImGui::Text("%llu", actorKey.Obj);

                ImGui::EndTable();
            }

            ImGui::Separator();
            ImGui::Text("Recent Input History:");

            if (ImGui::BeginTable("InputHistoryTable", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Raw Input", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Run Flag", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                uint64 highest = stream->highestInput;
                for (int i = 0; i < 15 && highest > i; i++)
                {
                    uint64 index = highest - i - 1;
                    auto input = stream->peek(index);

                    if (!input.has_value())
                        continue;

                    ImGui::TableNextRow();

                    // Index
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", index);

                    // Raw input
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%016llX", input->MyInputActions);

                    // Timestamp
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", input->SentAt);

                    // Run flag
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", input->RunAtLeastOnce ? "Yes" : "No");
                }

                ImGui::EndTable();
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Stream is not valid");
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No stream found for this player");
    }
}

void ArtilleryNetworkDebug::CollectPacketStats()
{
 if (!Bristlecone || !Cabling || !InputECS)
        return;

    // // Process each network connection separately
    //
    //     for (const auto& Pair : Bristlecone->GetPlayerNetworks())
    //     {
    //         int32 NetworkID = Pair.Key;
    //         FPlayerNetworkComponent* Network = Pair.Value;
    //
    //         if (!Network || !Network->QueueOfReceived.IsValid())
    //         {
    //             continue;
    //         }
    //
    //         FConnectionStats& ConnStats = Stats.ConnectionStats.FindOrAdd(NetworkID);
    //         uint32& lastProcessedCount = LastProcessedCounts.FindOrAdd(NetworkID, 0);
    //         uint32 currentCount = Network->QueueOfReceived->Count();
    //         ConnStats.PacketsReceivedLastSecond = currentCount;
    //
    //         if (currentCount > lastProcessedCount)
    //         {
    //             uint32 newPacketCount = currentCount - lastProcessedCount;
    //             ConnStats.TotalPacketsReceived += newPacketCount;
    //
    //             auto packet = Network->QueueOfReceived->Peek();
    //             if (packet)
    //             {
    //                 FPacketDisplay display;
    //                 display.IsIncoming = true;
    //                 display.NetworkID = NetworkID;
    //
    //                 // Only add this packet if we don't already have one with the same sequence number
    //                 bool alreadyTracking = false;
    //                 for (const auto& existing : RecentPackets)
    //                 {
    //                     if (existing.SequenceNumber == display.SequenceNumber &&
    //                         existing.PlayerId == display.PlayerId &&
    //                         existing.IsIncoming == display.IsIncoming &&
    //                         existing.NetworkID == display.NetworkID)
    //                     {
    //                         alreadyTracking = true;
    //                         break;
    //                     }
    //                 }
    //
    //                 if (!alreadyTracking)
    //                 {
    //                     RecentPackets.Insert(display, 0);
    //
    //                     ConnStats.AverageLatencyMs = (ConnStats.AverageLatencyMs * 0.95f) + (display.LatencyMs * 0.05f);
    //                     ConnStats.MaxLatencyMs = FMath::Max(ConnStats.MaxLatencyMs, display.LatencyMs);
    //                     ConnStats.MinLatencyMs = FMath::Min(ConnStats.MinLatencyMs, display.LatencyMs);
    //
    //                     while (RecentPackets.Num() > MAX_PACKETS_DISPLAY)
    //                     {
    //                         RecentPackets.RemoveAt(RecentPackets.Num() - 1);
    //                     }
    //                 }
    //             }
    //
    //             lastProcessedCount = currentCount;
    //         }
    //
    //         if (Network->QueueToSend.IsValid())
    //         {
    //             static TMap<int32, uint32> LastOutgoingCounts;
    //             uint32& lastOutCount = LastOutgoingCounts.FindOrAdd(NetworkID, 0);
    //             uint32 currentOutCount = Network->QueueToSend->Count();
    //
    //             if (currentOutCount > lastOutCount)
    //             {
    //                 uint32 newOutPackets = currentOutCount - lastOutCount;
    //                 ConnStats.TotalPacketsSent += newOutPackets;
    //                 ConnStats.PacketsSentLastSecond += newOutPackets;
    //
    //                 for (uint32 i = 0; i < newOutPackets; i++)
    //                 {
    //                     FPacketDisplay display;
    //                     display.Timestamp = NarrowClock::getSlicedMicrosecondNow();
    //                     display.Size = sizeof(uint64_t);
    //                     display.PlayerId = 0; // Assume local player
    //                     display.SequenceNumber = Stats.CurrentSequenceNumber++;
    //                     display.LatencyMs = 0;
    //                     display.IsIncoming = false;
    //                     display.NetworkID = NetworkID;
    //
    //                     RecentPackets.Insert(display, 0);
    //                 }
    //
    //                 while (RecentPackets.Num() > MAX_PACKETS_DISPLAY)
    //                 {
    //                     RecentPackets.RemoveAt(RecentPackets.Num() - 1);
    //                 }
    //
    //                 lastOutCount = currentOutCount;
    //             }
    //         }
    //     }
    //
    // // Collect stats from Cabling - NON-DESTRUCTIVE MONITORING
    // if (Cabling && Cabling->GetCableThreadControlQueue().IsValid())
    // {
    //     uint32 currentCount = Cabling->GetCableThreadControlQueue()->Count();
    //
    //     // We just want to know that a new packet is available without dequeueing
    //     static uint32 lastCount = 0;
    //
    //     if (currentCount > lastCount)
    //     {
    //         uint32 newPackets = currentCount - lastCount;
    //         Stats.TotalInputsProcessed += newPackets;
    //         Stats.InputsProcessedLastSecond += newPackets;
    //
    //         lastCount = currentCount;
    //     }
    // }
    // Stats.UpdateFromConnectionStats();
}

void ArtilleryNetworkDebug::UpdateLatencyGraph()
{
    Stats.LatencyHistory.push_back(Stats.AverageLatencyMs);
    if (Stats.LatencyHistory.size() > 120) // Keep 2 minutes of history
    {
        Stats.LatencyHistory.pop_front();
    }

    float packetLossPercent = 0.0f;
    if (Stats.TotalPacketsSent > 0)
    {
        packetLossPercent = (float)Stats.DroppedPackets / (float)Stats.TotalPacketsSent * 100.0f;
    }

    Stats.PacketLossHistory.push_back(packetLossPercent);
    if (Stats.PacketLossHistory.size() > 120)
    {
        Stats.PacketLossHistory.pop_front();
    }
}

void ArtilleryNetworkDebug::CollectActiveStreams()
{
    if (!InputECS)
        return;

    StreamPacketCounts.Empty();

    for (auto It = InputECS->SessionPlayerToStreamMapping.CreateConstIterator(); It; ++It)
    {
        PlayerKey playerKey = It.Key();
        InputStreamKey streamKey = It.Value();

        auto stream = InputECS->GetStream(streamKey);
        if (!stream.IsValid())
            continue;

        StreamPacketCounts.Add((int)streamKey, stream->highestInput);
    }
}

ImVec4 ArtilleryNetworkDebug::GetHealthColor(float value, float good, float warning, float critical)
{
    if (value <= good)
    {
        return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    }
    else if (value <= warning)
    {
        float t = (value - good) / (warning - good);
        return ImVec4(t, 1.0f, 0.0f, 1.0f);
    }
    else if (value <= critical)
    {
        float t = (value - warning) / (critical - warning);
        return ImVec4(1.0f, 1.0f - t, 0.0f, 1.0f);
    }
    else
    {
        return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
}
