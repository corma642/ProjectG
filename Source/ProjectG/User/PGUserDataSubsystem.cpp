// Copyright Epic Games, Inc. All Rights Reserved.

#include "User/PGUserDataSubsystem.h"

#include "Character/PGRunnerCharacter.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterShopCatalog.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Shop/PGMapDefinition.h"
#include "Shop/PGMapShopCatalog.h"
#include "UObject/UObjectGlobals.h"
#include "User/PGUserSaveCodec.h"
#include "User/PGUserSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGUserData, Log, All);

namespace
{
int32 AddReceiptBalanceClamped(int32 CurrentBalance, int32 Amount)
{
	const int64 NewBalance = static_cast<int64>(FMath::Max(0, CurrentBalance)) + static_cast<int64>(FMath::Max(0, Amount));
	return static_cast<int32>(FMath::Clamp<int64>(NewBalance, 0, MAX_int32));
}
}

const TCHAR* UPGUserDataSubsystem::UserSaveSlotAName = TEXT("PGUserData_A");
const TCHAR* UPGUserDataSubsystem::UserSaveSlotBName = TEXT("PGUserData_B");
const TCHAR* UPGUserDataSubsystem::LegacyUserSaveSlotName = TEXT("PGUserData");

void UPGUserDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UPGUserDataSubsystem::HandlePreLoadMap);
	LoadUserData();

	// 카탈로그 캐시는 RuntimePreloadSubsystem의 Home primary 단계에서 갱신합니다.
	// 저장 데이터 로드는 먼저 끝내 두어 장착 스킨 하나만 Home nested 단계에서 예열할 수 있게 합니다.
}

void UPGUserDataSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	CancelPendingSkinLoad();
	UserSaveGame = nullptr;
	CachedCatalog = nullptr;
	CachedMapCatalog = nullptr;
	CachedCharacterSkins.Reset();
	CachedMaps.Reset();
	Super::Deinitialize();
}

int32 UPGUserDataSubsystem::GetReceiptBalance() const
{
	return UserSaveGame ? FMath::Max(0, UserSaveGame->ReceiptBalance) : 0;
}

bool UPGUserDataSubsystem::TryAddReceipts(int32 Amount, FName Reason)
{
	if (Amount <= 0)
	{
		return false;
	}

	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	const int32 PreviousBalance = UserData->ReceiptBalance;
	UserData->ReceiptBalance = AddReceiptBalanceClamped(UserData->ReceiptBalance, Amount);
	if (SaveUserData())
	{
		OnReceiptBalanceChanged.Broadcast(UserData->ReceiptBalance, Reason);
		return true;
	}

	UserData->ReceiptBalance = PreviousBalance;
	return false;
}

int32 UPGUserDataSubsystem::GetBestRunScore() const
{
	return UserSaveGame ? FMath::Max(0, UserSaveGame->BestRunScore) : 0;
}

bool UPGUserDataSubsystem::TrySettleRunRewards(int32 Receipts, int32 Score, FName Reason)
{
	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	const int32 ReceiptDelta = FMath::Max(0, Receipts);
	const int32 SanitizedScore = FMath::Max(0, Score);
	const int32 PreviousBalance = UserData->ReceiptBalance;
	const int32 PreviousBestScore = UserData->BestRunScore;

	UserData->ReceiptBalance = AddReceiptBalanceClamped(UserData->ReceiptBalance, ReceiptDelta);
	UserData->BestRunScore = FMath::Max(UserData->BestRunScore, SanitizedScore);

	// 런 보상은 영수증과 최고 점수가 함께 확정되어야 한다. 저장 실패 시 메모리 상태도
	// 이전 값으로 되돌려 다음 이동 요청에서 같은 런 보상을 다시 저장할 수 있게 한다.
	if (!SaveUserData())
	{
		UserData->ReceiptBalance = PreviousBalance;
		UserData->BestRunScore = PreviousBestScore;
		return false;
	}

	if (UserData->ReceiptBalance != PreviousBalance)
	{
		OnReceiptBalanceChanged.Broadcast(UserData->ReceiptBalance, Reason);
	}
	if (UserData->BestRunScore != PreviousBestScore)
	{
		OnBestRunScoreChanged.Broadcast(UserData->BestRunScore, Reason);
	}
	return true;
}

