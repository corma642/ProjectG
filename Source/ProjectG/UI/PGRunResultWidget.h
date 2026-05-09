// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGRunResultWidget.generated.h"

class UBackgroundBlur;
class UButton;
class UTextBlock;
class UWidget;

/**
 * 런 실패 결과 화면의 로직 위젯입니다.
 * BP가 레이아웃과 스타일을 담당하고, C++은 결과 데이터 주입, 버튼 처리, 등장 애니메이션만 담당합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API UPGRunResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 결과 위젯 생성 직후 현재 런의 실패 원인, 시간, 거리, 점수, 영수증, 배달 완료 수를 전달합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run Result")
	void ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// BP 상속 위젯에서 결과 데이터 수신 후 추가 연출이나 스타일 후처리를 붙일 수 있는 확장 지점입니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "ProjectG | Run Result")
	void OnRunResultConfigured(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered);

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UBackgroundBlur> BackdropBlur;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> BackdropDim;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> ResultRootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> ResultPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> TitleText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> ScoreRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> DistanceRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> ReceiptsRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> OrdersRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UTextBlock> ScoreValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UTextBlock> DistanceValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UTextBlock> ReceiptsValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UTextBlock> OrdersValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UButton> HomeButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> RetryButtonPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Result")
	TObjectPtr<UWidget> HomeButtonPanel;

private:
	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleRetryPressed();

	UFUNCTION()
	void HandleRetryReleased();

	UFUNCTION()
	void HandleHomeClicked();

	UFUNCTION()
	void HandleHomePressed();

	UFUNCTION()
	void HandleHomeReleased();

	void ApplyCachedResultTexts();
	void StartBackdropAnimation();
	void UpdateBackdropAnimation(float DeltaTime);
	void StartEntryAnimation();
	void ResetEntryAnimationState();
	void UpdateEntryAnimation(float DeltaTime);
	void ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& Offset, float Opacity) const;
	void SetActionButtonsEnabled(bool bEnabled) const;
	void ApplyWidgetScale(UWidget* Widget, float Scale) const;
	FText FormatNumber(int32 Value) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> AnimatedRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> AnimatedButtons;

	FName CachedFailureReason = NAME_None;
	float CachedRunTimeSeconds = 0.0f;
	float CachedDistanceMeters = 0.0f;
	int32 CachedScore = 0;
	int32 CachedReceipts = 0;
	int32 CachedOrdersDelivered = 0;

	float BackdropAnimationElapsed = 0.0f;
	float EntryAnimationElapsed = 0.0f;
	bool bBackdropAnimationFinished = true;
	bool bResultEntryStarted = false;
	bool bEntryAnimationFinished = true;
};
