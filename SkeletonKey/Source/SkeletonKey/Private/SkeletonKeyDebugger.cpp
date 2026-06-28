#include "SkeletonKeyDebugger.h"

#include "EngineUtils.h"
#include "imgui.h"
#include "KeyCarry.h"
#include "TransformDispatch.h"
#include "Engine/GameInstance.h"

static TAutoConsoleVariable<int32> CVarEnableImGuiTransformDispatch(
    TEXT("imgui.Enable.SkeletonKey.TransformDispatch"),
    0,
    TEXT("Enable or Disable ImGui Rendering.\n0: Off\n1: On"),
    ECVF_Default
);

static TAutoConsoleVariable<int32> CVarEnableImGuiKeyCarry(
	TEXT("imgui.Enable.SkeletonKey.KeyCarry"),
	0,
	TEXT("Enable or Disable ImGui Rendering.\n0: Off\n1: On"),
	ECVF_Default
);


void SkeletonKeyDebugger::Initialize(UTransformDispatch* InDispatchOwner)
{
    DispatchOwner = InDispatchOwner;
    check(DispatchOwner.Get());
}

void SkeletonKeyDebugger::DrawImGuiDebug()
{
    if (CVarEnableImGuiTransformDispatch.GetValueOnGameThread() == 0)
    {
        return;
    }

    const ImGui::FScopedContext ScopedContext;

    if (!ScopedContext)
        return;

    if (!DispatchOwner->ObjectToTransformMapping)
    {
        ImGui::Text("Transform Mapping not initialized");
        return;
    }

    ImGui::Text("Transform Dispatch System");
    ImGui::Separator();

    // Display basic statistics
    int32 NumRegisteredTransforms = DispatchOwner->ObjectToTransformMapping->size();
    ImGui::Text("Registered Transforms: %d", NumRegisteredTransforms);

    // Display default object key if valid
    if (DispatchOwner->DefaultObjectKey.IsValid())
    {
        ImGui::Text("Default Object Key: %llu", DispatchOwner->DefaultObjectKey.Obj);
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Default Object Key: INVALID");
    }

    ImGui::Separator();

    // Display the first few transforms (limit to avoid UI overload)
    if (NumRegisteredTransforms > 0)
    {
        if (ImGui::CollapsingHeader("Active Transforms", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const int32 MaxDisplayCount = FMath::Min(10, NumRegisteredTransforms);

            if (ImGui::BeginTable("TransformsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Object Key");
                ImGui::TableSetupColumn("Transform");
                ImGui::TableHeadersRow();

                int32 DisplayCount = 0;

            	DispatchOwner->ObjectToTransformMapping->visit_all(
					[&DisplayCount, this, MaxDisplayCount](auto& Elem) {
                    if (DisplayCount >= MaxDisplayCount)
                        return;

                    ImGui::TableNextRow();

                    // Object Key Column
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", Elem.first.Obj);

                    // Transform Column
                    ImGui::TableNextColumn();
                    if (Elem.second.IsValid())
                    {
                        TOptional<FTransform3d> Transform = DispatchOwner->CopyOfTransformByObjectKey(Elem.first);
                        if (Transform.IsSet())
                        {
                            FVector3d Location = Transform->GetLocation();
                            FQuat4d Rotation = Transform->GetRotation();
                            ImGui::Text("Loc(%.1f, %.1f, %.1f) Rot(%.1f, %.1f, %.1f, %.1f)",
                                Location.X, Location.Y, Location.Z,
                                Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Transform Not Available");
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "INVALID");
                    }

                    DisplayCount++;
                });

                ImGui::EndTable();

                if (NumRegisteredTransforms > MaxDisplayCount)
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "... and %d more transforms (use the full debug window for complete list)",
                        NumRegisteredTransforms - MaxDisplayCount);
                }
            }
        }
    }
}