const UPGCharacterShopCatalog* UPGUserDataSubsystem::GetCharacterCatalog() const
{
	return CachedCatalog;
}

const TArray<TObjectPtr<UPGCharacterSkinDefinition>>& UPGUserDataSubsystem::GetCharacterSkins() const
{
	return CachedCharacterSkins;
}

const UPGCharacterSkinDefinition* UPGUserDataSubsystem::FindCharacterSkin(FName SkinId) const
{
	if (SkinId.IsNone())
	{
		return nullptr;
	}

	for (const UPGCharacterSkinDefinition* SkinDefinition : CachedCharacterSkins)
	{
		if (SkinDefinition && SkinDefinition->SkinId == SkinId)
		{
			return SkinDefinition;
		}
	}
	return nullptr;
}

const UPGCharacterSkinDefinition* UPGUserDataSubsystem::GetDefaultCharacterSkin() const
{
	if (CachedCatalog && !CachedCatalog->DefaultSkinId.IsNone())
	{
		if (const UPGCharacterSkinDefinition* DefaultSkin = FindCharacterSkin(CachedCatalog->DefaultSkinId))
		{
			return DefaultSkin;
		}
	}

	for (const UPGCharacterSkinDefinition* SkinDefinition : CachedCharacterSkins)
	{
		if (SkinDefinition && SkinDefinition->bUnlockedByDefault)
		{
			return SkinDefinition;
		}
	}

	return CachedCharacterSkins.Num() > 0 ? CachedCharacterSkins[0] : nullptr;
}

const UPGCharacterSkinDefinition* UPGUserDataSubsystem::GetEquippedCharacterSkin() const
{
	if (UserSaveGame && !UserSaveGame->EquippedCharacterSkinId.IsNone())
	{
		if (const UPGCharacterSkinDefinition* EquippedSkin = FindCharacterSkin(UserSaveGame->EquippedCharacterSkinId))
		{
			return EquippedSkin;
		}
	}

	return GetDefaultCharacterSkin();
}

const UPGMapShopCatalog* UPGUserDataSubsystem::GetMapCatalog() const
{
	return CachedMapCatalog;
}

const TArray<TObjectPtr<UPGMapDefinition>>& UPGUserDataSubsystem::GetMaps() const
{
	return CachedMaps;
}

const UPGMapDefinition* UPGUserDataSubsystem::FindMap(FName MapId) const
{
	if (MapId.IsNone())
	{
		return nullptr;
	}

	for (const UPGMapDefinition* MapDefinition : CachedMaps)
	{
		if (MapDefinition && MapDefinition->MapId == MapId)
		{
			return MapDefinition;
		}
	}
	return nullptr;
}

const UPGMapDefinition* UPGUserDataSubsystem::GetDefaultMap() const
{
	if (CachedMapCatalog && !CachedMapCatalog->DefaultMapId.IsNone())
	{
		if (const UPGMapDefinition* DefaultMap = FindMap(CachedMapCatalog->DefaultMapId))
		{
			return DefaultMap;
		}
	}

	for (const UPGMapDefinition* MapDefinition : CachedMaps)
	{
		if (MapDefinition && MapDefinition->bUnlockedByDefault)
		{
			return MapDefinition;
		}
	}

	return CachedMaps.Num() > 0 ? CachedMaps[0] : nullptr;
}

const UPGMapDefinition* UPGUserDataSubsystem::GetEquippedMap() const
{
	if (UserSaveGame && !UserSaveGame->EquippedMapId.IsNone())
	{
		if (const UPGMapDefinition* EquippedMap = FindMap(UserSaveGame->EquippedMapId))
		{
			return EquippedMap;
		}
	}

	return GetDefaultMap();
}

void UPGUserDataSubsystem::RefreshShopCatalog()
{
	RefreshCatalogCache();
	EnsureDefaultOwnership();
}

void UPGUserDataSubsystem::SetPreviewRunner(APGRunnerCharacter* RunnerCharacter)
{
	PreviewRunnerCharacter = RunnerCharacter;
}

void UPGUserDataSubsystem::SetLastSelectedCharacterSkinId(FName SkinId)
{
	LastSelectedCharacterSkinId = SkinId;
}

void UPGUserDataSubsystem::SetLastSelectedMapId(FName MapId)
{
	LastSelectedMapId = MapId;
}

