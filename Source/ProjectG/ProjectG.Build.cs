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
            "MoviePlayer",
            // 플레이 지표 이벤트 기록(UPGAnalyticsSubsystem). 제공자 모듈은 Config/DefaultEngine.ini [Analytics*] 섹션이 정한다.
            "Analytics"
        });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // UE 5.6 기본 Android 템플릿의 미사용 Play Games/OBB 선언 제거, AdMob 의존성·Java 래퍼 복사, 광고 앱 ID 매니페스트 등록(UPGAdsSubsystem).
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "ProjectG_UPL.xml"));
            // Java 광고 래퍼를 JNI로 부르는 FJavaWrapper(Android/AndroidJNI.h)
            PrivateDependencyModuleNames.Add("Launch");
        }
    }
}
