// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGHomeScreenWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;

/** START 버튼 입력을 Home 흐름 관리자에 전달합니다. */
DECLARE_MULTICAST_DELEGATE(FPGHomeStartDeliveryRequestedSignature);
/** Settings 버튼 입력을 Home 흐름 관리자에 전달합니다. */
DECLARE_MULTICAST_DELEGATE(FPGHomeSettingsRequestedSignature);
/** Help 버튼 입력을 Home 흐름 관리자에 전달합니다. */
DECLARE_MULTICAST_DELEGATE(FPGHomeHelpRequestedSignature);
/** Shop 버튼 입력을 Home 흐름 관리자에 전달합니다. */
DECLARE_MULTICAST_DELEGATE(FPGHomeShopRequestedSignature);
/** 영수증 추가 버튼 입력을 Home 흐름 관리자에 전달합니다. */
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
	/** START 버튼 요청입니다. Home 흐름만 화면 전환과 카운트다운을 소유합니다. */
	FPGHomeStartDeliveryRequestedSignature OnStartDeliveryRequested;
	/** Settings 모달 생성 요청입니다. */
	FPGHomeSettingsRequestedSignature OnSettingsRequested;
	/** 튜토리얼 모달 생성 요청입니다. */
	FPGHomeHelpRequestedSignature OnHelpRequested;
	/** 캐릭터/맵 상점 화면 전환 요청입니다. */
	FPGHomeShopRequestedSignature OnShopRequested;
	/** 영수증 상점 모달 생성 요청입니다. */
	FPGHomeReceiptShopRequestedSignature OnReceiptShopRequested;

	/** START DELIVERY 이후 Home 요소들이 화면 밖으로 빠지는 퇴장 애니메이션을 시작합니다. */
	void PlayExitAnimation(float Duration);

	/** Settings/Help/Shop 화면이 열려 있거나 리소스 예열 대기 중일 때 Home 버튼 입력을 잠급니다. */
	void SetHomeButtonsEnabled(bool bEnabled) const;

	/** Home 화면에 표시할 0 이상의 누적 보유 영수증 값을 갱신합니다. */
	void SetReceiptBalance(int32 Balance) const;

	/** Home 화면에 표시하는 0 이상의 최고 런 점수를 갱신합니다. */
	void SetBestRunScore(int32 Score) const;

protected:
	/** BP 버튼 delegate를 연결하고 Home 진입 애니메이션을 시작합니다. */
	virtual void NativeConstruct() override;

	/** 위젯 제거 전에 모든 BP 버튼 delegate를 해제합니다. */
	virtual void NativeDestruct() override;

	/** 진입·퇴장 또는 영수증 상점 비활성 피드백이 재생되는 동안만 상태를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Home 진입·퇴장 opacity를 적용하는 필수 BP 루트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> RootPanel;

	/** 진입 시 위에서 내려오는 상단 게임 로고 이미지입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UImage> LogoImage;

	/** START/SHOP/SETTINGS/HELP 버튼을 묶어 이동 애니메이션을 적용하는 필수 패널입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> MenuPanel;

	/** 런 리소스 예열과 카운트다운 진입을 요청하는 필수 START 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> StartDeliveryButton;

	/** 캐릭터/맵 상점 전환을 요청하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> ShopButton;

	/** 공용 Settings 모달을 여는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> SettingsButton;

	/** 책자형 튜토리얼을 여는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UButton> HelpButton;

	/** 누적 영수증 아이콘/배경/텍스트에 진입 애니메이션을 적용하는 필수 패널입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UWidget> ReceiptPanel;

	/** 저장된 누적 영수증 잔액을 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	/** 저장된 최고 런 점수를 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Home")
	TObjectPtr<UTextBlock> BestScoreText;

	/** 테스트용 영수증 상품 모달을 여는 필수 추가 버튼입니다. */
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
	void ApplyReceiptShopUnavailableFeedback(float DeltaSeconds);
	void ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& StartOffset, float Delay, float AnimationTime, bool bReverse, float StartScale = 1.0f, float EndScale = 1.0f) const;

	bool bIsEntryAnimating = false;
	bool bIsExitAnimating = false;
	float EntryAnimationElapsed = 0.0f;
	float EntryAnimationDuration = 0.45f;
	float ExitAnimationElapsed = 0.0f;
	float ExitAnimationDuration = 0.35f;
	bool bReceiptShopUnavailableFeedbackActive = false;
	float ReceiptShopUnavailableFeedbackElapsed = 0.0f;
	float ReceiptShopUnavailableFeedbackDuration = 0.22f;
};