bool UPGUserDataSubsystem::IsCharacterOwned(FName SkinId) const
{
	if (SkinId.IsNone())
	{
		return false;
	}

	if (const UPGCharacterSkinDefinition* SkinDefinition = FindCharacterSkin(SkinId))
	{
		if (SkinDefinition->bUnlockedByDefault)
		{
			return true;
		}
	}

	return UserSaveGame && UserSaveGame->OwnedCharacterSkinIds.Contains(SkinId);
}

bool UPGUserDataSubsystem::IsCharacterEquipped(FName SkinId) const
{
	return UserSaveGame && !SkinId.IsNone() && UserSaveGame->EquippedCharacterSkinId == SkinId;
}

bool UPGUserDataSubsystem::IsMapOwned(FName MapId) const
{
	if (MapId.IsNone())
	{
		return false;
	}

	if (const UPGMapDefinition* MapDefinition = FindMap(MapId))
	{
		if (MapDefinition->bUnlockedByDefault)
		{
			return true;
		}
	}

	return UserSaveGame && UserSaveGame->OwnedMapIds.Contains(MapId);
}

bool UPGUserDataSubsystem::IsMapEquipped(FName MapId) const
{
	return UserSaveGame && !MapId.IsNone() && UserSaveGame->EquippedMapId == MapId;
}

bool UPGUserDataSubsystem::PurchaseCharacter(FName SkinId)
{
	const UPGCharacterSkinDefinition* SkinDefinition = FindCharacterSkin(SkinId);
	if (!SkinDefinition)
	{
		return false;
	}

	if (IsCharacterOwned(SkinId))
	{
		return true;
	}

	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	const int32 Price = FMath::Max(0, SkinDefinition->PriceReceipts);
	if (Price > 0 && GetReceiptBalance() < Price)
	{
		return false;
	}

	if (!CommitCatalogPurchase(*UserData, SkinId, Price, UserData->OwnedCharacterSkinIds, TEXT("CharacterPurchase")))
	{
		return false;
	}

	OnCharacterOwnershipChanged.Broadcast(SkinId);
	return true;
}

bool UPGUserDataSubsystem::EquipCharacter(FName SkinId)
{
	if (!IsCharacterOwned(SkinId) || !FindCharacterSkin(SkinId))
	{
		return false;
	}

	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	if (!CommitEquippedCatalogItem(SkinId, UserData->EquippedCharacterSkinId))
	{
		return false;
	}

	RequestApplySkinToRunner(FindCharacterSkin(SkinId), false);
	OnEquippedCharacterChanged.Broadcast(SkinId);
	return true;
}

bool UPGUserDataSubsystem::PurchaseMap(FName MapId)
{
	const UPGMapDefinition* MapDefinition = FindMap(MapId);
	if (!MapDefinition)
	{
		return false;
	}

	if (IsMapOwned(MapId))
	{
		return true;
	}

	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	const int32 Price = FMath::Max(0, MapDefinition->PriceReceipts);
	if (Price > 0 && GetReceiptBalance() < Price)
	{
		return false;
	}

	if (!CommitCatalogPurchase(*UserData, MapId, Price, UserData->OwnedMapIds, TEXT("MapPurchase")))
	{
		return false;
	}

	OnMapOwnershipChanged.Broadcast(MapId);
	return true;
}

bool UPGUserDataSubsystem::EquipMap(FName MapId)
{
	if (!IsMapOwned(MapId) || !FindMap(MapId))
	{
		return false;
	}

	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	if (!CommitEquippedCatalogItem(MapId, UserData->EquippedMapId))
	{
		return false;
	}

	OnEquippedMapChanged.Broadcast(MapId);
	return true;
}

bool UPGUserDataSubsystem::CommitCatalogPurchase(UPGUserSaveGame& UserData, FName ItemId, int32 PriceReceipts, TArray<FName>& OwnedItemIds, FName BalanceReason)
{
	const int32 Price = FMath::Max(0, PriceReceipts);
	if (Price > 0 && UserData.ReceiptBalance < Price)
	{
		return false;
	}

	const int32 PreviousBalance = UserData.ReceiptBalance;
	const TArray<FName> PreviousOwnedItemIds = OwnedItemIds;
	UserData.ReceiptBalance = FMath::Max(0, UserData.ReceiptBalance - Price);
	OwnedItemIds.AddUnique(ItemId);

	// 구매는 재화 차감과 보유 추가가 한 거래다. 같은 SaveGame을 한 번만 기록해야 두 상태가 어긋나지 않는다.
	if (!SaveUserData())
	{
		UserData.ReceiptBalance = PreviousBalance;
		OwnedItemIds = PreviousOwnedItemIds;
		return false;
	}

	if (Price > 0)
	{
		OnReceiptBalanceChanged.Broadcast(UserData.ReceiptBalance, BalanceReason);
	}
	return true;
}

