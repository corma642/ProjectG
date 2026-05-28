// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGMapShopItemWidget.generated.h"

class UButton;
class UImage;
class UPGMapDefinition;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FPGMapShopItemSelectedSignature, FName);

/**
 * 상점 맵 목록에 들어가는 개별 카드 로직 위젯입니다.
 * 카드 내부는 썸네일, 잠금, 현재 선택 마커만 표시하고 가격/액션은 상점 상세 영역이 담당합니다.
 */
UCLASS()
class PROJECTG_API UPGMapShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FPGMapShopItemSelectedSignature OnMapSelected;

	void SetMapDefinition(const UPGMapDefinition* InMapDefinition, bool bOwned, bool bEquipped, bool bSelected);
	void SetSelectionEnabled(bool bEnabled) const;
	FName GetMapId() const { return MapId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RootPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> LockImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> CurrentMarker;

private:
	UFUNCTION()
	void HandleSelectButtonClicked();

	void StartStateAnimation(bool bOwned, bool bEquipped, bool bSelected);
	void ApplyStateAnimation(float Alpha);
	void ApplyFinalState();

	FName MapId;
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
