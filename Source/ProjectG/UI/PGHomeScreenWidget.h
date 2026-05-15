// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGHomeScreenWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FPGHomeStartDeliveryRequestedSignature);
DECLARE_MULTICAST_DELEGATE(FPGHomeSettingsRequestedSignature);
DECLARE_MULTICAST_DELEGATE(FPGHomeHelpRequestedSignature);
DECLARE_MULTICAST_DELEGATE(FPGHomeShopRequestedSignature);

/**
 * 홈 화면 Blueprint Widget의 로직 클래스입니다.
 * 레이아웃과 스타일은 BP가 담당하고, C++은 버튼 이벤트와 퇴장 애니메이션만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGHomeScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGHomeStartDeliveryRequestedSignature OnStartDeliveryRequested;
	FPGHomeSettingsRequestedSignature OnSettingsRequested;
	FPGHomeHelpRequestedSignature OnHelpRequested;
	FPGHomeShopRequestedSignature OnShopRequested;

	// START DELIVERY 이후 홈 화면 요소들이 각 방향으로 빠지며 사라지는 애니메이션을 시작합니다.
	void PlayExitAnimation(float Duration);

	// 리소스 예열 대기나 설정 화면 표시 중 홈 버튼 입력을 잠그거나 다시 엽니다.
	void SetHomeButtonsEnabled(bool bEnabled) const;

	// Home 화면에 표시할 누적 영수증 보유량을 갱신합니다.
	void SetReceiptBalance(int32 Balance) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UImage> LogoImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> MenuPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> StartDeliveryButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> ShopButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> HelpButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> ReceiptPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

private:
	UFUNCTION()
	void HandleStartDeliveryClicked();

	UFUNCTION()
	void HandleShopClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleHelpClicked();

	void StartEntryAnimation();
	void ApplyEntryAnimation(float Alpha);
	void ApplyExitAnimation(float Alpha);
	void ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Delay, float AnimationTime, bool bReverse, float StartScale = 1.0f, float EndScale = 1.0f) const;

	bool bIsEntryAnimating = false;
	bool bIsExitAnimating = false;
	float EntryAnimationElapsed = 0.0f;
	float EntryAnimationDuration = 0.45f;
	float ExitAnimationElapsed = 0.0f;
	float ExitAnimationDuration = 0.35f;
};
