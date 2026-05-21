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
	UFUNCTION(BlueprintCallable, Category = "ProjectG | UI")
	void ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// BP 상속 위젯에서 결과 데이터 수신 후 추가 연출이나 스타일 후처리를 붙일 수 있는 확장 지점입니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "ProjectG | UI")
	void OnRunResultConfigured(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered);

	// 결과 화면 전체 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RootPanel;

	// 실패 직후 뒤 게임 화면을 흐리게 만드는 전체 화면 블러입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UBackgroundBlur> BackdropBlur;

	// 결과창 집중도를 높이는 반투명 딤 레이어입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> BackdropDim;

	// 배경 이미지, 제목, 결과 행, 버튼을 모두 담는 결과창 컨테이너입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ResultRootPanel;

	// 종이/패널 배경 이미지 위젯입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ResultPanel;

	// DELIVERY FAILED 제목 텍스트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> TitleText;

	// 점수 결과 행입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ScoreRow;

	// 진행 거리 결과 행입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> DistanceRow;

	// 이번 런에서 획득한 영수증 결과 행입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ReceiptsRow;

	// 배달 완료한 적재물 개수 결과 행입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> OrdersRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ScoreValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> DistanceValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ReceiptsValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> OrdersValueText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> HomeButton;

	// Retry 버튼 전체 패널입니다. 버튼 진입 애니메이션은 Button이 아니라 이 패널에 적용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RetryButtonPanel;

	// Home 버튼 전체 패널입니다. 버튼 진입 애니메이션은 Button이 아니라 이 패널에 적용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
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
