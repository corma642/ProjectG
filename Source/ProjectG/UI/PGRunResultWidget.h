// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "PGRunResultWidget.generated.h"

class UBorder;
class UBackgroundBlur;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UScaleBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * 런 실패 후 결과 화면을 C++에서 기본 구성하는 위젯입니다.
 * BP 상속 클래스는 레이아웃 로직을 다시 만들지 않고 배경, 아이콘, 폰트, 색상 리소스만 교체하는 용도로 사용합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API UPGRunResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPGRunResultWidget(const FObjectInitializer& ObjectInitializer);

	// 결과 위젯 생성 직후 현재 런의 실패 원인, 시간, 거리, 점수를 전달하고 UI 텍스트/등장 애니메이션을 갱신합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run Result")
	void ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// BP 상속 위젯에서 결과 데이터 수신 후 추가 연출이나 리소스 교체 후처리를 붙일 수 있는 확장 지점입니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "ProjectG | Run Result")
	void OnRunResultConfigured(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered);

	// 게임오버 카드 배경 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateBrush BackgroundBrush;

	// 점수 행 아이콘 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateBrush ScoreIconBrush;

	// 거리 행 아이콘 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateBrush DistanceIconBrush;

	// 영수증 행 아이콘 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateBrush ReceiptsIconBrush;

	// 배달 완료 행 아이콘 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateBrush OrdersIconBrush;

	// 재시작 버튼 아이콘 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateBrush RetryIconBrush;

	// 홈 버튼 아이콘 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateBrush HomeIconBrush;

	// 제목 텍스트 폰트입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateFontInfo TitleFont;

	// 결과 행 라벨 텍스트 폰트입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateFontInfo LabelFont;

	// 결과 행 값 텍스트 폰트입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateFontInfo ValueFont;

	// 버튼 텍스트 폰트입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateFontInfo ButtonFont;

	// 제목 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateColor TitleColor;

	// 결과 행 라벨 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateColor LabelColor;

	// 결과 행 값 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateColor ValueColor;

	// 재시작 버튼 텍스트 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateColor RetryButtonColor;

	// 홈 버튼 텍스트 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Style")
	FSlateColor HomeButtonColor;

	// 버튼 기본 상태 배경 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button")
	FSlateBrush ButtonNormalBrush;

	// 버튼에 마우스를 올렸을 때의 배경 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button")
	FSlateBrush ButtonHoveredBrush;

	// 버튼을 누르는 동안의 배경 이미지입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button")
	FSlateBrush ButtonPressedBrush;

	// 버튼 기본 상태 배경 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button")
	FSlateColor ButtonNormalTint;

	// 버튼에 마우스를 올렸을 때의 배경 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button")
	FSlateColor ButtonHoveredTint;

	// 버튼을 누르는 동안의 배경 색상입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button")
	FSlateColor ButtonPressedTint;

	// 버튼 기본 상태 크기 배율입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button", meta = (ClampMin = "0.5", UIMin = "0.8", UIMax = "1.2"))
	float ButtonNormalScale = 1.0f;

	// 버튼에 마우스를 올렸을 때의 크기 배율입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button", meta = (ClampMin = "0.5", UIMin = "0.8", UIMax = "1.2"))
	float ButtonHoveredScale = 1.0f;

	// 버튼을 누르는 동안의 크기 배율입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Button", meta = (ClampMin = "0.5", UIMin = "0.8", UIMax = "1.2"))
	float ButtonPressedScale = 0.96f;

	// 결과 화면 뒤 게임 화면에 적용할 최종 블러 강도입니다. 0이면 블러 없이 딤만 적용합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Backdrop", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float BackdropBlurStrength = 8.0f;

	// 결과 화면 뒤 게임 화면을 어둡게 덮는 색상입니다. 알파값이 최종 어두워지는 정도입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Backdrop")
	FLinearColor BackdropTintColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);

	// 블러와 딤이 목표값까지 도달하는 시간입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Backdrop", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "2.0"))
	float BackdropAnimationDuration = 0.45f;

	// 블러/딤 진행률이 이 값에 도달하면 결과 카드 등장 애니메이션을 시작합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Backdrop", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BackdropResultStartAlpha = 0.8f;

	// 결과 카드의 내부 디자인 기준 크기입니다. 기본값은 실제 결과창 종이 배경 기준으로 확정한 600x1200입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout")
	FVector2D PanelDesignSize = FVector2D(600.0f, 1200.0f);

	// 결과 카드가 화면 가장자리와 유지할 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout")
	FMargin PanelViewportPadding = FMargin(16.0f);

	// 종이 배경 이미지 안쪽에 실제 텍스트/버튼이 들어갈 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout")
	FMargin PanelContentPadding = FMargin(64.0f, 130.0f, 64.0f, 95.0f);

	// 제목 아래 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "160.0"))
	float TitleBottomPadding = 90.0f;

	// 결과 행 영역 아래 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "180.0"))
	float RowsBottomPadding = 100.0f;

	// 각 결과 행 사이의 세로 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "96.0"))
	float ResultRowBottomPadding = 50.0f;

	// 버튼 사이와 버튼 아래쪽에 적용할 세로 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "80.0"))
	float ButtonBottomPadding = 24.0f;

	// 버튼 높이입니다. 0 이하이면 내부 콘텐츠 기준 자동 높이를 사용합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "180.0"))
	float ActionButtonHeight = 96.0f;

	// 결과 행 아이콘 크기입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout")
	FVector2D ResultRowIconSize = FVector2D(56.0f, 56.0f);

	// 결과 행 아이콘과 라벨 사이의 간격입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "96.0"))
	float ResultRowIconLabelSpacing = 24.0f;

	// 결과 행 라벨과 값 사이에 유지할 최소 간격입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "96.0"))
	float ResultRowLabelValueMinSpacing = 12.0f;

	// 버튼 아이콘 크기입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout")
	FVector2D ActionButtonIconSize = FVector2D(54.0f, 54.0f);

	// 버튼 내부 좌우 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "80.0"))
	float ActionButtonHorizontalPadding = 20.0f;

	// 버튼 내부 상하 여백입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "48.0"))
	float ActionButtonVerticalPadding = 8.0f;

	// 버튼 아이콘과 텍스트 사이의 간격입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Run Result | Layout", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "96.0"))
	float ActionButtonIconTextSpacing = 24.0f;

