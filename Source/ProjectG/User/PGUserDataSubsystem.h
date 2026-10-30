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

/** 영수증 잔액이 저장까지 확정된 뒤 새 잔액과 변경 사유를 전달하는 delegate입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGUserReceiptBalanceChangedSignature, int32, NewBalance, FName, Reason);

/** 최고 점수가 저장까지 확정된 뒤 새 점수와 변경 사유를 전달하는 delegate입니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGUserBestRunScoreChangedSignature, int32, NewBestScore, FName, Reason);

/** 캐릭터 또는 맵 상품의 보유·장착·preview ID 변경을 전달하는 공용 delegate 형식입니다. */
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
	/** A/B 저장 스냅샷을 복구하고 맵 전환 delegate를 연결합니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 비동기 스킨 요청과 캐시·delegate를 GameInstance 종료 전에 해제합니다. */
	virtual void Deinitialize() override;

	/** 저장된 영수증 잔액을 0 이상으로 보정해 반환합니다. */
	int32 GetReceiptBalance() const;

	/** 양수 영수증을 추가해 저장까지 성공하면 true를 반환하고 잔액 delegate를 발생시킵니다. */
	bool TryAddReceipts(int32 Amount, FName Reason);

	/** 양수 영수증을 차감해 저장까지 성공하면 true를 반환하고 잔액 delegate를 발생시킵니다. 잔액이 모자라면 아무것도 바꾸지 않고 false입니다. */
	bool TrySpendReceipts(int32 Amount, FName Reason);

	/** Home과 결과 화면에서 사용하는 저장된 최고 런 점수를 반환합니다. */
	int32 GetBestRunScore() const;

	/** 현재 런의 영수증과(bUpdateBestScore면) 최고 점수를 하나의 SaveGame 트랜잭션으로 확정합니다. 튜토리얼 런은 최고 점수를 갱신하지 않습니다. */
	bool TrySettleRunRewards(int32 Receipts, int32 Score, FName Reason, bool bUpdateBestScore = true);

	/** 튜토리얼 스테이지를 끝내 보상까지 받은 저장 기록이 있는지 반환합니다. 저장 데이터가 없으면 false입니다. */
	bool IsTutorialCompleted() const;

	/** 튜토리얼 첫 완료를 저장합니다: 완료 플래그와 보상 영수증을 한 번의 저장으로 확정하고 잔액 delegate를 발생시킵니다. 이미 완료했거나 저장이 실패하면 아무것도 바꾸지 않고 false입니다. */
	bool TryGrantTutorialCompletion(int32 RewardReceipts, FName Reason);

	/** 예열 후 캐시된 캐릭터 상점 카탈로그를 반환합니다. 준비되지 않았으면 nullptr입니다. */
	const UPGCharacterShopCatalog* GetCharacterCatalog() const;

	/** 현재 카탈로그의 유효 캐릭터 상품을 에디터 진열 순서대로 반환합니다. */
	const TArray<TObjectPtr<UPGCharacterSkinDefinition>>& GetCharacterSkins() const;

	/** 고유 SkinId에 대응하는 캐릭터 상품을 찾고 없으면 nullptr을 반환합니다. */
	const UPGCharacterSkinDefinition* FindCharacterSkin(FName SkinId) const;

	/** 카탈로그가 지정한 기본 캐릭터 상품을 반환합니다. */
	const UPGCharacterSkinDefinition* GetDefaultCharacterSkin() const;

	/** 저장된 장착 ID를 해석하고 유효하지 않으면 기본 캐릭터로 보정해 반환합니다. */
	const UPGCharacterSkinDefinition* GetEquippedCharacterSkin() const;

	/** 예열 후 캐시된 맵 상점 카탈로그를 반환합니다. 준비되지 않았으면 nullptr입니다. */
	const UPGMapShopCatalog* GetMapCatalog() const;

	/** 현재 카탈로그의 유효 맵 상품을 에디터 진열 순서대로 반환합니다. */
	const TArray<TObjectPtr<UPGMapDefinition>>& GetMaps() const;

	/** 고유 MapId에 대응하는 맵 상품을 찾고 없으면 nullptr을 반환합니다. */
	const UPGMapDefinition* FindMap(FName MapId) const;

	/** 카탈로그가 지정한 기본 맵 상품을 반환합니다. */
	const UPGMapDefinition* GetDefaultMap() const;

	/** 저장된 장착 ID를 해석하고 유효하지 않으면 기본 맵으로 보정해 반환합니다. */
	const UPGMapDefinition* GetEquippedMap() const;

	/** preload 완료 후 카탈로그 포인터와 기본 보유 상태를 다시 동기화합니다. */
	void RefreshShopCatalog();

	/** Home/Shop에서 스킨을 미리 적용할 러너를 약한 참조로 지정합니다. */
	void SetPreviewRunner(APGRunnerCharacter* RunnerCharacter);

	/** 상점 재진입 시 복구할 마지막 캐릭터 선택 ID를 메모리에 기록합니다. */
	void SetLastSelectedCharacterSkinId(FName SkinId);

	/** 이번 앱 실행 중 마지막으로 선택한 캐릭터 상품 ID를 반환합니다. */
	FName GetLastSelectedCharacterSkinId() const { return LastSelectedCharacterSkinId; }

	/** 상점 재진입 시 복구할 마지막 맵 선택 ID를 메모리에 기록합니다. */
	void SetLastSelectedMapId(FName MapId);

	/** 이번 앱 실행 중 마지막으로 선택한 맵 상품 ID를 반환합니다. */
	FName GetLastSelectedMapId() const { return LastSelectedMapId; }

	/** 기본 해금 또는 저장된 보유 목록에 SkinId가 포함됐는지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsCharacterOwned(FName SkinId) const;

	/** SkinId가 현재 저장된 장착 캐릭터인지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsCharacterEquipped(FName SkinId) const;

	/** 기본 해금 또는 저장된 보유 목록에 MapId가 포함됐는지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsMapOwned(FName MapId) const;

	/** MapId가 현재 저장된 장착 맵인지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | User")
	bool IsMapEquipped(FName MapId) const;

	/** 잔액을 차감해 캐릭터를 구매하며 저장까지 완료됐을 때만 true를 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool PurchaseCharacter(FName SkinId);

	/** 보유 캐릭터를 장착해 저장까지 성공하면 true를 반환하고, 프리뷰 러너에 비동기 적용을 요청합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool EquipCharacter(FName SkinId);

	/** 잔액을 차감해 맵을 구매하며 저장까지 완료됐을 때만 true를 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool PurchaseMap(FName MapId);

	/** 보유 맵을 장착해 저장까지 성공하면 true를 반환합니다. 레벨 이동은 호출자가 담당합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool EquipMap(FName MapId);

	/** 저장값을 바꾸지 않고 SkinId 리소스를 현재 preview 러너에 비동기로 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	bool PreviewCharacter(FName SkinId);

	/** 저장된 장착 캐릭터를 현재 preview 러너에 다시 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | User")
	void RestoreEquippedCharacter();

	/** 저장된 영수증 잔액 변경이 확정된 뒤 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserReceiptBalanceChangedSignature OnReceiptBalanceChanged;

	/** 최고 런 점수 변경이 저장된 뒤 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserBestRunScoreChangedSignature OnBestRunScoreChanged;

	/** 캐릭터 구매가 저장된 뒤 구매한 SkinId와 함께 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnCharacterOwnershipChanged;

	/** 캐릭터 장착이 저장된 뒤 장착 SkinId와 함께 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnEquippedCharacterChanged;

	/** 맵 구매가 저장된 뒤 구매한 MapId와 함께 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnMapOwnershipChanged;

	/** 맵 장착이 저장된 뒤 장착 MapId와 함께 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnEquippedMapChanged;

	/** 가장 최근 preview 요청의 스킨 적용이 끝난 뒤 SkinId와 함께 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | User")
	FPGUserCharacterSkinChangedSignature OnPreviewCharacterChanged;

private:
	/** 잔액 차감과 보유 ID 추가를 한 번 저장하고 실패하면 두 값을 함께 rollback합니다. */
	bool CommitCatalogPurchase(UPGUserSaveGame& UserData, FName ItemId, int32 PriceReceipts, TArray<FName>& OwnedItemIds, FName BalanceReason);

	/** 장착 ID를 저장하고 저장 실패 시 이전 ID로 되돌립니다. */
	bool CommitEquippedCatalogItem(FName ItemId, FName& EquippedItemId);

	/** 두 A/B 스냅샷을 검증하고 generation이 가장 높은 정상 저장을 활성 상태로 복구합니다. */
	void LoadUserData();

	/** 비활성 A/B 슬롯에 다음 generation을 기록·재검증한 뒤에만 활성 슬롯을 교체합니다. */
	bool SaveUserData();

	/** 슬롯 envelope와 SaveGame payload를 검증해 유효한 사용자 데이터만 반환합니다. */
	bool TryLoadUserDataSlot(const TCHAR* SlotName, UPGUserSaveGame*& OutUserData, uint64& OutGeneration, bool& bOutSlotExists) const;

	/** 지정 슬롯에 저장한 직후 CRC와 역직렬화를 다시 확인합니다. */
	bool WriteUserDataSlot(const TCHAR* SlotName, uint64 Generation) const;
	const TCHAR* GetNextUserSaveSlotName() const;
	UPGUserSaveGame* EnsureUserData();
	void RefreshCatalogCache();
	void EnsureDefaultOwnership();
	/** 맵 전환 전에 preview 러너와 아직 끝나지 않은 스킨 load handle을 해제합니다. */
	void HandlePreLoadMap(const FString& MapName);

	/** 진행 중인 async 스킨 요청을 취소하고 handle 참조를 즉시 반납합니다. */
	void CancelPendingSkinLoad();

	/** 스킨의 mesh/AnimClass를 비동기 예열하고 request serial로 마지막 요청만 유효하게 만듭니다. */
	bool RequestApplySkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition, bool bBroadcastPreview);

	/** 완료 순서가 뒤집혀도 최신 serial과 일치하는 스킨만 러너에 적용합니다. */
	void HandleSkinResourcesLoaded(FName SkinId, int32 RequestSerial, bool bBroadcastPreview);
	bool ApplyLoadedSkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition) const;
	APGRunnerCharacter* GetRunnerCharacter() const;

	static const TCHAR* UserSaveSlotAName;
	static const TCHAR* UserSaveSlotBName;
	static const TCHAR* LegacyUserSaveSlotName;
	static constexpr int32 UserSaveIndex = 0;

	// 현재 활성 A/B 스냅샷에서 복원해 모든 영구 상태 변경의 단일 원본으로 사용합니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGUserSaveGame> UserSaveGame;

	// RuntimePreload 완료 뒤 강한 참조로 유지하는 캐릭터 상점 카탈로그입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGCharacterShopCatalog> CachedCatalog;

	// RuntimePreload 완료 뒤 강한 참조로 유지하는 맵 상점 카탈로그입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGMapShopCatalog> CachedMapCatalog;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGCharacterSkinDefinition>> CachedCharacterSkins;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGMapDefinition>> CachedMaps;

	// 레벨/GameMode가 소유하며 맵 전환 시 사라질 수 있으므로 약한 참조만 유지합니다.
	TWeakObjectPtr<APGRunnerCharacter> PreviewRunnerCharacter;

	// 완료 delegate까지 async 요청을 유지하며 새 요청 또는 맵 전환 시 명시적으로 해제합니다.
	TSharedPtr<FStreamableHandle> PendingSkinLoadHandle;

	// 마지막으로 검증된 A/B 슬롯과 generation입니다. 다음 저장은 반대 슬롯에 기록합니다.
	FString ActiveUserSaveSlotName;
	uint64 CurrentUserSaveGeneration = 0;

	// 비동기 완료 순서와 무관하게 마지막 preview/equip 요청만 적용하기 위한 단조 증가 serial입니다.
	int32 SkinLoadRequestSerial = 0;
	FName LastSelectedCharacterSkinId;
	FName LastSelectedMapId;
};
