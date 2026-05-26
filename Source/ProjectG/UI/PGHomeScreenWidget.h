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
DECLARE_MULTICAST_DELEGATE(FPGHomeReceiptShopRequestedSignature);

/**
 * 앱 시작 직후 표시되는 Home 화면의 로직 위젯입니다.
 * BP가 로고/버튼/영수증 표시 레이아웃을 담당하고, C++은 버튼 이벤트와 진입/퇴장 애니메이션만 처리합니다.
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
	FPGHomeReceiptShopRequestedSignature OnReceiptShopRequested;

	// START DELIVERY 이후 Home 요소들이 화면 밖으로 빠지는 퇴장 애니메이션을 시작합니다.
	void PlayExitAnimation(float Duration);

	// Settings/Help/Shop 화면이 열려 있거나 리소스 예열 대기 중일 때 Home 버튼 입력을 잠급니다.
	void SetHomeButtonsEnabled(bool bEnabled) const;

	// Home 화면에 표시할 누적 보유 영수증 값을 갱신합니다.
	void SetReceiptBalance(int32 Balance) const;

	// Home 화면에 표시하는 지금까지의 최고 런 점수를 갱신합니다.
	void SetBestRunScore(int32 Score) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Home 위젯 전체 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> RootPanel;

	// 상단 게임 로고 이미지입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UImage> LogoImage;

	// START/SHOP/SETTINGS/HELP 버튼을 묶는 메뉴 패널입니다.
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

	// 누적 영수증 아이콘/배경/텍스트를 포함하는 표시 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> ReceiptPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UTextBlock> BestScoreText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> ReceiptAddButton;

private:
	UFUNCTION()
	void HandleStartDeliveryClicked();

	UFUNCTION()
	void HandleShopClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleHelpClicked();

	UFUNCTION()
	void HandleReceiptAddClicked();

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
