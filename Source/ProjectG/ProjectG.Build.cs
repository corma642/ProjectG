// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

/// <summary>ProjectG 런타임 모듈의 UE 및 Android 빌드 의존성을 정의합니다.</summary>
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
            "ApplicationCore",
            "EnhancedInput",
            "DeveloperSettings",
            "UMG",
            "Slate",
            "SlateCore",
            "Niagara"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            // 레벨 전환 로딩 화면(UPGLoadingScreenSubsystem)
            "MoviePlayer"
        });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // UE 5.6 기본 Android 템플릿이 추가하는 미사용 Play Games/OBB 선언을 프로젝트 범위에서 제거합니다.
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "ProjectG_UPL.xml"));
        }
    }
}