bool UPGUserDataSubsystem::CommitEquippedCatalogItem(FName ItemId, FName& EquippedItemId)
{
	const FName PreviousEquippedItemId = EquippedItemId;
	EquippedItemId = ItemId;
	if (SaveUserData())
	{
		return true;
	}

	EquippedItemId = PreviousEquippedItemId;
	return false;
}

bool UPGUserDataSubsystem::PreviewCharacter(FName SkinId)
{
	const UPGCharacterSkinDefinition* SkinDefinition = FindCharacterSkin(SkinId);
	if (!SkinDefinition)
	{
		return false;
	}

	return RequestApplySkinToRunner(SkinDefinition, true);
}

void UPGUserDataSubsystem::RestoreEquippedCharacter()
{
	RequestApplySkinToRunner(GetEquippedCharacterSkin(), false);
}

void UPGUserDataSubsystem::LoadUserData()
{
	UPGUserSaveGame* SlotAData = nullptr;
	UPGUserSaveGame* SlotBData = nullptr;
	uint64 SlotAGeneration = 0;
	uint64 SlotBGeneration = 0;
	bool bSlotAExists = false;
	bool bSlotBExists = false;
	const bool bSlotAValid = TryLoadUserDataSlot(UserSaveSlotAName, SlotAData, SlotAGeneration, bSlotAExists);
	const bool bSlotBValid = TryLoadUserDataSlot(UserSaveSlotBName, SlotBData, SlotBGeneration, bSlotBExists);

	if (bSlotAValid && (!bSlotBValid || SlotAGeneration >= SlotBGeneration))
	{
		UserSaveGame = SlotAData;
		ActiveUserSaveSlotName = UserSaveSlotAName;
		CurrentUserSaveGeneration = SlotAGeneration;
	}
	else if (bSlotBValid)
	{
		UserSaveGame = SlotBData;
		ActiveUserSaveSlotName = UserSaveSlotBName;
		CurrentUserSaveGeneration = SlotBGeneration;
	}

	if (UserSaveGame && ((bSlotAExists && !bSlotAValid) || (bSlotBExists && !bSlotBValid)))
	{
		UE_LOG(LogPGUserData, Warning, TEXT("Recovered user data from the last valid snapshot after ignoring a damaged save slot."));
	}

	if (!UserSaveGame)
	{
		if (bSlotAExists || bSlotBExists)
		{
			UE_LOG(LogPGUserData, Warning, TEXT("All user data snapshots are damaged. Starting with new user data."));
		}
		UserSaveGame = Cast<UPGUserSaveGame>(UGameplayStatics::CreateSaveGameObject(UPGUserSaveGame::StaticClass()));
		SaveUserData();
	}

	if (UserSaveGame)
	{
		UserSaveGame->ReceiptBalance = FMath::Max(0, UserSaveGame->ReceiptBalance);
		UserSaveGame->BestRunScore = FMath::Max(0, UserSaveGame->BestRunScore);
	}
}

bool UPGUserDataSubsystem::SaveUserData()
{
	if (!UserSaveGame)
	{
		return false;
	}

	const TCHAR* TargetSlotName = GetNextUserSaveSlotName();
	const uint64 NextGeneration = CurrentUserSaveGeneration + 1;
	if (!WriteUserDataSlot(TargetSlotName, NextGeneration))
	{
		UE_LOG(LogPGUserData, Warning, TEXT("Failed to write or verify user data snapshot '%s'."), TargetSlotName);
		return false;
	}

	ActiveUserSaveSlotName = TargetSlotName;
	CurrentUserSaveGeneration = NextGeneration;

	if (UGameplayStatics::DoesSaveGameExist(LegacyUserSaveSlotName, UserSaveIndex))
	{
		UGameplayStatics::DeleteGameInSlot(LegacyUserSaveSlotName, UserSaveIndex);
	}
	return true;
}

