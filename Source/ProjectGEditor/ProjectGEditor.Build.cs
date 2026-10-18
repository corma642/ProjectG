// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/// <summary>헤드리스 에디터 자동화에서 위젯 렌더링·히트 테스트 검증에 쓰는 에디터 전용 모듈입니다.</summary>
public class ProjectGEditor : ModuleRules
{
    public ProjectGEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "ImageCore",
            "Localization",
            "ProjectG",
            "RenderCore",
            "Slate",
            "SlateCore",
            "UMG",
            "UnrealEd"
        });
    }
}
