// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PGEditorAutomationLibrary.generated.h"

class UUserWidget;

/**
 * 헤드리스 에디터 Python 스크립트가 위젯 작업 결과를 검증할 때 쓰는 에디터 전용 함수 모음입니다.
 * 기본 Python API에 없는 위젯 인스턴스 생성, 게임 진입 함수 호출, 오프스크린 렌더링, 히트 테스트만 제공합니다.
 * 위젯 트리 편집은 엔진의 unreal.EditorUtilityLibrary.add_source_widget을 그대로 씁니다.
 * 런타임 모듈과 게임 빌드에는 포함되지 않습니다.
 */
UCLASS()
class UPGEditorAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 에디터 월드에 위젯 인스턴스를 만듭니다. 렌더링·히트 테스트 전에 Python에서 상태를 바꿀 수 있도록 인스턴스를 돌려줍니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static UUserWidget* CreateWidgetInstance(TSubclassOf<UUserWidget> WidgetClass);

	/**
	 * 게임이 위젯을 화면에 올린 직후 부르는 C++ 진입 함수(열기 애니메이션, 튜토리얼 페이지 연결 등)를 대신 호출합니다.
	 * 이 함수들은 UFUNCTION이 아니라 Python에서 직접 부를 수 없습니다. 대상: Pause, Settings, Shop, ReceiptShop, Tutorial, 배달 카드, 점수 표시, 화면 전환 페이드.
	 * 그릴 때마다 NativeConstruct가 위젯을 진입 전 상태로 되돌리므로, 그리기·히트 테스트 호출 직전에 매번 부릅니다.
	 * 진입 함수가 없는 위젯이면 아무것도 하지 않고 false를 반환합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static bool PlayWidgetEntry(UUserWidget* Widget);

	/**
	 * 위젯을 Size(px) 크기의 화면에 배경색 위로 한 번 그려 PNG로 저장합니다.
	 * AdvanceSeconds(초)가 0보다 크면 그만큼 시간이 흐른 것으로 NativeTick을 돌린 뒤의 화면을 그립니다. 진입 애니메이션보다 길게 주면 끝난 상태가 나옵니다.
	 * 실제 RHI가 필요하므로 -NullRHI 실행에서는 false를 반환합니다(-RenderOffscreen으로 실행).
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static bool RenderWidgetToPng(UUserWidget* Widget, FIntPoint Size, FLinearColor Background, const FString& PngPath, float AdvanceSeconds = 0.0f);

	/**
	 * 위젯을 Size(px) 크기의 화면에 배치했을 때 각 Point(px)에서 터치/클릭을 받는 최상단 위젯 이름을 같은 순서로 반환합니다.
	 * 위젯 자신(루트)이 입력을 받는 지점은 그 인스턴스 이름, 입력을 받는 위젯이 없는 지점은 None이고, 그릴 수 없으면 빈 배열입니다.
	 * 장식 위젯이 버튼을 가리는지 확인할 때 씁니다. AdvanceSeconds는 RenderWidgetToPng와 같습니다.
	 * bIgnoreEnabledState가 false면 게임과 같이 비활성 위젯에서 경로가 끊겨 그 부모가 반환됩니다.
	 * true면 비활성 여부와 무관하게 그 지점의 최상단 위젯을 반환하므로, 잠긴 버튼이 가려졌는지도 확인할 수 있습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static TArray<FName> FindHitWidgetNames(UUserWidget* Widget, FIntPoint Size, const TArray<FVector2D>& Points, float AdvanceSeconds = 0.0f, bool bIgnoreEnabledState = false);

	/**
	 * 위젯을 그린 뒤 원하는 크기가 배정된 크기보다 큰(잘리거나 이웃과 겹치는) TextBlock을 "이름: desired WxH > allotted WxH ("문구")" 형식으로 반환합니다.
	 * 현지화로 문구 길이가 바뀌어 UI가 밀리는지 확인할 때 씁니다. 숨겨진 TextBlock과 중첩 UserWidget 안의 TextBlock도 검사합니다.
	 * Tolerance(px)보다 작은 초과는 무시합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static TArray<FString> FindOverflowingTextBlocks(UUserWidget* Widget, FIntPoint Size, float AdvanceSeconds = 0.0f, float Tolerance = 1.0f);

	/**
	 * 디자이너가 만든 텍스트처럼 패키지 네임스페이스와 Key를 가진 현지화 가능 텍스트를 만듭니다.
	 * Python의 문자열→Text 변환은 문화권 불변(INVTEXT)이라 현지화 수집에서 빠지므로, Python으로 WBP 문구를 넣을 때는 이 함수를 거칩니다.
	 * PackageOwner는 그 WBP(에셋)이고, 네임스페이스가 없으면 새로 만듭니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static FText MakeLocalizedText(UObject* PackageOwner, const FString& Key, const FString& SourceString);

	/**
	 * 게임 텍스트의 현지화 미리보기를 해당 문화권으로 켭니다(에디터 언어는 바뀌지 않음). 빈 문자열이면 끕니다.
	 * 에디터 프로세스는 게임 locres를 자동으로 읽지 않으므로, 위젯 QA에서 번역된 문구를 그리려면 먼저 호출합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static void SetGameLocalizationPreview(const FString& Culture);

	/**
	 * Localization Dashboard가 읽는 게임 현지화 타깃을 Config/DefaultEditor.ini에 등록합니다(같은 이름이 있으면 갱신).
	 * Cultures의 NativeCultureIndex번째가 원어 문화권입니다. 수집 범위는 Source·Config의 h/cpp/ini와 Content의 uasset/umap입니다.
	 * PackageExcludePaths는 수집에서 뺄 Content 경로 와일드카드(예: Content/L10N/*, 외부 에셋 팩)입니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static bool RegisterGameLocalizationTarget(const FString& TargetName, const TArray<FString>& Cultures, int32 NativeCultureIndex, const TArray<FString>& PackageExcludePaths);

	/**
	 * 이후 그리기·히트 테스트에서 WBP의 SafeZone이 절개 영역(노치·펀치홀) 여백을 PixelPadding(px)만큼 두도록 흉내 냅니다.
	 * Size는 그릴 화면 크기이고, 여백이 모두 0이면 흉내를 끕니다. 에디터는 실제 기기 여백을 모르므로 QA가 값을 정해 넣습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static void SetSafeZoneEmulation(FIntPoint Size, FMargin PixelPadding);

	/**
	 * 게임이 레벨 전환 중 띄우는 로딩 화면(UPGLoadingScreenSubsystem, 설정 Go Deliver! Settings > Loading Screen)을 같은 함수로 만들어 PNG로 그립니다.
	 * 로딩 화면은 MoviePlayer 위젯이라 PIE·헤드리스에서 볼 수 없으므로 모양 확인은 이 캡처로 합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Editor Automation")
	static bool RenderLoadingScreenToPng(FIntPoint Size, const FString& PngPath);
};
