// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/// <summary>Android Shipping을 포함한 ProjectG 게임 실행 타깃을 정의합니다.</summary>
public class ProjectGTarget : TargetRules
{
	public ProjectGTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("ProjectG");
	}
}
