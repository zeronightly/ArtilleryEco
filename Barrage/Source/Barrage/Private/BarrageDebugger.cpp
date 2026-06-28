// NOTE: patch's BarrageDebugger.cpp called FBarragePrimitive helpers with a
// (UObject* WorldContextObject, FBLet) signature — that was part of the patch's
// statics-removal that current main NEVER picked up. Current main's
// FBarragePrimitive::GetPosition/GetVelocity/etc. take only FBLet, so the
// FBarragePrimitive::* calls below have been adapted to drop the leading
// GEngine->GetWorld() argument. If current main's API later grows a world ctx
// param, restore those args.
#include "BarrageDebugger.h"

#include "BarrageContactEvent.h"
#include "FBarragePrimitive.h"
#include "FWorldSimOwner.h"
#include "imgui.h"

static TAutoConsoleVariable<int32> CVarEnableImGuiBarrage(
    TEXT("imgui.Enable.Barrage"),
    0,
    TEXT("Enable or Disable ImGui Rendering.\n0: Off\n1: On"),
    ECVF_Default
);

void BarrageDebugger::Draw(float DeltaTime)
{
    DrawImGuiDebug();
}

void BarrageDebugger::Initialize(UBarrageDispatch* InDispatchOwner)
{
    DispatchOwner = InDispatchOwner;
    check(DispatchOwner.Get());
}

void BarrageDebugger::DrawImGuiDebug()
{
    if (CVarEnableImGuiBarrage.GetValueOnGameThread() == 0)
    {
        return;
    }

    const ImGui::FScopedContext ScopedContext;

    if (!ScopedContext)
        return;

    ImGui::Text("Barrage Physics System");

    // Display basic statistics
    int32 NumRegisteredPrimitives = 0;
    int32 NumCharacters = 0;
    int32 NumBoxes = 0;
    int32 NumSpheres = 0;
    int32 NumCapsules = 0;
    int32 NumProjectiles = 0;

    // Get statistics safely using a shared ptr hold-open
    auto HoldCuckooLifecycle = DispatchOwner->JoltBodyLifecycleMapping;
    if (HoldCuckooLifecycle && HoldCuckooLifecycle.get() && !HoldCuckooLifecycle.get()->empty())
    {
        NumRegisteredPrimitives = HoldCuckooLifecycle->size();

        // Calculate primitive type counts
       HoldCuckooLifecycle->visit_all( 
		[&](auto& Pair) {
        
            if (Pair.first.KeyIntoBarrage != 0 && FBarragePrimitive::IsNotNull(Pair.second))
            {
                switch (Pair.second->Me)
                {
                case FBShape::Character:
                    NumCharacters++;
                    break;
                case FBShape::Box:
                    NumBoxes++;
                    break;
                case FBShape::Sphere:
                    NumSpheres++;
                    break;
                case FBShape::Capsule:
                    NumCapsules++;
                    break;
                case FBShape::Projectile:
                    NumProjectiles++;
                    break;
                default: break;
                }
            }
        });
    }

    ImGui::Text("Registered Primitives: %d", NumRegisteredPrimitives);
    ImGui::Text("Characters: %d | Boxes: %d | Spheres: %d | Capsules: %d | Projectiles: %d",
        NumCharacters, NumBoxes, NumSpheres, NumCapsules, NumProjectiles);

    // Contact events
    TSharedPtr<TCircularQueue<BarrageContactEvent>> HoldOpen = DispatchOwner->ContactEventPump;
    if (HoldOpen)
    {
        ImGui::Text("Contact Event Queue Size: %d", HoldOpen->Count());
    }
}

