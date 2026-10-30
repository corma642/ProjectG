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
	/** 결과 위젯 생성 직후 실패 원인, 초 단위 시간, 미터 단위 거리, 점수, 영수증, 완료 배달 수를 전달합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | UI")
	void ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered);

	/** 이어하기 버튼 상태를 넣습니다. bOffered가 false면 버튼 영역을 접고, bAffordable이 false면 비용을 보여 준 채 버튼을 흐리게 잠급니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | UI")
	void ConfigureContinueOption(bool bOffered, bool bAffordable, int32 ReceiptCost);

	/** 뒤로가기로 Home 버튼과 같은 동작을 수행합니다. 버튼이 아직 잠겨 있으면(진입 애니메이션 중) false를 반환합니다. */
	bool RequestHome();

protected:
	/** BP 버튼·행 캐시를 연결하고 ConfigureRunResult 이전에도 안전한 초기 표현을 준비합니다. */
	virtual void NativeConstruct() override;

	/** Retry/Home 버튼 delegate와 임시 애니메이션 캐시를 제거 전에 해제합니다. */
	virtual void NativeDestruct() override;

	/** backdrop과 결과 행의 시차 진입 애니메이션이 끝날 때까지 상태를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** BP 상속 위젯에서 결과 데이터 수신 후 추가 연출이나 스타일 후처리를 붙이는 확장 지점입니다. */
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

	/** 제목 아래에 실패 원인(적재물 추락/트랙 추락)을 한 줄로 알리는 필수 텍스트입니다. 모르는 원인이면 비웁니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> FailureReasonText;

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

	/** 정수 런 점수를 표시하는 필수 결과 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ScoreValueText;

	/** 런 진행 거리를 미터 단위로 표시하는 필수 결과 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> DistanceValueText;

	/** 이번 런에서 획득한 영수증 수를 표시하는 필수 결과 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ReceiptsValueText;

	/** 이번 런에서 완료한 배달 수를 표시하는 필수 결과 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> OrdersValueText;

	/** 영수증을 내고 실패 지점부터 이어하기를 요청하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> ContinueButton;

	/** 이어하기 비용(영수증 수)을 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ContinueCostText;

	// 이어하기 버튼 전체 패널입니다. 이어하기를 제안하지 않을 때 통째로 접고, 진입 애니메이션도 이 패널에 적용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> ContinueButtonPanel;

	/** 같은 맵에서 새 런을 시작하도록 요청하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> RetryButton;

	/** 결과를 닫고 Home 레벨로 복귀하도록 요청하는 필수 버튼입니다. */
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
	void HandleContinueClicked();

	UFUNCTION()
	void HandleContinuePressed();

	UFUNCTION()
	void HandleContinueReleased();

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
	static FText GetFailureReasonText(FName FailureReason);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> AnimatedRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> AnimatedButtons;

	float CachedDistanceMeters = 0.0f;
	int32 CachedScore = 0;
	int32 CachedReceipts = 0;
	int32 CachedOrdersDelivered = 0;
	// 이어하기를 제안하고 잔액도 충분할 때만 true. 진입 애니메이션이 끝나 버튼을 켤 때 이 값까지 만족해야 이어하기 버튼이 켜진다.
	bool bCanContinue = false;

	float BackdropAnimationElapsed = 0.0f;
	float EntryAnimationElapsed = 0.0f;
	bool bBackdropAnimationFinished = true;
	bool bResultEntryStarted = false;
	bool bEntryAnimationFinished = true;
};
