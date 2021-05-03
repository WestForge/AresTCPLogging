// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TCPLoggingEditor : ModuleRules
{
    public TCPLoggingEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add("TCPLoggingEditor/Private");

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Analytics",
                "AnalyticsVisualEditing",
                "Engine",
                "Projects",
                "DeveloperSettings"
            }
            );

        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "Settings"
            }
        );
    }
}