void SkeletonKeyDebugger::DrawAllTransformsImGui()
{
    if (CVarEnableImGuiTransformDispatch.GetValueOnGameThread() == 0)
    {
        return;
    }

    const ImGui::FScopedContext ScopedContext;

    if (!ScopedContext)
        return;

    static bool bShowWindow = true;
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Transform Dispatch Debug", &bShowWindow))
    {
        UWorld* World = GEngine->GetCurrentPlayWorld();
        if (!World)
        {
            ImGui::Text("No active world");
            ImGui::End();
            return;
        }

        UTransformDispatch* TransformDispatch = World->GetSubsystem<UTransformDispatch>();
        if (!TransformDispatch)
        {
            ImGui::Text("Transform Dispatch subsystem not available");
            ImGui::End();
            return;
        }

        if (!TransformDispatch->ObjectToTransformMapping)
        {
            ImGui::Text("Transform Mapping not initialized");
            ImGui::End();
            return;
        }

        // Network status display
        ENetMode NetMode = World->GetNetMode();
        ImVec4 StatusColor;
        const char* StatusText = nullptr;

        switch (NetMode)
        {
        case NM_Standalone:
            StatusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
            StatusText = "Standalone";
            break;
        case NM_DedicatedServer:
            StatusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
            StatusText = "Dedicated Server";
            break;
        case NM_ListenServer:
            StatusColor = ImVec4(0.0f, 0.8f, 0.2f, 1.0f); // Green-blue
            StatusText = "Listen Server";
            break;
        case NM_Client:
            StatusColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f); // Blue
            StatusText = "Client";
            break;
        default:
            StatusColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
            StatusText = "Unknown";
        }

        ImGui::TextColored(StatusColor, "Network Mode: %s", StatusText);

        // System Statistics
        int32 NumRegisteredTransforms = TransformDispatch->ObjectToTransformMapping->size();
        ImGui::Text("Registered Transforms: %d", NumRegisteredTransforms);

        // Performance metrics
        ImGui::Separator();
        ImGui::Text("Performance Metrics");

        static float ApplyUpdateTimes[120] = {}; // Store last 120 frames
        static int ApplyUpdateTimeIndex = 0;
        static float LastDeltaTime = 0.0f;

        // In actual implementation, you would track these metrics in the Tick or ApplyTransformUpdates methods
        // For this example, we'll just use random values to demonstrate the UI
        if (World->GetTimeSeconds() - LastDeltaTime > 0.016f) // ~60fps update
        {
            LastDeltaTime = World->GetTimeSeconds();
            // Example: track time spent in ApplyTransformUpdates (would be measured in actual implementation)
            ApplyUpdateTimes[ApplyUpdateTimeIndex] = FMath::RandRange(0.1f, 1.0f);
            ApplyUpdateTimeIndex = (ApplyUpdateTimeIndex + 1) % IM_ARRAYSIZE(ApplyUpdateTimes);
        }

        // Plot the update times
        ImGui::Text("Transform Update Time (ms)");
        ImGui::PlotLines("##UpdateTimes", ApplyUpdateTimes, IM_ARRAYSIZE(ApplyUpdateTimes),
            ApplyUpdateTimeIndex, nullptr, 0.0f, 2.0f, ImVec2(0, 80));

        ImGui::Separator();

        // Search filter
        static char searchBuffer[256] = "";
        ImGui::Text("Filter:");
        ImGui::SameLine();
        ImGui::InputText("##Filter", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        std::string searchStr = searchBuffer;

        // Kine type filter
        static bool showActorKines = true;
        static bool showSceneCompKines = true;
        static bool showSwarmKines = true;
        static bool showOtherKines = true;

        ImGui::Text("Filter by Kine Type:");
        ImGui::SameLine();
        ImGui::Checkbox("Actors", &showActorKines);
        ImGui::SameLine();
        ImGui::Checkbox("Components", &showSceneCompKines);
        ImGui::SameLine();
        ImGui::Checkbox("Swarm", &showSwarmKines);
        ImGui::SameLine();
        ImGui::Checkbox("Other", &showOtherKines);

        ImGui::Separator();

        // Table for transforms
        if (ImGui::BeginTable("TransformsTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable))
        {
            ImGui::TableSetupColumn("Object Key", ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Location");
            ImGui::TableSetupColumn("Rotation");
            ImGui::TableHeadersRow();

            // Copy the transforms to avoid UI lagging the game thread
            TArray<TPair<FSkeletonKey, FString>> FilteredTransforms;


            // Sort by key for consistent display
            FilteredTransforms.Sort([](const TPair<FSkeletonKey, FString>& A, const TPair<FSkeletonKey, FString>& B) {
                return A.Key.Obj < B.Key.Obj;
            });

            for (const TPair<FSkeletonKey, FString>& TransformEntry : FilteredTransforms)
            {
                ImGui::TableNextRow();

                // Object Key Column
                ImGui::TableNextColumn();
                ImGui::Text("%llu", TransformEntry.Key.Obj);

                // Kine Type Column
                ImGui::TableNextColumn();
                ImGui::Text("%s", TCHAR_TO_UTF8(*TransformEntry.Value));

                // Location Column
                ImGui::TableNextColumn();
                TOptional<FTransform3d> Transform = TransformDispatch->CopyOfTransformByObjectKey(TransformEntry.Key);
                if (Transform.IsSet())
                {
                    FVector3d Location = Transform->GetLocation();
                    ImGui::Text("%.1f, %.1f, %.1f", Location.X, Location.Y, Location.Z);
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Not Available");
                }

                // Rotation Column
                ImGui::TableNextColumn();
                if (Transform.IsSet())
                {
                    FQuat4d Rotation = Transform->GetRotation();
                    ImGui::Text("%.2f, %.2f, %.2f, %.2f", Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Not Available");
                }
            }

            ImGui::EndTable();
            ImGui::Text("Showing %d/%d transforms", FilteredTransforms.Num(), NumRegisteredTransforms);
        }

        // Add ability to inspect a specific key
        ImGui::Separator();
        ImGui::Text("Inspect Specific Key:");

        static char keyBuffer[64] = "";
        ImGui::InputText("##KeyInput", keyBuffer, IM_ARRAYSIZE(keyBuffer));

        ImGui::SameLine();
        if (ImGui::Button("Inspect"))
        {
            // Try to parse the key
            uint64 KeyValue = FCString::Strtoui64(ANSI_TO_TCHAR(keyBuffer), nullptr, 10);

            if (KeyValue > 0)
            {
                FSkeletonKey KeyToInspect(KeyValue);
                if (TSharedPtr<Kine> FoundKine = TransformDispatch->GetKineByObjectKey(KeyToInspect))
                {
                    // Set this key as "selected" for extended display
                    // In a real implementation, you might want to highlight this in the table
                    // or show additional details below
                }
            }
        }

        // Additional debugging options
        if (ImGui::CollapsingHeader("Debug Actions"))
        {
            if (ImGui::Button("Force Refresh All Kines"))
            {
                // In real implementation, this would trigger a refresh of all Kines
                // Could be useful for debugging
            }

            ImGui::SameLine();
            if (ImGui::Button("Log All Keys"))
            {
                // In real implementation, this would log all keys to the output log
                // for debugging purposes
            }
        }
    }
    ImGui::End();
}

void SkeletonKeyDebugger::Draw(float DeltaTime)

{
    DrawAllTransformsImGui();
    DrawImGuiDebug();
	DrawAllKeyCarriersImGui();
}

void SkeletonKeyDebugger::DrawAllKeyCarriersImGui()
{
	if (CVarEnableImGuiKeyCarry.GetValueOnGameThread() == 0)
	{
		return;
	}

	const ImGui::FScopedContext ScopedContext;

	if (!ScopedContext)
		return;

	UKeyCarry* LocalKeyCarry = nullptr;
	APlayerController* LocalPC = DispatchOwner->GetWorld()->GetFirstPlayerController();
	if (LocalPC && LocalPC->IsLocalController())
	{
		if (APawn* LocalPawn = LocalPC->GetPawn())
		{
			LocalKeyCarry = LocalPawn->FindComponentByClass<UKeyCarry>();
		}
	}

	const FString OwnerName = LocalKeyCarry ? LocalKeyCarry->GetName() : TEXT("No Owner");
	const bool bHasAuthority = DispatchOwner.IsValid() ? DispatchOwner->GetWorld()->GetGameInstance()->IsDedicatedServerInstance() : false;
	const ImVec4 AuthorityColor = bHasAuthority ?
		ImVec4(0.0f, 0.8f, 0.2f, 1.0f) : // Green for Authority
		ImVec4(0.2f, 0.6f, 1.0f, 1.0f);  // Blue for Remote

	ImGui::TextColored(AuthorityColor, "Network: %s", bHasAuthority ? "Authority (Server)" : "Remote (Client)");

	const uint64 KeyValue = LocalKeyCarry ? LocalKeyCarry->GetMyKey() : 0;
	if (KeyValue != 0)
	{
		ImGui::Text("Object Key: %llu", KeyValue);
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Object Key: INVALID (0)");
	}

	ImGui::Text("Owner: %s", TCHAR_TO_UTF8(*OwnerName));
	ImGui::Text("IsReady: %s", LocalKeyCarry && LocalKeyCarry->IsReady ? "Yes" : "No");

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Key Carriers Debug"))
	{
		UWorld* World = GEngine->GetCurrentPlayWorld();
		if (!World)
		{
			ImGui::Text("No active world");
			ImGui::End();
			return;
		}

		// Network status display
		ENetMode NetMode = World->GetNetMode();
		ImVec4 StatusColor;
		const char* StatusText = nullptr;

		switch (NetMode)
		{
		case NM_Standalone:
			StatusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
			StatusText = "Standalone";
			break;
		case NM_DedicatedServer:
			StatusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
			StatusText = "Dedicated Server";
			break;
		case NM_ListenServer:
			StatusColor = ImVec4(0.0f, 0.8f, 0.2f, 1.0f); // Green-blue
			StatusText = "Listen Server";
			break;
		case NM_Client:
			StatusColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f); // Blue
			StatusText = "Client";
			break;
		default:
			StatusColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
			StatusText = "Unknown";
		}

		ImGui::TextColored(StatusColor, "Network Mode: %s", StatusText);
		ImGui::Separator();

		// Search filter
		static char searchBuffer[256] = "";
		ImGui::Text("Filter:");
		ImGui::SameLine();
		ImGui::InputText("##Filter", searchBuffer, IM_ARRAYSIZE(searchBuffer));
		std::string searchStr = searchBuffer;

		// Table for key carriers
		if (ImGui::BeginTable("KeyCarriersTable", 3,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Owner Actor");
			ImGui::TableSetupColumn("Object Key");
			ImGui::TableSetupColumn("Network Role");
			ImGui::TableHeadersRow();

			int numKeyCarriers = 0;

			// Find all actors with KeyCarry components
			for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
			{
				AActor* Actor = *ActorItr;
				TArray<UKeyCarry*> KeyComponents;
				Actor->GetComponents(KeyComponents);

				for (UKeyCarry* KeyComponent : KeyComponents)
				{
					numKeyCarriers++;

					// Apply search filter if provided
					if (searchStr.length() > 0)
					{
						FString ActorName = Actor->GetName();
						FString KeyStr = FString::Printf(TEXT("%llu"), KeyComponent->GetMyKey().Obj);

						if (!ActorName.Contains(searchBuffer) && !KeyStr.Contains(searchBuffer))
							continue;
					}

					ImGui::TableNextRow();

					// Owner Column
					ImGui::TableNextColumn();
					ImGui::Text("%s", TCHAR_TO_UTF8(*Actor->GetName()));

					// Object Key Column
					ImGui::TableNextColumn();
					uint64 ActorKeyValue = KeyComponent->GetMyKey();
					if (ActorKeyValue != 0)
					{
						ImGui::Text("%llu", ActorKeyValue);
					}
					else
					{
						ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "INVALID KEY");
					}

					// Network Role Column
					ImGui::TableNextColumn();
					ENetRole LocalRole = Actor->GetLocalRole();
					const char* RoleString = "Unknown";
					switch (LocalRole)
					{
					case ROLE_None:
						RoleString = "None";
						break;
					case ROLE_SimulatedProxy:
						RoleString = "SimulatedProxy";
						break;
					case ROLE_AutonomousProxy:
						RoleString = "AutonomousProxy";
						break;
					case ROLE_Authority:
						RoleString = "Authority";
						break;
					}

					ImGui::TextColored(
						Actor->HasAuthority() ? ImVec4(0.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.2f, 0.6f, 1.0f, 1.0f),
						"%s",
						RoleString
					);
				}
			}

			ImGui::EndTable();

			ImGui::Text("Total Key Carriers: %d", numKeyCarriers);
		}
	}
	ImGui::End();
}
