// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGCargoDeliveryItemWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UWidget;
struct FPGActiveCargoInfo;

/** 카드 퇴장 애니메이션이 끝나 HUD 풀이 위젯을 회수할 수 있을 때 발생합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPGCargoDeliveryItemExitFinishedSignature, class UPGCargoDeliveryItemWidget*);

/**
 * HUD 적재물 리스트에 표시되는 개별 배달 카드의 로직 위젯입니다.
 * BP가 카드 레이아웃과 스타일을 구성하고, C++은 썸네일/남은 시간/진행도와 등장/퇴장 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGCargoDeliveryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 퇴장이 끝난 카드 자신을 HUD 카드 풀에 반환하도록 알립니다. */
	FPGCargoDeliveryItemExitFinishedSignature OnExitAnimationFinished;

	/** 활성 적재물 ID, 썸네일, 남은 시간, 진행률과 손실 임박 비율을 카드에 반영합니다. */
	void ConfigureCargoItem(const FPGActiveCargoInfo& CargoInfo);

	/** 손실 임박 비율(0~1, 바구니 테두리 대비)을 받아 경고 문턱을 넘으면 카드 테두리를 깜빡이고, 아래면 숨깁니다. */
	UFUNCTION(BlueprintCallable, Category = "Cargo HUD")
	void SetDangerRatio(float DangerRatio);

	/** 카드가 목록에 추가될 때 짧은 진입 애니메이션을 시작합니다. */
	void PlayEnterAnimation();

	/** 카드 제거 전에 퇴장 애니메이션을 시작하고 완료 delegate를 발생시킵니다. */
	void PlayExitAnimation();

protected:
	/** BP 카드의 초기 투명도를 진입 전 상태로 맞추고 경고 테두리를 현재 상태에 맞춥니다. */
	virtual void NativeConstruct() override;

	/** 경고 테두리 펄스와, 진입 또는 퇴장 중인 카드의 opacity·이동 상태를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 카드 전체 opacity와 이동 애니메이션을 적용하는 필수 BP 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UWidget> RootPanel;

	// 적재물 DataAsset의 예열된 썸네일을 표시하는 필수 이미지입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UImage> ThumbnailImage;

	// 배달 완료까지 남은 시간을 초 단위 텍스트로 표시합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UTextBlock> RemainingTimeText;

	// 적재물의 0~1 배달 진행률을 표시하는 필수 진행 바입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UProgressBar> DeliveryProgressBar;

	// 이 적재물이 바구니 테두리로 밀려날 때 카드 둘레에서 깜빡이는 경고 테두리입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UWidget> DangerFrame;

private:
	void ApplyAnimationState(float Alpha, bool bExiting);

	int32 CargoInstanceId = INDEX_NONE;
	// 짧은 주기 갱신에서 같은 값을 다시 그리지 않기 위한 마지막 표시 상태입니다.
	int32 LastDisplayedRemainingSeconds = INDEX_NONE;
	TWeakObjectPtr<UTexture2D> LastDisplayedThumbnail;
	bool bIsEntering = false;
	bool bIsExiting = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.16f;
	// 경고 테두리 밝기(0이면 숨김, 문턱에서 테두리까지 0.4~1)와 펄스 시간입니다.
	float DangerAlpha = 0.0f;
	float DangerPulseElapsed = 0.0f;
	// Cargo > CargoDangerWarningRatio 복사본입니다.
	float CargoDangerWarningRatio = 1.0f;
};
