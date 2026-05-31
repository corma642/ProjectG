// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGUserDataSubsystem.generated.h"

class APGRunnerCharacter;
class UPGCharacterShopCatalog;
class UPGCharacterSkinDefinition;
class UPGMapDefinition;
class UPGMapShopCatalog;
class UPGUserSaveGame;
struct FStreamableHandle;

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
	// 현재 런의 영수증과 최고 점수를 하나의 SaveGame 트랜잭션으로 확정합니다.
	bool TrySettleRunRewards(int32 Receipts, int32 Score, FName Reason);

	const UPGCharacterShopCatalog* GetCharacterCatalog() const;
	const TArray<TObjectPtr<UPGCharacterSkinDefinition>>& GetCharacterSkins() const;
	const UPGCharacterSkinDefinition* FindCharacterSkin(FName SkinId) const;
	const UPGCharacterSkinDefinition* GetDefaultCharacterSkin() const;
	const UPGCharacterSkinDefinition* GetEquippedCharacterSkin() const;
	const UPGMapShopCatalog* GetMapCatalog() const;
	const TArray<TObjectPtr<UPGMapDefinition>>& GetMaps() const;
	const UPGMapDefinition* FindMap(FName MapId) const;
	const UPGMapDefinition* GetDefaultMap() const;
	const UPGMapDefinition* GetEquippedMap() const;
	void RefreshShopCatalog();

	void SetPreviewRunner(APGRunnerCharacter* RunnerCharacter);
	void SetLastSelectedCharacterSkinId(FName SkinId);
	FName GetLastSelectedCharacterSkinId() const { return LastSelectedCharacterSkinId; }
	void SetLastSelectedMapId(FName MapId);
	FName GetLastSelectedMapId() const { return LastSelectedMapId; }

	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsCharacterOwned(FName SkinId) const;

	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsCharacterEquipped(FName SkinId) const;

	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsMapOwned(FName MapId) const;

	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsMapEquipped(FName MapId) const;

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool PurchaseCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool EquipCharacter(FName SkinId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool PurchaseMap(FName MapId);

	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool EquipMap(FName MapId);

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
	FPGUserCharacterSkinChangedSignature OnMapOwnershipChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnEquippedMapChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnPreviewCharacterChanged;

private:
	void LoadUserData();
	bool SaveUserData();
	bool TryLoadUserDataSlot(const TCHAR* SlotName, UPGUserSaveGame*& OutUserData, uint64& OutGeneration, bool& bOutSlotExists) const;
	bool WriteUserDataSlot(const TCHAR* SlotName, uint64 Generation) const;
	const TCHAR* GetNextUserSaveSlotName() const;
	UPGUserSaveGame* EnsureUserData();
	void RefreshCatalogCache();
	void EnsureDefaultOwnership();
	void HandlePreLoadMap(const FString& MapName);
	void CancelPendingSkinLoad();
	bool RequestApplySkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition, bool bBroadcastPreview);
	void HandleSkinResourcesLoaded(FName SkinId, int32 RequestSerial, bool bBroadcastPreview);
	bool ApplyLoadedSkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition) const;
	APGRunnerCharacter* GetRunnerCharacter() const;

	static const TCHAR* UserSaveSlotAName;
	static const TCHAR* UserSaveSlotBName;
	static const TCHAR* LegacyUserSaveSlotName;
	static constexpr int32 UserSaveIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPGUserSaveGame> UserSaveGame;

	UPROPERTY(Transient)
	TObjectPtr<UPGCharacterShopCatalog> CachedCatalog;

	UPROPERTY(Transient)
	TObjectPtr<UPGMapShopCatalog> CachedMapCatalog;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGCharacterSkinDefinition>> CachedCharacterSkins;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGMapDefinition>> CachedMaps;

	TWeakObjectPtr<APGRunnerCharacter> PreviewRunnerCharacter;
	TSharedPtr<FStreamableHandle> PendingSkinLoadHandle;
	FString ActiveUserSaveSlotName;
	uint64 CurrentUserSaveGeneration = 0;
	int32 SkinLoadRequestSerial = 0;
	FName LastSelectedCharacterSkinId;
	FName LastSelectedMapId;
};
