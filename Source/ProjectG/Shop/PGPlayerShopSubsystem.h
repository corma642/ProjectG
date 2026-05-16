// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGPlayerShopSubsystem.generated.h"

class APGRunnerCharacter;
class UPGCharacterShopCatalog;
class UPGCharacterSkinDefinition;
class UPGPlayerShopSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGCharacterSkinChangedSignature, FName, SkinId);

/**
 * 캐릭터 스킨의 구매/보유/장착 상태를 관리하는 상점 서브시스템입니다.
 * 지갑 서브시스템과 연동해 영수증을 차감하고, 러너에는 메시/애님만 적용합니다.
 */
UCLASS()
class PROJECTG_API UPGPlayerShopSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const UPGCharacterShopCatalog* GetCharacterCatalog() const;
	const TArray<TObjectPtr<UPGCharacterSkinDefinition>>& GetCharacterSkins() const;
	const UPGCharacterSkinDefinition* FindCharacterSkin(FName SkinId) const;
	const UPGCharacterSkinDefinition* GetDefaultCharacterSkin() const;
	const UPGCharacterSkinDefinition* GetEquippedCharacterSkin() const;

	void RefreshShopCatalog();

	// Home/Shop 프리뷰에서 메시를 교체할 월드 러너를 지정합니다.
	void SetPreviewRunner(APGRunnerCharacter* RunnerCharacter);
	void SetLastSelectedCharacterSkinId(FName SkinId);
	FName GetLastSelectedCharacterSkinId() const { return LastSelectedCharacterSkinId; }

	UFUNCTION(BlueprintPure, Category = "ProjectG | Shop")
	bool IsCharacterOwned(FName SkinId) const;

	UFUNCTION(BlueprintPure, Category = "ProjectG | Shop")
	bool IsCharacterEquipped(FName SkinId) const;

	UFUNCTION(BlueprintCallable, Category = "ProjectG | Shop")
	bool PurchaseCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | Shop")
	bool EquipCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | Shop")
	bool PreviewCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | Shop")
	void RestoreEquippedCharacter();

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Shop")
	FPGCharacterSkinChangedSignature OnCharacterOwnershipChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Shop")
	FPGCharacterSkinChangedSignature OnEquippedCharacterChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Shop")
	FPGCharacterSkinChangedSignature OnPreviewCharacterChanged;

private:
	void LoadShop();
	bool SaveShop() const;
	UPGPlayerShopSaveGame* EnsureShopSave();
	void RefreshCatalogCache();
	void EnsureDefaultOwnership();
	bool ApplySkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition) const;
	APGRunnerCharacter* GetRunnerCharacter() const;

	static const TCHAR* ShopSaveSlotName;
	static constexpr int32 ShopSaveUserIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPGPlayerShopSaveGame> ShopSaveGame;

	UPROPERTY(Transient)
	TObjectPtr<UPGCharacterShopCatalog> CachedCatalog;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGCharacterSkinDefinition>> CachedCharacterSkins;

	TWeakObjectPtr<APGRunnerCharacter> PreviewRunnerCharacter;
	FName LastSelectedCharacterSkinId;
};
