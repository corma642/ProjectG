// Copyright Epic Games, Inc. All Rights Reserved.

#include "Loading/PGRuntimePreloadSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RunEvents/PGRunEventPresentationSet.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterShopCatalog.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Sound/SoundBase.h"
#include "Tutorial/PGTutorialPageSet.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRuntimePreload, Log, All);

void UPGRuntimePreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartPreload();
}

void UPGRuntimePreloadSubsystem::Deinitialize()
{
	ReleaseHandles();
	Super::Deinitialize();
}

void UPGRuntimePreloadSubsystem::StartPreload()
{
	if (bIsReady || bIsPreloading)
	{
		return;
	}

	bIsPreloading = true;

	TArray<FSoftObjectPath> ResourcePaths;
	CollectPrimaryResourcePaths(ResourcePaths);
	if (ResourcePaths.IsEmpty())
	{
		HandlePrimaryResourcesLoaded();
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	PrimaryLoadHandle = StreamableManager.RequestAsyncLoad(
		ResourcePaths,
		FStreamableDelegate::CreateUObject(this, &UPGRuntimePreloadSubsystem::HandlePrimaryResourcesLoaded),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("PGRuntimePrimaryResources"));
}

void UPGRuntimePreloadSubsystem::CollectPrimaryResourcePaths(TArray<FSoftObjectPath>& OutPaths) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	AddUniquePath(OutPaths, Settings->HomeFlow.HomeScreenWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->HomeFlow.RunCountdownWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->HomeFlow.SettingsScreenWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->HomeFlow.TutorialScreenWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->HomeFlow.TutorialPageSet.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->HomeFlow.ScreenTransitionFadeWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->Shop.ShopScreenWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->Shop.CharacterShopItemWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->Shop.CharacterShopCatalog.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->Shop.ReceiptShopWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->Shop.ReceiptShopPackItemWidgetClass.ToSoftObjectPath());
	for (const FPGReceiptShopPack& ReceiptPack : Settings->Shop.ReceiptPacks)
	{
		AddUniquePath(OutPaths, ReceiptPack.Thumbnail.ToSoftObjectPath());
	}
	AddUniquePath(OutPaths, Settings->Music.HomeMusic.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->Music.RunMusic.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->SFX.CommonButtonClickSound.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->SFX.CountdownTickSound.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->SFX.ShopPurchaseSuccessSound.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->SFX.ShopPurchaseFailSound.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->SFX.CargoDropSound.ToSoftObjectPath());
	for (const TSoftObjectPtr<USoundBase>& JumpSound : Settings->SFX.MaleJumpSounds)
	{
		AddUniquePath(OutPaths, JumpSound.ToSoftObjectPath());
	}
	for (const TSoftObjectPtr<USoundBase>& JumpSound : Settings->SFX.FemaleJumpSounds)
	{
		AddUniquePath(OutPaths, JumpSound.ToSoftObjectPath());
	}

	AddUniquePath(OutPaths, Settings->RunHUD.RunHUDWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunHUD.RunPauseWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunHUD.CargoDeliveryItemWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunHUD.CargoScoreIndicatorWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunHUD.CargoDeliveredVFX.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunHUD.CargoDeliveredSound.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->DebugUI.FPSDisplayWidgetClass.ToSoftObjectPath());

	AddUniquePath(OutPaths, Settings->RunResult.GameOverWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunResult.GameOverSound.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunResult.GameOverVFX.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunResult.GameOverCameraShake.ToSoftObjectPath());

	AddUniquePath(OutPaths, Settings->RunEvent.DefaultRunEventSet.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunEvent.DefaultPresentationSet.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->MovingHazard.HazardActorClass.ToSoftObjectPath());

	AddUniquePath(OutPaths, Settings->CargoDelivery.DefaultCargoDropSet.ToSoftObjectPath());
}

