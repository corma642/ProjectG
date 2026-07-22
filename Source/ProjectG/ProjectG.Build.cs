// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ProjectG : ModuleRules
{
    public ProjectG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 프로젝트 공개 폴더 경로 추가
        PublicIncludePaths.Add("ProjectG");

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "DeveloperSettings",
            "UMG",
            "Slate",
            "SlateCore",
            "Niagara"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // UE 5.6 기본 Android 템플릿이 추가하는 미사용 Play Games/OBB 선언을 프로젝트 범위에서 제거합니다.
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "ProjectG_UPL.xml"));
        }

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
