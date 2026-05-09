// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Cargo/PGCargoStackComponent.h"
#include "PGCargoDeliveryItemWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UWidget;

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
	FPGCargoDeliveryItemExitFinishedSignature OnExitAnimationFinished;

	void ConfigureCargoItem(const FPGActiveCargoInfo& CargoInfo);
	void PlayEnterAnimation();
	void PlayExitAnimation();

	int32 GetCargoInstanceId() const { return CargoInstanceId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UTextBlock> RemainingTimeText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Cargo HUD")
	TObjectPtr<UProgressBar> DeliveryProgressBar;

private:
	void ApplyAnimationState(float Alpha, bool bExiting);

	int32 CargoInstanceId = INDEX_NONE;
	bool bIsEntering = false;
	bool bIsExiting = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.16f;
};