bool UPGUserDataSubsystem::TryLoadUserDataSlot(const TCHAR* SlotName, UPGUserSaveGame*& OutUserData, uint64& OutGeneration, bool& bOutSlotExists) const
{
	OutUserData = nullptr;
	OutGeneration = 0;
	bOutSlotExists = UGameplayStatics::DoesSaveGameExist(SlotName, UserSaveIndex);
	if (!bOutSlotExists)
	{
		return false;
	}

	TArray<uint8> SaveData;
	TArray<uint8> Payload;
	if (!UGameplayStatics::LoadDataFromSlot(SaveData, SlotName, UserSaveIndex)
		|| !PGUserSaveCodec::Decode(SaveData, Payload, OutGeneration))
	{
		return false;
	}

	OutUserData = Cast<UPGUserSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload));
	return OutUserData != nullptr;
}

bool UPGUserDataSubsystem::WriteUserDataSlot(const TCHAR* SlotName, uint64 Generation) const
{
	TArray<uint8> Payload;
	TArray<uint8> SaveData;
	if (!UGameplayStatics::SaveGameToMemory(UserSaveGame, Payload)
		|| !PGUserSaveCodec::Encode(Payload, Generation, SaveData)
		|| !UGameplayStatics::SaveDataToSlot(SaveData, SlotName, UserSaveIndex))
	{
		return false;
	}

	// 저장 직후 다시 읽어 CRC와 SaveGame 역직렬화까지 확인한 뒤에만 최신 스냅샷으로 확정한다.
	UPGUserSaveGame* VerifiedUserData = nullptr;
	uint64 VerifiedGeneration = 0;
	bool bSlotExists = false;
	return TryLoadUserDataSlot(SlotName, VerifiedUserData, VerifiedGeneration, bSlotExists)
		&& VerifiedGeneration == Generation;
}

const TCHAR* UPGUserDataSubsystem::GetNextUserSaveSlotName() const
{
	return ActiveUserSaveSlotName == UserSaveSlotAName ? UserSaveSlotBName : UserSaveSlotAName;
}

UPGUserSaveGame* UPGUserDataSubsystem::EnsureUserData()
{
	if (!UserSaveGame)
	{
		LoadUserData();
	}
	return UserSaveGame;
}

