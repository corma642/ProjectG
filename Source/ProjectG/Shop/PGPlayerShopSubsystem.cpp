// Copyright Epic Games, Inc. All Rights Reserved.

#include "Shop/PGPlayerShopSubsystem.h"

#include "Character/PGRunnerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterShopCatalog.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Shop/PGPlayerShopSaveGame.h"
#include "Wallet/PGPlayerWalletSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGPlayerShop, Log, All);

const TCHAR* UPGPlayerShopSubsystem::ShopSaveSlotName = TEXT("PGPlayerShop");

void UPGPlayerShopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadShop();
	RefreshCatalogCache();
	EnsureDefaultOwnership();
}

void UPGPlayerShopSubsystem::Deinitialize()
{
	SaveShop();
	ShopSaveGame = nullptr;
	CachedCatalog = nullptr;
	CachedCharacterSkins.Reset();
	Super::Deinitialize();
}

const UPGCharacterShopCatalog* UPGPlayerShopSubsystem::GetCharacterCatalog() const
{
	return CachedCatalog;
}

const TArray<TObjectPtr<UPGCharacterSkinDefinition>>& UPGPlayerShopSubsystem::GetCharacterSkins() const
{
	return CachedCharacterSkins;
}

const UPGCharacterSkinDefinition* UPGPlayerShopSubsystem::FindCharacterSkin(FName SkinId) const
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

const UPGCharacterSkinDefinition* UPGPlayerShopSubsystem::GetDefaultCharacterSkin() const
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

const UPGCharacterSkinDefinition* UPGPlayerShopSubsystem::GetEquippedCharacterSkin() const
{
	if (ShopSaveGame && !ShopSaveGame->EquippedCharacterSkinId.IsNone())
	{
		if (const UPGCharacterSkinDefinition* EquippedSkin = FindCharacterSkin(ShopSaveGame->EquippedCharacterSkinId))
		{
			return EquippedSkin;
		}
	}

	return GetDefaultCharacterSkin();
}

void UPGPlayerShopSubsystem::RefreshShopCatalog()
{
	RefreshCatalogCache();
	EnsureDefaultOwnership();
}

bool UPGPlayerShopSubsystem::IsCharacterOwned(FName SkinId) const
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

	return ShopSaveGame && ShopSaveGame->OwnedCharacterSkinIds.Contains(SkinId);
}

bool UPGPlayerShopSubsystem::IsCharacterEquipped(FName SkinId) const
{
	return ShopSaveGame && !SkinId.IsNone() && ShopSaveGame->EquippedCharacterSkinId == SkinId;
}

bool UPGPlayerShopSubsystem::PurchaseCharacter(FName SkinId)
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

	UPGPlayerShopSaveGame* ShopSave = EnsureShopSave();
	if (!ShopSave)
	{
		return false;
	}

	UPGPlayerWalletSubsystem* WalletSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPGPlayerWalletSubsystem>() : nullptr;
	const int32 Price = FMath::Max(0, SkinDefinition->PriceReceipts);
	if (Price > 0 && (!WalletSubsystem || !WalletSubsystem->TrySpendReceipts(Price, TEXT("CharacterPurchase"))))
	{
		return false;
	}

	ShopSave->OwnedCharacterSkinIds.AddUnique(SkinId);
	if (!SaveShop())
	{
		ShopSave->OwnedCharacterSkinIds.Remove(SkinId);
		SaveShop();
		if (Price > 0 && WalletSubsystem)
		{
			WalletSubsystem->AddReceipts(Price, TEXT("CharacterPurchaseRollback"));
		}
		return false;
	}
	OnCharacterOwnershipChanged.Broadcast(SkinId);
	return true;
}

bool UPGPlayerShopSubsystem::EquipCharacter(FName SkinId)
{
	if (!IsCharacterOwned(SkinId) || !FindCharacterSkin(SkinId))
	{
		return false;
	}

	UPGPlayerShopSaveGame* ShopSave = EnsureShopSave();
	if (!ShopSave)
	{
		return false;
	}

	const FName PreviousEquippedSkinId = ShopSave->EquippedCharacterSkinId;
	ShopSave->EquippedCharacterSkinId = SkinId;
	if (!SaveShop())
	{
		ShopSave->EquippedCharacterSkinId = PreviousEquippedSkinId;
		SaveShop();
		return false;
	}
	ApplySkinToRunner(FindCharacterSkin(SkinId));
	OnEquippedCharacterChanged.Broadcast(SkinId);
	return true;
}

bool UPGPlayerShopSubsystem::PreviewCharacter(FName SkinId)
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

void UPGPlayerShopSubsystem::RestoreEquippedCharacter()
{
	ApplySkinToRunner(GetEquippedCharacterSkin());
}

