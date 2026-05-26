// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGUserDataSubsystem.generated.h"

class APGRunnerCharacter;
class UPGCharacterShopCatalog;
class UPGCharacterSkinDefinition;
class UPGUserSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGUserReceiptBalanceChangedSignature, int32, NewBalance, FName, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGUserBestRunScoreChangedSignature, int32, NewBestScore, FName, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGUserCharacterSkinChangedSignature, FName, SkinId);

/**
 * 영수증 재화와 캐릭터 스킨 보유/장착 상태를 통합 관리하는 GameInstance Subsystem입니다.
 * 인게임 러너 상태와 분리된 유저 진행 데이터는 이 Subsystem과 UPGUserSaveGame을 통해 저장합니다.
 */
UCLASS()
class PROJECTG_API UPGUserDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	int32 GetReceiptBalance() const;
	void AddReceipts(int32 Amount, FName Reason);
	bool TryAddReceipts(int32 Amount, FName Reason);
	bool CanSpendReceipts(int32 Amount) const;
	bool TrySpendReceipts(int32 Amount, FName Reason);
	int32 GetBestRunScore() const;
	bool TryUpdateBestRunScore(int32 Score, FName Reason);

	const UPGCharacterShopCatalog* GetCharacterCatalog() const;
	const TArray<TObjectPtr<UPGCharacterSkinDefinition>>& GetCharacterSkins() const;
	const UPGCharacterSkinDefinition* FindCharacterSkin(FName SkinId) const;
	const UPGCharacterSkinDefinition* GetDefaultCharacterSkin() const;
	const UPGCharacterSkinDefinition* GetEquippedCharacterSkin() const;
	void RefreshShopCatalog();

	void SetPreviewRunner(APGRunnerCharacter* RunnerCharacter);
	void SetLastSelectedCharacterSkinId(FName SkinId);
	FName GetLastSelectedCharacterSkinId() const { return LastSelectedCharacterSkinId; }

	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsCharacterOwned(FName SkinId) const;

	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsCharacterEquipped(FName SkinId) const;

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool PurchaseCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool EquipCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool PreviewCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	void RestoreEquippedCharacter();

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserReceiptBalanceChangedSignature OnReceiptBalanceChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserBestRunScoreChangedSignature OnBestRunScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnCharacterOwnershipChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnEquippedCharacterChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnPreviewCharacterChanged;

private:
	void LoadUserData();
	bool SaveUserData() const;
	UPGUserSaveGame* EnsureUserData();
	void RefreshCatalogCache();
	void EnsureDefaultOwnership();
	bool ApplySkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition) const;
	APGRunnerCharacter* GetRunnerCharacter() const;

	static const TCHAR* UserSaveSlotName;
	static constexpr int32 UserSaveIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPGUserSaveGame> UserSaveGame;

	UPROPERTY(Transient)
	TObjectPtr<UPGCharacterShopCatalog> CachedCatalog;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGCharacterSkinDefinition>> CachedCharacterSkins;

	TWeakObjectPtr<APGRunnerCharacter> PreviewRunnerCharacter;
	FName LastSelectedCharacterSkinId;
};
