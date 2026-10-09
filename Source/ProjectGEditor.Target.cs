// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/// <summary>에디터 개발과 Blueprint 검증에 사용하는 ProjectG Editor 타깃을 정의합니다.</summary>
public class ProjectGEditorTarget : TargetRules
{
	public ProjectGEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("ProjectG");
		ExtraModuleNames.Add("ProjectGEditor");
	}
}