void UPGPlayerShopSubsystem::LoadShop()
{
	if (USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(ShopSaveSlotName, ShopSaveUserIndex))
	{
		ShopSaveGame = Cast<UPGPlayerShopSaveGame>(LoadedSave);
	}

	if (!ShopSaveGame)
	{
		ShopSaveGame = Cast<UPGPlayerShopSaveGame>(UGameplayStatics::CreateSaveGameObject(UPGPlayerShopSaveGame::StaticClass()));
	}
}

bool UPGPlayerShopSubsystem::SaveShop() const
{
	if (!ShopSaveGame)
	{
		return false;
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(ShopSaveGame, ShopSaveSlotName, ShopSaveUserIndex);
	if (!bSaved)
	{
		UE_LOG(LogPGPlayerShop, Warning, TEXT("Failed to save player shop to slot '%s'."), ShopSaveSlotName);
	}
	return bSaved;
}

UPGPlayerShopSaveGame* UPGPlayerShopSubsystem::EnsureShopSave()
{
	if (!ShopSaveGame)
	{
		LoadShop();
	}
	return ShopSaveGame;
}

void UPGPlayerShopSubsystem::RefreshCatalogCache()
{
	CachedCharacterSkins.Reset();
	CachedCatalog = nullptr;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	CachedCatalog = Settings->Shop.CharacterShopCatalog.Get();
	if (!CachedCatalog)
	{
		return;
	}

	TSet<FName> SeenSkinIds;
	for (UPGCharacterSkinDefinition* SkinDefinition : CachedCatalog->CharacterSkins)
	{
		if (!SkinDefinition)
		{
			continue;
		}

		if (SkinDefinition->SkinId.IsNone())
		{
			UE_LOG(LogPGPlayerShop, Warning, TEXT("Character skin '%s' has empty SkinId."), *GetNameSafe(SkinDefinition));
			continue;
		}
		if (SeenSkinIds.Contains(SkinDefinition->SkinId))
		{
			UE_LOG(LogPGPlayerShop, Warning, TEXT("Duplicate character SkinId '%s' in shop catalog."), *SkinDefinition->SkinId.ToString());
			continue;
		}
		if (!SkinDefinition->SkeletalMesh)
		{
			UE_LOG(LogPGPlayerShop, Warning, TEXT("Character skin '%s' has no SkeletalMesh."), *SkinDefinition->SkinId.ToString());
		}

		SeenSkinIds.Add(SkinDefinition->SkinId);
		CachedCharacterSkins.Add(SkinDefinition);
	}

	if (!CachedCatalog->DefaultSkinId.IsNone() && !FindCharacterSkin(CachedCatalog->DefaultSkinId))
	{
		UE_LOG(LogPGPlayerShop, Warning, TEXT("DefaultSkinId '%s' was not found in character shop catalog."), *CachedCatalog->DefaultSkinId.ToString());
	}
}

void UPGPlayerShopSubsystem::EnsureDefaultOwnership()
{
	UPGPlayerShopSaveGame* ShopSave = EnsureShopSave();
	const UPGCharacterSkinDefinition* DefaultSkin = GetDefaultCharacterSkin();
	if (!ShopSave || !DefaultSkin)
	{
		return;
	}

	bool bChanged = false;
	if (!ShopSave->OwnedCharacterSkinIds.Contains(DefaultSkin->SkinId))
	{
		ShopSave->OwnedCharacterSkinIds.Add(DefaultSkin->SkinId);
		bChanged = true;
	}
	for (const UPGCharacterSkinDefinition* SkinDefinition : CachedCharacterSkins)
	{
		if (SkinDefinition && SkinDefinition->bUnlockedByDefault && !ShopSave->OwnedCharacterSkinIds.Contains(SkinDefinition->SkinId))
		{
			ShopSave->OwnedCharacterSkinIds.Add(SkinDefinition->SkinId);
			bChanged = true;
		}
	}
	if (ShopSave->EquippedCharacterSkinId.IsNone() || !IsCharacterOwned(ShopSave->EquippedCharacterSkinId))
	{
		ShopSave->EquippedCharacterSkinId = DefaultSkin->SkinId;
		bChanged = true;
	}

	if (bChanged)
	{
		SaveShop();
	}
}

bool UPGPlayerShopSubsystem::ApplySkinToRunner(const UPGCharacterSkinDefinition* SkinDefinition) const
{
	APGRunnerCharacter* RunnerCharacter = GetRunnerCharacter();
	return RunnerCharacter && SkinDefinition && RunnerCharacter->ApplyCharacterSkin(SkinDefinition);
}

APGRunnerCharacter* UPGPlayerShopSubsystem::GetRunnerCharacter() const
{
	return Cast<APGRunnerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}
