// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class TCPLogging : ModuleRules
    {
        public TCPLogging(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "Sockets",
                    "Networking",
					// ... add other public dependencies that you statically link with here ...
				}
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Analytics",
					// ... add private dependencies that you statically link with here ...
				}
                );

            PublicIncludePathModuleNames.Add("Analytics");
        }
    }
}
