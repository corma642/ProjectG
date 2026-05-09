// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Cargo/PGCargoStackComponent.h"
#include "PGRunHUDWidget.generated.h"

class UButton;
class UPanelWidget;
class UPGCargoDeliveryItemWidget;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGRunHUDPauseRequestedSignature);

/**
 * 러닝 중 점수, 거리, 영수증, 적재물 배송 리스트를 표시하는 HUD 로직 위젯입니다.
 * BP가 레이아웃과 스타일을 담당하고, C++은 데이터 갱신과 Pause 버튼, 간단한 진입/퇴장 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGRunHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGRunHUDPauseRequestedSignature OnPauseRequested;

	void SetCargoItemWidgetClass(TSubclassOf<UPGCargoDeliveryItemWidget> InCargoItemWidgetClass);
	void SetScore(int32 Score);
	void SetDistanceMeters(float DistanceMeters);
	void SetReceipts(int32 Receipts);
	void SetPauseButtonEnabled(bool bEnabled) const;
	void RefreshCargoList(const TArray<FPGActiveCargoInfo>& ActiveCargoInfos);
	void PlayEnterAnimation();
	void PlayExitAnimation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run HUD")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run HUD")
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run HUD")
	TObjectPtr<UTextBlock> DistanceText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run HUD")
	TObjectPtr<UTextBlock> ReceiptsText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run HUD")
	TObjectPtr<UButton> PauseButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run HUD")
	TObjectPtr<UPanelWidget> CargoListBox;

private:
	UFUNCTION()
	void HandlePauseClicked();

	void ApplyAnimationState(float Alpha, bool bExiting);
	UPGCargoDeliveryItemWidget* AcquireCargoItemWidget();
	void HandleCargoItemExitFinished(UPGCargoDeliveryItemWidget* ItemWidget);

	UPROPERTY(Transient)
	TSubclassOf<UPGCargoDeliveryItemWidget> CargoItemWidgetClass;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UPGCargoDeliveryItemWidget>> ActiveItemWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGCargoDeliveryItemWidget>> InactiveItemPool;

	bool bIsEntering = false;
	bool bIsExiting = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.22f;
};
