// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SplitScreen : ModuleRules
{
	public SplitScreen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // PublicIncludePaths.AddRange(new string[] { "SplitScreen" });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
