// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/PGGameSettings.h"
#include "PGReceiptShopPackItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FPGReceiptShopPackSelectedSignature, int32 /*PackIndex*/);

/**
 * 영수증 상점의 단일 상품 행 위젯입니다.
 * BP는 상품 배경/아이콘/가격 버튼 디자인을 담당하고, C++은 상품 데이터 표시와 구매 피드백만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGReceiptShopPackItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGReceiptShopPackSelectedSignature OnPackSelected;

	void InitializePack(const FPGReceiptShopPack& InPack, int32 InPackIndex);
	void PlayPurchaseFeedback(bool bSuccess);
	void SetPackButtonEnabled(bool bEnabled) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 상품 행 전체 루트입니다. 구매 성공/실패 피드백은 이 루트에 적용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UWidget> RootPanel;

	// 상품을 구매하는 버튼입니다. 가격 버튼 영역을 그대로 덮도록 BP에서 배치합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UButton> PurchaseButton;

	// 상품 이름 텍스트입니다. 예: SMALL PACK
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UTextBlock> PackNameText;

	// 지급 영수증 수량 텍스트입니다. C++은 "x50" 형식으로 표시합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UTextBlock> ReceiptAmountText;

	// 테스트 구매 가격 텍스트입니다. 실제 IAP 연동 전까지 표시용 문자열만 사용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Receipt Shop")
	TObjectPtr<UTextBlock> PriceText;

	// 상품별 이미지가 필요한 경우에만 BP에서 이름을 맞춥니다. 없으면 BP 기본 이미지를 유지합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional), Category = "Receipt Shop")
	TObjectPtr<UImage> ThumbnailImage;

private:
	UFUNCTION()
	void HandlePurchaseClicked();

	void ApplyPurchaseFeedback(float DeltaSeconds);

	FPGReceiptShopPack PackData;
	int32 PackIndex = INDEX_NONE;

	bool bFeedbackActive = false;
	float FeedbackElapsed = 0.0f;
	float FeedbackDuration = 0.22f;
	bool bFeedbackSuccess = true;
};