void UPGRuntimePreloadSubsystem::CollectNestedResourcePaths(TArray<FSoftObjectPath>& OutPaths) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	const UPGRunEventPresentationSet* PresentationSet = Settings->RunEvent.DefaultPresentationSet.Get();
	if (PresentationSet)
	{
		for (const FPGRunEventPresentationEntry& Entry : PresentationSet->Entries)
		{
			AddUniquePath(OutPaths, Entry.PreviewWidgetClass.ToSoftObjectPath());
			AddUniquePath(OutPaths, Entry.ActiveWidgetClass.ToSoftObjectPath());
			AddUniquePath(OutPaths, Entry.PreviewNiagaraSystem.ToSoftObjectPath());
			AddUniquePath(OutPaths, Entry.ActiveNiagaraSystem.ToSoftObjectPath());
			AddUniquePath(OutPaths, Entry.PreviewSound.ToSoftObjectPath());
			AddUniquePath(OutPaths, Entry.TriggerSound.ToSoftObjectPath());
			AddUniquePath(OutPaths, Entry.ActiveLoopSound.ToSoftObjectPath());
		}
	}

	const UPGTutorialPageSet* TutorialPageSet = Settings->HomeFlow.TutorialPageSet.Get();
	if (TutorialPageSet)
	{
		for (const TSoftObjectPtr<UTexture2D>& PageTexture : TutorialPageSet->Pages)
		{
			AddUniquePath(OutPaths, PageTexture.ToSoftObjectPath());
		}
	}

	const UPGCharacterShopCatalog* ShopCatalog = Settings->Shop.CharacterShopCatalog.Get();
	if (ShopCatalog)
	{
		for (const UPGCharacterSkinDefinition* SkinDefinition : ShopCatalog->CharacterSkins)
		{
			if (!SkinDefinition)
			{
				continue;
			}

			AddUniquePath(OutPaths, FSoftObjectPath(SkinDefinition));
			if (SkinDefinition->Thumbnail)
			{
				AddUniquePath(OutPaths, FSoftObjectPath(SkinDefinition->Thumbnail.Get()));
			}
			if (SkinDefinition->SkeletalMesh)
			{
				AddUniquePath(OutPaths, FSoftObjectPath(SkinDefinition->SkeletalMesh.Get()));
			}
			if (SkinDefinition->AnimClass)
			{
				AddUniquePath(OutPaths, FSoftObjectPath(SkinDefinition->AnimClass));
			}
		}
	}
}

void UPGRuntimePreloadSubsystem::AddUniquePath(TArray<FSoftObjectPath>& OutPaths, const FSoftObjectPath& Path) const
{
	if (!Path.IsValid())
	{
		return;
	}

	OutPaths.AddUnique(Path);
}

void UPGRuntimePreloadSubsystem::HandlePrimaryResourcesLoaded()
{
	TArray<FSoftObjectPath> ResourcePaths;
	CollectNestedResourcePaths(ResourcePaths);
	if (ResourcePaths.IsEmpty())
	{
		CompletePreload();
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	NestedLoadHandle = StreamableManager.RequestAsyncLoad(
		ResourcePaths,
		FStreamableDelegate::CreateUObject(this, &UPGRuntimePreloadSubsystem::HandleNestedResourcesLoaded),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("PGRuntimeNestedResources"));
}

void UPGRuntimePreloadSubsystem::HandleNestedResourcesLoaded()
{
	CompletePreload();
}

void UPGRuntimePreloadSubsystem::CompletePreload()
{
	bIsPreloading = false;
	bIsReady = true;
	OnPreloadCompleted.Broadcast();
}

void UPGRuntimePreloadSubsystem::ReleaseHandles()
{
	if (NestedLoadHandle.IsValid())
	{
		NestedLoadHandle->ReleaseHandle();
		NestedLoadHandle.Reset();
	}

	if (PrimaryLoadHandle.IsValid())
	{
		PrimaryLoadHandle->ReleaseHandle();
		PrimaryLoadHandle.Reset();
	}

	bIsReady = false;
	bIsPreloading = false;
}
