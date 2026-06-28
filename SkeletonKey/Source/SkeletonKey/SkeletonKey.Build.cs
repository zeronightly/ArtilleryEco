// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class SkeletonKey : ModuleRules
{
	public SkeletonKey(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/SkeletonKey"),
				Path.Combine(PluginDirectory,"Source/SkeletonKey/LibSeq"),
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/SkeletonKey/LibSeq"),
				// ... add other private include paths required here ...
			}
			);

		PublicDefinitions.Add("SEQ_NO_DEBUG=1");
		PublicDependencyModuleNames.AddRange(
			new string[]
			{	
				"Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "ApplicationCore",
                "GameplayTasks",
                "GameplayTags", "ImGui", "ImGuiLibrary"
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"ApplicationCore",
				"GameplayTasks",
				"GameplayTags", "ImGui", "ImGuiLibrary"
				// ... add private dependencies that you statically link with here ...	
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
