// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGCharacterShopItemWidget.generated.h"

class UImage;
class UPGCharacterSkinDefinition;
class UButton;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FPGCharacterShopItemSelectedSignature, FName);

/**
 * 상점 캐릭터 목록의 개별 카드 로직 위젯입니다.
 * BP가 카드 레이아웃과 스타일을 담당하고, C++은 데이터 표시와 선택 이벤트만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGCharacterShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGCharacterShopItemSelectedSignature OnCharacterSelected;

	void SetCharacterSkin(const UPGCharacterSkinDefinition* InSkinDefinition, bool bOwned, bool bEquipped, bool bSelected);
	FName GetSkinId() const { return SkinId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UWidget> PricePanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<class UTextBlock> PriceText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UWidget> LockImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Shop Item")
	TObjectPtr<UWidget> CurrentMarker;

private:
	UFUNCTION()
	void HandleSelectButtonClicked();

	void StartStateAnimation(bool bOwned, bool bEquipped, bool bSelected);
	void ApplyStateAnimation(float Alpha);
	void ApplyFinalState();
	void BroadcastSelected();

	FName SkinId;
	bool bLastOwned = false;
	bool bLastEquipped = false;
	bool bLastSelected = false;
	bool bHasState = false;
	bool bStateAnimationActive = false;
	float StateAnimationElapsed = 0.0f;
	float StateAnimationDuration = 0.18f;
	float StartRootScale = 1.0f;
	float StartRootOpacity = 0.86f;
	float StartThumbnailScale = 1.0f;
	float StartLockOpacity = 0.0f;
	float StartMarkerOpacity = 0.0f;
	FVector2D StartMarkerTranslation = FVector2D::ZeroVector;
};
