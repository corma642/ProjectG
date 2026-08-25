// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGCharacterShopItemWidget.generated.h"

class UImage;
class UPGCharacterSkinDefinition;
class UButton;
class UWidget;

/** 캐릭터 카드 선택 시 해당 상품의 SkinId를 상점 화면에 전달합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPGCharacterShopItemSelectedSignature, FName);

/**
 * 상점 캐릭터 목록에 들어가는 개별 카드 로직 위젯입니다.
 * 카드 안에는 썸네일, 잠금 표시, CURRENT 표시만 두고 가격/구매 상태는 상점 상세 영역에서 표시합니다.
 */
UCLASS()
class PROJECTG_API UPGCharacterShopItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 카드가 눌렸을 때 현재 SkinId를 상점 화면에 전달합니다. */
	FPGCharacterShopItemSelectedSignature OnCharacterSelected;

	/** 캐릭터 상품과 보유·장착·선택 상태를 카드 표현에 주입합니다. */
	void SetCharacterSkin(const UPGCharacterSkinDefinition* InSkinDefinition, bool bOwned, bool bEquipped, bool bSelected);

	/** 상위 상점의 전환·모달 상태에 맞춰 카드 선택 입력을 잠급니다. */
	void SetSelectionEnabled(bool bEnabled) const;

	/** 현재 카드가 나타내는 상품의 SkinId를 반환합니다. */
	FName GetSkinId() const { return SkinId; }

protected:
	/** BP 선택 버튼 delegate를 연결합니다. */
	virtual void NativeConstruct() override;

	/** 재사용 또는 제거 전에 선택 버튼 delegate를 해제합니다. */
	virtual void NativeDestruct() override;

	/** 보유·장착·선택 상태가 바뀐 직후의 짧은 카드 애니메이션만 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 카드 전체 루트입니다. 선택 상태에 따라 스케일과 투명도 애니메이션을 적용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RootPanel;

	// 카드 전체를 덮는 투명 선택 버튼입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> SelectButton;

	// 캐릭터 스킨 썸네일 이미지입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UImage> ThumbnailImage;

	// 미보유 캐릭터에 표시되는 잠금 오버레이입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> LockImage;

	// 현재 장착 중인 캐릭터에 표시되는 CURRENT 마커입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
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