private:
	void InitializeDefaultStyle();
	void BuildDefaultLayout();
	void StartBackdropAnimation();
	void UpdateBackdropAnimation(float DeltaTime);
	void StartEntryAnimation();
	void ResetEntryAnimationOffsets();
	void UpdateEntryAnimation(float DeltaTime);
	void ApplyWidgetOffset(UWidget* Widget, const FVector2D& Offset) const;
	void ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& Offset, float Opacity) const;
	void SetActionButtonsEnabled(bool bEnabled) const;
	void ApplyButtonScale(UButton* Button, float Scale) const;
	UTextBlock* CreateResultRow(UVerticalBox* ParentBox, const FSlateBrush& IconBrush, const FText& LabelText);
	UButton* CreateActionButton(UVerticalBox* ParentBox, const FSlateBrush& IconBrush, const FText& ButtonText, const FSlateColor& TextColor);
	FText FormatNumber(int32 Value) const;

	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleRetryHovered();

	UFUNCTION()
	void HandleRetryUnhovered();

	UFUNCTION()
	void HandleRetryPressed();

	UFUNCTION()
	void HandleRetryReleased();

	UFUNCTION()
	void HandleHomeClicked();

	UFUNCTION()
	void HandleHomeHovered();

	UFUNCTION()
	void HandleHomeUnhovered();

	UFUNCTION()
	void HandleHomePressed();

	UFUNCTION()
	void HandleHomeReleased();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBackgroundBlur> BackdropBlur;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackdropDim;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> PanelScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ResultRowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DistanceValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReceiptsValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OrdersValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HomeButton;

	TArray<TWeakObjectPtr<UWidget>> AnimatedRows;
	TArray<TWeakObjectPtr<UWidget>> AnimatedButtons;

	float BackdropAnimationElapsed = 0.0f;
	float EntryAnimationElapsed = 0.0f;
	bool bLayoutBuilt = false;
	bool bBackdropAnimationFinished = true;
	bool bResultEntryStarted = false;
	bool bEntryAnimationFinished = true;
};
