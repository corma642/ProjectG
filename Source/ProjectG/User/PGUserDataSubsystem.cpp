// Copyright Epic Games, Inc. All Rights Reserved.

#include "User/PGUserDataSubsystem.h"

#include "Character/PGRunnerCharacter.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterShopCatalog.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Shop/PGMapDefinition.h"
#include "Shop/PGMapShopCatalog.h"
#include "User/PGUserSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGUserData, Log, All);

const TCHAR* UPGUserDataSubsystem::UserSaveSlotName = TEXT("PGUserData");

void UPGUserDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadUserData();
	RefreshCatalogCache();
	EnsureDefaultOwnership();
}

void UPGUserDataSubsystem::Deinitialize()
{
	SaveUserData();
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

void UPGUserDataSubsystem::AddReceipts(int32 Amount, FName Reason)
{
	TryAddReceipts(Amount, Reason);
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
	UserData->ReceiptBalance = FMath::Max(0, UserData->ReceiptBalance) + Amount;
	if (SaveUserData())
	{
		OnReceiptBalanceChanged.Broadcast(UserData->ReceiptBalance, Reason);
		return true;
	}

	UserData->ReceiptBalance = PreviousBalance;
	return false;
}

bool UPGUserDataSubsystem::CanSpendReceipts(int32 Amount) const
{
	return Amount > 0 && GetReceiptBalance() >= Amount;
}

bool UPGUserDataSubsystem::TrySpendReceipts(int32 Amount, FName Reason)
{
	if (!CanSpendReceipts(Amount))
	{
		return false;
	}

	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	const int32 PreviousBalance = UserData->ReceiptBalance;
	UserData->ReceiptBalance = FMath::Max(0, UserData->ReceiptBalance - Amount);
	if (!SaveUserData())
	{
		UserData->ReceiptBalance = PreviousBalance;
		return false;
	}

	OnReceiptBalanceChanged.Broadcast(UserData->ReceiptBalance, Reason);
	return true;
}

int32 UPGUserDataSubsystem::GetBestRunScore() const
{
	return UserSaveGame ? FMath::Max(0, UserSaveGame->BestRunScore) : 0;
}

bool UPGUserDataSubsystem::TryUpdateBestRunScore(int32 Score, FName Reason)
{
	const int32 SanitizedScore = FMath::Max(0, Score);
	if (SanitizedScore <= GetBestRunScore())
	{
		return false;
	}

	UPGUserSaveGame* UserData = EnsureUserData();
	if (!UserData)
	{
		return false;
	}

	const int32 PreviousBestScore = UserData->BestRunScore;
	UserData->BestRunScore = SanitizedScore;
	if (!SaveUserData())
	{
		UserData->BestRunScore = PreviousBestScore;
		return false;
	}

	OnBestRunScoreChanged.Broadcast(UserData->BestRunScore, Reason);
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

	const int32 PreviousBalance = UserData->ReceiptBalance;
	const TArray<FName> PreviousOwnedSkinIds = UserData->OwnedCharacterSkinIds;

	UserData->ReceiptBalance = FMath::Max(0, UserData->ReceiptBalance - Price);
	UserData->OwnedCharacterSkinIds.AddUnique(SkinId);

	// 구매는 재화 차감과 보유 추가가 한 거래다. 같은 SaveGame을 한 번만 저장해 두 상태가 어긋나지 않게 한다.
	if (!SaveUserData())
	{
		UserData->ReceiptBalance = PreviousBalance;
		UserData->OwnedCharacterSkinIds = PreviousOwnedSkinIds;
		return false;
	}

	if (Price > 0)
	{
		OnReceiptBalanceChanged.Broadcast(UserData->ReceiptBalance, TEXT("CharacterPurchase"));
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

	const FName PreviousEquippedSkinId = UserData->EquippedCharacterSkinId;
	UserData->EquippedCharacterSkinId = SkinId;
	if (!SaveUserData())
	{
		UserData->EquippedCharacterSkinId = PreviousEquippedSkinId;
		return false;
	}

	ApplySkinToRunner(FindCharacterSkin(SkinId));
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

	const int32 PreviousBalance = UserData->ReceiptBalance;
	const TArray<FName> PreviousOwnedMapIds = UserData->OwnedMapIds;

	UserData->ReceiptBalance = FMath::Max(0, UserData->ReceiptBalance - Price);
	UserData->OwnedMapIds.AddUnique(MapId);

	if (!SaveUserData())
	{
		UserData->ReceiptBalance = PreviousBalance;
		UserData->OwnedMapIds = PreviousOwnedMapIds;
		return false;
	}

	if (Price > 0)
	{
		OnReceiptBalanceChanged.Broadcast(UserData->ReceiptBalance, TEXT("MapPurchase"));
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

	const FName PreviousEquippedMapId = UserData->EquippedMapId;
	UserData->EquippedMapId = MapId;
	if (!SaveUserData())
	{
		UserData->EquippedMapId = PreviousEquippedMapId;
		return false;
	}

	OnEquippedMapChanged.Broadcast(MapId);
	return true;
}

bool UPGUserDataSubsystem::PreviewCharacter(FName SkinId)
{
	const UPGCharacterSkinDefinition* SkinDefinition = FindCharacterSkin(SkinId);
	if (!SkinDefinition)
	{
		return false;
	}

	const bool bApplied = ApplySkinToRunner(SkinDefinition);
	if (bApplied)
	{
		OnPreviewCharacterChanged.Broadcast(SkinId);
	}
	return bApplied;
}

void UPGUserDataSubsystem::RestoreEquippedCharacter()
{
	ApplySkinToRunner(GetEquippedCharacterSkin());
}

void UPGUserDataSubsystem::LoadUserData()
{
	if (USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(UserSaveSlotName, UserSaveIndex))
	{
		UserSaveGame = Cast<UPGUserSaveGame>(LoadedSave);
	}

	if (!UserSaveGame)
	{
		UserSaveGame = Cast<UPGUserSaveGame>(UGameplayStatics::CreateSaveGameObject(UPGUserSaveGame::StaticClass()));
		SaveUserData();
	}

	if (UserSaveGame)
	{
		UserSaveGame->ReceiptBalance = FMath::Max(0, UserSaveGame->ReceiptBalance);
		UserSaveGame->BestRunScore = FMath::Max(0, UserSaveGame->BestRunScore);
	}
}

bool UPGUserDataSubsystem::SaveUserData() const
{
	if (!UserSaveGame)
	{
		return false;
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(UserSaveGame, UserSaveSlotName, UserSaveIndex);
	if (!bSaved)
	{
		UE_LOG(LogPGUserData, Warning, TEXT("Failed to save user data to slot '%s'."), UserSaveSlotName);
	}
	return bSaved;
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
			if (!SkinDefinition->SkeletalMesh)
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

bool UPGUserDataSubsystem::ApplySkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition) const
{
	APGRunnerCharacter* RunnerCharacter = GetRunnerCharacter();
	return RunnerCharacter && SkinDefinition && RunnerCharacter->ApplyCharacterSkin(SkinDefinition);
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