void UPGUserDataSubsystem::RefreshCatalogCache()
{
	CachedCharacterSkins.Reset();
	CachedMaps.Reset();
	CachedCatalog = nullptr;
	CachedMapCatalog = nullptr;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	CachedCatalog = Settings->Shop.CharacterShopCatalog.Get();
	if (CachedCatalog)
	{
		TSet<FName> SeenSkinIds;
		for (UPGCharacterSkinDefinition* SkinDefinition : CachedCatalog->CharacterSkins)
		{
			if (!SkinDefinition)
			{
				continue;
			}

			if (SkinDefinition->SkinId.IsNone())
			{
				UE_LOG(LogPGUserData, Warning, TEXT("Character skin '%s' has empty SkinId."), *GetNameSafe(SkinDefinition));
				continue;
			}
			if (SeenSkinIds.Contains(SkinDefinition->SkinId))
			{
				UE_LOG(LogPGUserData, Warning, TEXT("Duplicate character SkinId '%s' in shop catalog."), *SkinDefinition->SkinId.ToString());
				continue;
			}
			if (SkinDefinition->SkeletalMesh.IsNull())
			{
				UE_LOG(LogPGUserData, Warning, TEXT("Character skin '%s' has no SkeletalMesh."), *SkinDefinition->SkinId.ToString());
			}

			SeenSkinIds.Add(SkinDefinition->SkinId);
			CachedCharacterSkins.Add(SkinDefinition);
		}

		if (!CachedCatalog->DefaultSkinId.IsNone() && !FindCharacterSkin(CachedCatalog->DefaultSkinId))
		{
			UE_LOG(LogPGUserData, Warning, TEXT("DefaultSkinId '%s' was not found in character shop catalog."), *CachedCatalog->DefaultSkinId.ToString());
		}
	}

	CachedMapCatalog = Settings->Shop.MapShopCatalog.Get();
	if (!CachedMapCatalog)
	{
		return;
	}

	TSet<FName> SeenMapIds;
	TSet<FString> SeenMapLevelPackages;
	for (UPGMapDefinition* MapDefinition : CachedMapCatalog->Maps)
	{
		if (!MapDefinition)
		{
			continue;
		}

		if (MapDefinition->MapId.IsNone())
		{
			UE_LOG(LogPGUserData, Warning, TEXT("Map definition '%s' has empty MapId."), *GetNameSafe(MapDefinition));
			continue;
		}
		if (SeenMapIds.Contains(MapDefinition->MapId))
		{
			UE_LOG(LogPGUserData, Warning, TEXT("Duplicate map MapId '%s' in map shop catalog."), *MapDefinition->MapId.ToString());
			continue;
		}
		if (MapDefinition->Level.IsNull())
		{
			UE_LOG(LogPGUserData, Warning, TEXT("Map '%s' has no Level asset."), *MapDefinition->MapId.ToString());
		}
		else
		{
			const FString LevelPackageName = UPGMapShopCatalog::GetNormalizedLevelPackageName(MapDefinition);
			if (SeenMapLevelPackages.Contains(LevelPackageName))
			{
				UE_LOG(LogPGUserData, Warning, TEXT("Map level '%s' is registered more than once in map shop catalog."), *LevelPackageName);
			}
			SeenMapLevelPackages.Add(LevelPackageName);
		}
		if (MapDefinition->CargoDropSet.IsNull())
		{
			UE_LOG(LogPGUserData, Warning, TEXT("Map '%s' has no CargoDropSet."), *MapDefinition->MapId.ToString());
		}

		SeenMapIds.Add(MapDefinition->MapId);
		CachedMaps.Add(MapDefinition);
	}

	if (!CachedMapCatalog->DefaultMapId.IsNone() && !FindMap(CachedMapCatalog->DefaultMapId))
	{
		UE_LOG(LogPGUserData, Warning, TEXT("DefaultMapId '%s' was not found in map shop catalog."), *CachedMapCatalog->DefaultMapId.ToString());
	}
}

void UPGUserDataSubsystem::EnsureDefaultOwnership()
{
	UPGUserSaveGame* UserData = EnsureUserData();
	const UPGCharacterSkinDefinition* DefaultSkin = GetDefaultCharacterSkin();
	if (!UserData)
	{
		return;
	}

	bool bChanged = false;
	if (DefaultSkin && !UserData->OwnedCharacterSkinIds.Contains(DefaultSkin->SkinId))
	{
		UserData->OwnedCharacterSkinIds.Add(DefaultSkin->SkinId);
		bChanged = true;
	}
	for (const UPGCharacterSkinDefinition* SkinDefinition : CachedCharacterSkins)
	{
		if (SkinDefinition && SkinDefinition->bUnlockedByDefault && !UserData->OwnedCharacterSkinIds.Contains(SkinDefinition->SkinId))
		{
			UserData->OwnedCharacterSkinIds.Add(SkinDefinition->SkinId);
			bChanged = true;
		}
	}
	if (DefaultSkin && (UserData->EquippedCharacterSkinId.IsNone() || !IsCharacterOwned(UserData->EquippedCharacterSkinId)))
	{
		UserData->EquippedCharacterSkinId = DefaultSkin->SkinId;
		bChanged = true;
	}

	const UPGMapDefinition* DefaultMap = GetDefaultMap();
	if (DefaultMap)
	{
		if (!UserData->OwnedMapIds.Contains(DefaultMap->MapId))
		{
			UserData->OwnedMapIds.Add(DefaultMap->MapId);
			bChanged = true;
		}
		for (const UPGMapDefinition* MapDefinition : CachedMaps)
		{
			if (MapDefinition && MapDefinition->bUnlockedByDefault && !UserData->OwnedMapIds.Contains(MapDefinition->MapId))
			{
				UserData->OwnedMapIds.Add(MapDefinition->MapId);
				bChanged = true;
			}
		}
		if (UserData->EquippedMapId.IsNone() || !IsMapOwned(UserData->EquippedMapId))
		{
			UserData->EquippedMapId = DefaultMap->MapId;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		SaveUserData();
	}
}

void UPGUserDataSubsystem::HandlePreLoadMap(const FString& MapName)
{
	// 맵 전환 중 이전 월드 러너와 프리뷰 로드가 새 월드 로드까지 유지되면 저사양 기기의 메모리 피크가 커진다.
	CancelPendingSkinLoad();
	PreviewRunnerCharacter.Reset();
}

void UPGUserDataSubsystem::CancelPendingSkinLoad()
{
	++SkinLoadRequestSerial;
	if (PendingSkinLoadHandle.IsValid())
	{
		PendingSkinLoadHandle->CancelHandle();
		PendingSkinLoadHandle.Reset();
	}
}

bool UPGUserDataSubsystem::RequestApplySkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition, bool bBroadcastPreview)
{
	if (!SkinDefinition || SkinDefinition->SkeletalMesh.IsNull() || !GetRunnerCharacter())
	{
		return false;
	}

	CancelPendingSkinLoad();
	const int32 RequestSerial = SkinLoadRequestSerial;

	TArray<FSoftObjectPath> ResourcePaths;
	ResourcePaths.AddUnique(SkinDefinition->SkeletalMesh.ToSoftObjectPath());
	if (!SkinDefinition->AnimClass.IsNull())
	{
		ResourcePaths.AddUnique(SkinDefinition->AnimClass.ToSoftObjectPath());
	}

	// Home 장착 스킨처럼 이미 준비된 리소스는 즉시 적용합니다.
	// Shop에서 처음 선택한 스킨은 비동기 로드가 끝날 때까지 기존 프리뷰 메시를 유지합니다.
	if (SkinDefinition->SkeletalMesh.Get() && (SkinDefinition->AnimClass.IsNull() || SkinDefinition->AnimClass.Get()))
	{
		HandleSkinResourcesLoaded(SkinDefinition->SkinId, RequestSerial, bBroadcastPreview);
		return true;
	}

	PendingSkinLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ResourcePaths,
		FStreamableDelegate::CreateUObject(this, &UPGUserDataSubsystem::HandleSkinResourcesLoaded, SkinDefinition->SkinId, RequestSerial, bBroadcastPreview),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("PGCharacterSkinPreview"));
	return PendingSkinLoadHandle.IsValid();
}