void BarrageDebugger::DrawAllBarrageImGui()
{
    static bool bShowWindow = true;

    const ImGui::FScopedContext ScopedContext;
    if (!ScopedContext || !IsInGameThread())
        return;

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Barrage Physics Debug", &bShowWindow))
    {
        UWorld* World = DispatchOwner->GetWorld();
        if (!World)
        {
            ImGui::Text("No active world");
            ImGui::End();
            return;
        }

        UBarrageDispatch* BarrageDispatch = World->GetSubsystem<UBarrageDispatch>();
        if (!BarrageDispatch)
        {
            ImGui::Text("Barrage Dispatch subsystem not available");
            ImGui::End();
            return;
        }

        if (!BarrageDispatch->JoltBodyLifecycleMapping)
        {
            ImGui::Text("Barrage-to-Jolt mapping not initialized");
            ImGui::End();
            return;
        }

        // Main tabs
        static int currentTab = 0;
        if (ImGui::BeginTabBar("BarrageTabBar"))
        {
            if (ImGui::BeginTabItem("Overview"))
            {
                DrawImGuiDebug();

                // Performance metrics
                ImGui::Separator();
                ImGui::Text("Performance Metrics");

                static float StepTimes[120] = {}; // Store last 120 frames
                static float ContactTimes[120] = {}; // Store last 120 frames
                static int TimeIndex = 0;
                static float LastDeltaTime = 0.0f;

                // In actual implementation, you would track these metrics in Tick
                if (World->GetTimeSeconds() - LastDeltaTime > 0.016f) // ~60fps update
                {
                    LastDeltaTime = World->GetTimeSeconds();
                    // Example: These would be measured elsewhere
                    StepTimes[TimeIndex] = FMath::RandRange(0.1f, 1.0f);
                    ContactTimes[TimeIndex] = FMath::RandRange(0.05f, 0.5f);
                    TimeIndex = (TimeIndex + 1) % IM_ARRAYSIZE(StepTimes);
                }

                ImGui::Text("Physics Step Time (ms)");
                ImGui::PlotLines("##StepTimes", StepTimes, IM_ARRAYSIZE(StepTimes),
                    TimeIndex, nullptr, 0.0f, 2.0f, ImVec2(0, 80));

                ImGui::Text("Contact Processing Time (ms)");
                ImGui::PlotLines("##ContactTimes", ContactTimes, IM_ARRAYSIZE(ContactTimes),
                    TimeIndex, nullptr, 0.0f, 1.0f, ImVec2(0, 80));

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Barrage-Jolt Mappings"))
            {
                DrawBarrageToJoltKeyMapping();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Contact Events"))
            {
                // Display recent contact events
                TSharedPtr<TCircularQueue<BarrageContactEvent>> HoldOpen = BarrageDispatch->ContactEventPump;
                if (HoldOpen && HoldOpen->Count() > 0)
                {
                    if (ImGui::BeginTable("ContactEventsTable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                    {
                        ImGui::TableSetupColumn("Type");
                        ImGui::TableSetupColumn("Body 1");
                        ImGui::TableSetupColumn("Body 2");
                        ImGui::TableSetupColumn("Time");
                        ImGui::TableHeadersRow();

                        // We can't directly iterate a circular queue, but we could display recent events
                        // In a real implementation, you'd want to keep a buffer of the last N events
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("Contact events would be displayed here");
                        ImGui::TableNextColumn();
                        ImGui::Text("Additional implementation required");

                        ImGui::EndTable();
                    }
                }
                else
                {
                    ImGui::Text("No contact events currently in the queue");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Characters"))
            {
                // Display character-specific info
                TSharedPtr<TMap<FBarrageKey, TSharedPtr<FBCharacterBase>>> CharacterMap =
                    BarrageDispatch->JoltGameSim ? BarrageDispatch->JoltGameSim->CharacterToJoltMapping : nullptr;

                if (CharacterMap && CharacterMap->Num() > 0)
                {
                    if (ImGui::BeginTable("CharactersTable", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                    {
                        ImGui::TableSetupColumn("Key");
                        ImGui::TableSetupColumn("Position");
                        ImGui::TableSetupColumn("Velocity");
                        ImGui::TableSetupColumn("Ground State");
                        ImGui::TableSetupColumn("Max Speed");
                        ImGui::TableHeadersRow();

                        for (const auto& Pair : *CharacterMap)
                        {
                            if (Pair.Value && Pair.Value->mCharacter)
                            {
                                ImGui::TableNextRow();

                                // Key
                                ImGui::TableNextColumn();
                                ImGui::Text("%llu", Pair.Key.KeyIntoBarrage);

                                // Position
                                ImGui::TableNextColumn();
                                JPH::Vec3 Position = Pair.Value->mCharacter->GetPosition();
                                ImGui::Text("%.1f, %.1f, %.1f", Position.GetX(), Position.GetY(), Position.GetZ());

                                // Velocity
                                ImGui::TableNextColumn();
                                JPH::Vec3 Velocity = Pair.Value->mCharacter->GetLinearVelocity();
                                ImGui::Text("%.1f, %.1f, %.1f", Velocity.GetX(), Velocity.GetY(), Velocity.GetZ());

                                // Ground State
                                ImGui::TableNextColumn();
                                const char* GroundStateStr = "Unknown";
                                switch (FBarragePrimitive::FromJoltGroundState(Pair.Value->mCharacter->GetGroundState()))
                                {
                                case FBarragePrimitive::FBGroundState::OnGround:
                                    GroundStateStr = "OnGround";
                                    break;
                                case FBarragePrimitive::FBGroundState::OnSteepGround:
                                    GroundStateStr = "OnSteepGround";
                                    break;
                                case FBarragePrimitive::FBGroundState::NotSupported:
                                    GroundStateStr = "NotSupported";
                                    break;
                                case FBarragePrimitive::FBGroundState::InAir:
                                    GroundStateStr = "InAir";
                                    break;
                                }
                                ImGui::Text("%s", GroundStateStr);

                                // Max Speed
                                ImGui::TableNextColumn();
                                ImGui::Text("%.1f", Pair.Value->mMaxSpeed * 100.0f); // Convert back from internal units
                            }
                        }

                        ImGui::EndTable();
                    }
                }
                else
                {
                    ImGui::Text("No characters currently active");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Debug Actions"))
            {
                if (ImGui::Button("Optimize Broad Phase"))
                {
                    if (BarrageDispatch->JoltGameSim)
                    {
                        BarrageDispatch->JoltGameSim->OptimizeBroadPhase();
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Clear Contact Queue"))
                {
                    TSharedPtr<TCircularQueue<BarrageContactEvent>> HoldOpen = BarrageDispatch->ContactEventPump;
                    if (HoldOpen)
                    {
                        HoldOpen->Empty();
                    }
                }

                // Debug raycast
                static float RayFrom[3] = {0.0f, 0.0f, 0.0f};
                static float RayDir[3] = {0.0f, 0.0f, 1.0f};

                ImGui::Separator();
                ImGui::Text("Debug Raycast");
                ImGui::InputFloat3("Origin", RayFrom);
                ImGui::InputFloat3("Direction", RayDir);

                if (ImGui::Button("Cast Ray"))
                {
                    if (BarrageDispatch)
                    {
                        TSharedPtr<FHitResult> HitResult = MakeShareable(new FHitResult());
                        BarrageDispatch->CastRay(
                            FVector3d(RayFrom[0], RayFrom[1], RayFrom[2]),
                            FVector3d(RayDir[0], RayDir[1], RayDir[2]),
                            BarrageDispatch->GetDefaultBroadPhaseLayerFilter(Layers::CAST_QUERY),
                            BarrageDispatch->GetDefaultLayerFilter(Layers::CAST_QUERY),
                            JPH::BodyFilter(),
                            HitResult);

                        if (HitResult->bBlockingHit)
                        {
                            // Display hit result
                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                                "Hit at (%.1f, %.1f, %.1f)",
                                HitResult->Location.X,
                                HitResult->Location.Y,
                                HitResult->Location.Z);

                            ImGui::Text("Body ID: %d", HitResult->MyItem);
                            ImGui::Text("Distance: %.2f", HitResult->Distance);
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No hit");
                        }
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void BarrageDebugger::DrawBarrageToJoltKeyMapping()
{
    const ImGui::FScopedContext ScopedContext;
    if (!ScopedContext || !IsInGameThread())
        return;

    UWorld* World = DispatchOwner->GetWorld();
    if (!World)
        return;

    UBarrageDispatch* BarrageDispatch = World->GetSubsystem<UBarrageDispatch>();
    if (!BarrageDispatch || !BarrageDispatch->JoltBodyLifecycleMapping)
        return;

    // Search filter
    static char searchBuffer[256] = "";
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::InputText("##Filter", searchBuffer, IM_ARRAYSIZE(searchBuffer));
    std::string searchStr = searchBuffer;

    // Type filter
    static bool showBoxes = true;
    static bool showSpheres = true;
    static bool showCapsules = true;
    static bool showCharacters = true;
    static bool showProjectiles = true;
    static bool showOther = true;
    static bool showTombstoned = false;

    ImGui::Text("Filter by Type:");
    ImGui::SameLine();
    ImGui::Checkbox("Boxes", &showBoxes);
    ImGui::SameLine();
    ImGui::Checkbox("Spheres", &showSpheres);
    ImGui::SameLine();
    ImGui::Checkbox("Capsules", &showCapsules);
    ImGui::SameLine();
    ImGui::Checkbox("Characters", &showCharacters);
    ImGui::SameLine();
    ImGui::Checkbox("Projectiles", &showProjectiles);
    ImGui::SameLine();
    ImGui::Checkbox("Other", &showOther);

    ImGui::SameLine();
    ImGui::Checkbox("Show Tombstoned", &showTombstoned);

    ImGui::Separator();

    // Table for mappings
    if (ImGui::BeginTable("MappingsTable", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable))
    {
        ImGui::TableSetupColumn("Barrage Key", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Jolt Body ID");
        ImGui::TableSetupColumn("Out Key");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Tombstone");
        ImGui::TableHeadersRow();

        // Get and hold references for thread safety
        auto HoldCuckooLifecycle = BarrageDispatch->JoltBodyLifecycleMapping;
        TSharedPtr<FWorldSimOwner> HoldJoltGameSim = BarrageDispatch->JoltGameSim;

        if (HoldCuckooLifecycle && HoldCuckooLifecycle.get() && !HoldCuckooLifecycle.get()->empty() && HoldJoltGameSim)
        {
            // Copy data to a regular array for display
            TArray<TPair<FBarrageKey, TSharedPtr<FBarragePrimitive>>> FilteredEntries;

            HoldCuckooLifecycle->visit_all( 
            [&](auto& Pair) {
            
                // Skip invalid entries
                if (Pair.first.KeyIntoBarrage == 0 || !Pair.second)
                    return;

                // Apply tombstone filter
                if (!showTombstoned && Pair.second->tombstone > 0)
                    return;;

                // Apply type filter
                bool showThisType = false;
                switch (Pair.second->Me)
                {
                case FBShape::Box:
                    showThisType = showBoxes;
                    break;
                case FBShape::Sphere:
                    showThisType = showSpheres;
                    break;
                case FBShape::Capsule:
                    showThisType = showCapsules;
                    break;
                case FBShape::Character:
                    showThisType = showCharacters;
                    break;
                case FBShape::Projectile:
                    showThisType = showProjectiles;
                    break;
                default:
                    showThisType = showOther;
                }

                if (!showThisType)
                    return;;

                // Apply text search filter
                if (searchStr.length() > 0)
                {
                    FString KeyStr = FString::Printf(TEXT("%llu"), Pair.first.KeyIntoBarrage);
                    FString OutKeyStr = FString::Printf(TEXT("%llu"), Pair.second->KeyOutOfBarrage.Obj);

                    if (!KeyStr.Contains(searchBuffer) && !OutKeyStr.Contains(searchBuffer))
                        return;
                }

                FilteredEntries.Add(TPair<FBarrageKey, TSharedPtr<FBarragePrimitive>>(Pair.first, Pair.second));
            });

            // Sort by Barrage key
            FilteredEntries.Sort([](const TPair<FBarrageKey, TSharedPtr<FBarragePrimitive>>& A,
                                   const TPair<FBarrageKey, TSharedPtr<FBarragePrimitive>>& B) {
                return A.Key.KeyIntoBarrage < B.Key.KeyIntoBarrage;
            });

            // Display filtered entries
            for (const auto& Entry : FilteredEntries)
            {
                ImGui::TableNextRow();

                // Barrage Key
                ImGui::TableNextColumn();
                ImGui::Text("%llu", Entry.Key.KeyIntoBarrage);

                // Jolt Body ID
                ImGui::TableNextColumn();
                JPH::BodyID JoltID;
                bool HasJoltID = bool( HoldJoltGameSim->BarrageToJoltMapping->visit(Entry.Key, [&](auto& a) { 
                if (HasJoltID && !JoltID.IsInvalid())
                {
                    ImGui::Text("%u", JoltID.GetIndexAndSequenceNumber());
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "N/A");
                }

                // Out Key
                ImGui::TableNextColumn();
                ImGui::Text("%llu", Entry.Value->KeyOutOfBarrage.Obj);

                // Type
                ImGui::TableNextColumn();
                const char* TypeName = "Unknown";
                ImVec4 TypeColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                switch (Entry.Value->Me)
                {
                case FBShape::Box:
                    TypeName = "Box";
                    TypeColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
                    break;
                case FBShape::Sphere:
                    TypeName = "Sphere";
                    TypeColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
                    break;
                case FBShape::Capsule:
                    TypeName = "Capsule";
                    TypeColor = ImVec4(0.0f, 0.8f, 0.4f, 1.0f);
                    break;
                case FBShape::Character:
                    TypeName = "Character";
                    TypeColor = ImVec4(1.0f, 0.9f, 0.0f, 1.0f);
                    break;
                case FBShape::Projectile:
                    TypeName = "Projectile";
                    TypeColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                    break;
                case FBShape::Static:
                    TypeName = "Static";
                    TypeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    break;
                }

                ImGui::TextColored(TypeColor, "%s", TypeName);

                // Position — adapted: current main's API is GetPosition(FBLet) not (UObject*, FBLet)
                ImGui::TableNextColumn();
                FVector3f Position = FBarragePrimitive::GetPosition(Entry.Value);
                if (!Position.ContainsNaN())
                {
                    ImGui::Text("%.1f, %.1f, %.1f", Position.X, Position.Y, Position.Z);
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "N/A");
                }

                // Tombstone
                ImGui::TableNextColumn();
                if (Entry.Value->tombstone > 0)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Yes (%u)", Entry.Value->tombstone);
                }
                else
                {
                    ImGui::Text("No");
                }
            }));

                ImGui::EndTable();
                ImGui::Text("Showing %d/%d primitives", FilteredEntries.Num(), HoldCuckooLifecycle->size());
            }
        }

        // Object inspection
        ImGui::Separator();
        ImGui::Text("Inspect Specific Object:");

        static char keyBuffer[64] = "";
        ImGui::InputText("##KeyInput", keyBuffer, IM_ARRAYSIZE(keyBuffer));
        ImGui::SameLine();

        static FBarrageKey inspectedKey;
        static FBLet inspectedObject;

        if (ImGui::Button("Inspect Barrage Key"))
        {
            // Try to parse the key and find the object
            uint64 KeyValue = FCString::Strtoui64(ANSI_TO_TCHAR(keyBuffer), nullptr, 10);
            if (KeyValue > 0)
            {
                inspectedKey = FBarrageKey(KeyValue);
                inspectedObject = BarrageDispatch->GetShapeRef(inspectedKey);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Inspect Out Key"))
        {
            // Try to parse the key and find the object
            uint64 KeyValue = FCString::Strtoui64(ANSI_TO_TCHAR(keyBuffer), nullptr, 10);
            if (KeyValue > 0)
            {
                FSkeletonKey OutKey(KeyValue);
                inspectedObject = BarrageDispatch->GetShapeRef(OutKey);
                if (inspectedObject)
                {
                    inspectedKey = inspectedObject->KeyIntoBarrage;
                }
            }
        }

        // Display inspection results
        if (inspectedObject)
        {
            ImGui::Separator();
            ImGui::Text("Object Details:");

            ImGui::Columns(2);

            ImGui::Text("Barrage Key:"); ImGui::NextColumn();
            ImGui::Text("%llu", inspectedKey.KeyIntoBarrage); ImGui::NextColumn();

            ImGui::Text("Out Key:"); ImGui::NextColumn();
            ImGui::Text("%llu", inspectedObject->KeyOutOfBarrage.Obj); ImGui::NextColumn();

            ImGui::Text("Type:"); ImGui::NextColumn();
            const char* TypeName = "Unknown";
            switch (inspectedObject->Me)
            {
            case FBShape::Box: TypeName = "Box"; break;
            case FBShape::Sphere: TypeName = "Sphere"; break;
            case FBShape::Capsule: TypeName = "Capsule"; break;
            case FBShape::Character: TypeName = "Character"; break;
            case FBShape::Projectile: TypeName = "Projectile"; break;
            case FBShape::Static: TypeName = "Static"; break;
            case Uninitialized: TypeName = "Uh-Oh";
                break;
            case Complex: TypeName = "Complex";
                break;
            }
            ImGui::Text("%s", TypeName); ImGui::NextColumn();

            ImGui::Text("Tombstone:"); ImGui::NextColumn();
            ImGui::Text("%u", inspectedObject->tombstone); ImGui::NextColumn();

            ImGui::Text("Position:"); ImGui::NextColumn();
            FVector3f Position = FBarragePrimitive::GetPosition(inspectedObject);
            if (!Position.ContainsNaN())
            {
                ImGui::Text("%.3f, %.3f, %.3f", Position.X, Position.Y, Position.Z);
            }
            else
            {
                ImGui::Text("N/A");
            }
            ImGui::NextColumn();

            ImGui::Text("Velocity:"); ImGui::NextColumn();
            FVector3f Velocity = FBarragePrimitive::GetVelocity(inspectedObject);
            if (!Velocity.ContainsNaN())
            {
                ImGui::Text("%.3f, %.3f, %.3f", Velocity.X, Velocity.Y, Velocity.Z);
            }
            else
            {
                ImGui::Text("N/A");
            }
            ImGui::NextColumn();

            if (inspectedObject->Me == FBShape::Character)
            {
                ImGui::Text("Ground State:"); ImGui::NextColumn();
                FBarragePrimitive::FBGroundState GroundState = FBarragePrimitive::GetCharacterGroundState(inspectedObject);
                const char* GroundStateStr = "Unknown";
                switch (GroundState)
                {
                case FBarragePrimitive::FBGroundState::OnGround: GroundStateStr = "OnGround"; break;
                case FBarragePrimitive::FBGroundState::OnSteepGround: GroundStateStr = "OnSteepGround"; break;
                case FBarragePrimitive::FBGroundState::NotSupported: GroundStateStr = "NotSupported"; break;
                case FBarragePrimitive::FBGroundState::InAir: GroundStateStr = "InAir"; break;
                case FBarragePrimitive::FBGroundState::NotFound: GroundStateStr = "NotFound"; break;
                }
                ImGui::Text("%s", GroundStateStr); ImGui::NextColumn();

                ImGui::Text("Ground Normal:"); ImGui::NextColumn();
                FVector3f GroundNormal = FBarragePrimitive::GetCharacterGroundNormal(inspectedObject);
                if (!GroundNormal.ContainsNaN() && !GroundNormal.IsZero())
                {
                    ImGui::Text("%.3f, %.3f, %.3f", GroundNormal.X, GroundNormal.Y, GroundNormal.Z);
                }
                else
                {
                    ImGui::Text("N/A");
                }
                ImGui::NextColumn();
            }

            ImGui::Columns(1);

            // Actions
            ImGui::Separator();
            ImGui::Text("Actions:");

            if (ImGui::Button("Apply Force"))
            {
                if (FBarragePrimitive::IsNotNull(inspectedObject))
                {
                    FBarragePrimitive::ApplyForce(FVector3d(0, 0, 1000), inspectedObject);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Set Zero Velocity"))
            {
                if (FBarragePrimitive::IsNotNull(inspectedObject))
                {
                    FBarragePrimitive::SetVelocity(FVector3d(0, 0, 0), inspectedObject);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Debug Entity"))
            {
                // Add code to dump entity debug info to output log
                if (FBarragePrimitive::IsNotNull(inspectedObject))
                {
                    UE_LOG(LogTemp, Warning, TEXT("--- BARRAGE ENTITY DEBUG ---"));
                    UE_LOG(LogTemp, Warning, TEXT("Barrage Key: %llu"), inspectedKey.KeyIntoBarrage);
                    UE_LOG(LogTemp, Warning, TEXT("Out Key: %llu"), inspectedObject->KeyOutOfBarrage.Obj);
                    UE_LOG(LogTemp, Warning, TEXT("Type: %d"), inspectedObject->Me);
                    UE_LOG(LogTemp, Warning, TEXT("Tombstone: %u"), inspectedObject->tombstone);

                    FVector3f Pos = FBarragePrimitive::GetPosition(inspectedObject);
                    UE_LOG(LogTemp, Warning, TEXT("Position: %s"), *Pos.ToString());

                    FVector3f Vel = FBarragePrimitive::GetVelocity(inspectedObject);
                    UE_LOG(LogTemp, Warning, TEXT("Velocity: %s"), *Vel.ToString());
                }
            }
        }
    }
}
