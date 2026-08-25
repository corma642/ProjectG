// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/PGGameSettings.h"
#include "PGReceiptShopWidget.generated.h"

class UBackgroundBlur;
class UButton;
class UPanelWidget;
class UPGReceiptShopPackItemWidget;
class UPGUserDataSubsystem;
class UTextBlock;
class UWidget;

/** 닫기 애니메이션이 끝나 상위 Home 흐름이 입력을 복구할 때 발생합니다. */
DECLARE_MULTICAST_DELEGATE(FPGReceiptShopClosedSignature);

/**
 * Home에서 여는 영수증 구매 화면입니다.
 * 상품 클릭은 설정된 영수증을 즉시 지급하며, 누적 재화 저장은 UPGUserDataSubsystem이 담당합니다.
 */
UCLASS()
class PROJECTG_API UPGReceiptShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 닫힌 모달을 Home 또는 Shop 흐름에 알립니다. */
	FPGReceiptShopClosedSignature OnReceiptShopClosed;

	/** 유저 지갑, 카드 클래스와 상품 설정을 연결하고 상품 목록을 생성합니다. */
	void InitializeReceiptShop(UPGUserDataSubsystem* InUserDataSubsystem, TSubclassOf<UPGReceiptShopPackItemWidget> InPackItemWidgetClass, const TArray<FPGReceiptShopPack>& InReceiptPacks);

	/** 배경 blur/dim과 상품 패널 진입 애니메이션을 시작합니다. */
	void PlayOpenAnimation();

	/** 입력을 잠근 뒤 역방향 애니메이션을 재생하고 완료 시 닫힘 delegate를 발생시킵니다. */
	void PlayCloseAnimation();

protected:
	/** BP 버튼을 연결하고 모달의 진입 전 초기 상태를 준비합니다. */
	virtual void NativeConstruct() override;

	/** 버튼, 카드와 유저 잔액 delegate를 제거 전에 해제합니다. */
	virtual void NativeDestruct() override;

	/** 열림 또는 닫힘 애니메이션 동안에만 모달 표현을 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 영수증 상점 모달 전체의 opacity 기준이 되는 필수 BP 루트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UWidget> RootPanel;

	/** 뒤 Home 또는 Shop 화면을 살짝 흐리게 하는 필수 블러 레이어입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UBackgroundBlur> BackgroundBlur;

	/** 뒤 화면을 어둡게 덮는 필수 딤 레이어입니다. 실제 색상과 브러시는 BP가 담당합니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UWidget> BackdropDim;

	/** 제목, 현재 보유량, 상품 리스트에 진입 scale을 적용하는 필수 상점 패널입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UWidget> ShopPanel;

	/** 모달 닫힘 애니메이션을 시작하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UButton> CloseButton;

	/** 지급 직후 갱신되는 누적 영수증 잔액을 표시하는 필수 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UTextBlock> ReceiptBalanceText;

	/** 상품 행을 자식으로 소유하는 필수 패널입니다. VerticalBox, ScrollBox 등 UPanelWidget 계열을 사용할 수 있습니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UPanelWidget> PackListBox;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleReceiptBalanceChanged(int32 NewBalance, FName Reason);

	void HandlePackSelected(int32 PackIndex);
	void RebuildPackList();
	void RefreshReceiptBalance() const;
	void SetReceiptShopButtonsEnabled(bool bEnabled) const;
	void ApplyEntryAnimation(float Alpha) const;

	// GameInstance가 소유하는 유저 데이터 경계를 관찰하며 모달 종료 시 delegate를 해제합니다.
	TWeakObjectPtr<UPGUserDataSubsystem> UserDataSubsystem;

	// 상품 목록을 재생성하는 동안 Widget Blueprint 클래스가 GC되지 않도록 유지합니다.
	UPROPERTY(Transient)
	TSubclassOf<UPGReceiptShopPackItemWidget> PackItemWidgetClass;

	TArray<FPGReceiptShopPack> ReceiptPacks;

	// PackListBox가 수명을 소유하는 상품 행을 순회·검색하기 위한 관찰 캐시입니다.
	TArray<TWeakObjectPtr<UPGReceiptShopPackItemWidget>> PackItemWidgets;
	TMap<int32, TWeakObjectPtr<UPGReceiptShopPackItemWidget>> PackItemWidgetsByPackIndex;

	bool bIsOpening = false;
	bool bIsClosing = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.38f;
};