void UPGUserDataSubsystem::HandleSkinResourcesLoaded(FName SkinId, int32 RequestSerial, bool bBroadcastPreview)
{
	// 카드를 빠르게 넘긴 경우 마지막 요청만 반영합니다. 이전 로드 완료가 최신 프리뷰를 덮어쓰면 안 됩니다.
	if (RequestSerial != SkinLoadRequestSerial)
	{
		return;
	}

	// 메시 컴포넌트와 AnimInstance가 적용된 리소스를 직접 참조하므로 완료된 로드 핸들은 더 이상 보관하지 않습니다.
	if (PendingSkinLoadHandle.IsValid())
	{
		PendingSkinLoadHandle->ReleaseHandle();
		PendingSkinLoadHandle.Reset();
	}

	const UPGCharacterSkinDefinition* SkinDefinition = FindCharacterSkin(SkinId);
	if (!ApplyLoadedSkinToRunner(SkinDefinition))
	{
		UE_LOG(LogPGUserData, Warning, TEXT("Character skin '%s' could not be applied after async load."), *SkinId.ToString());
		return;
	}

	if (bBroadcastPreview)
	{
		OnPreviewCharacterChanged.Broadcast(SkinId);
	}
}

bool UPGUserDataSubsystem::ApplyLoadedSkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition) const
{
	if (!SkinDefinition || !SkinDefinition->SkeletalMesh.Get() || (!SkinDefinition->AnimClass.IsNull() && !SkinDefinition->AnimClass.Get()))
	{
		return false;
	}

	APGRunnerCharacter* RunnerCharacter = GetRunnerCharacter();
	return RunnerCharacter && RunnerCharacter->ApplyCharacterSkin(SkinDefinition);
}

APGRunnerCharacter* UPGUserDataSubsystem::GetRunnerCharacter() const
{
	if (PreviewRunnerCharacter.IsValid())
	{
		return PreviewRunnerCharacter.Get();
	}

	if (UWorld* World = GetWorld())
	{
		if (APGGameModeBase* GameMode = World->GetAuthGameMode<APGGameModeBase>())
		{
			return GameMode->GetPlayableRunnerCharacter();
		}
	}

	return nullptr;
}
