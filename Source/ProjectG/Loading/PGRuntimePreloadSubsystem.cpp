// Copyright Epic Games, Inc. All Rights Reserved.

#include "Loading/PGRuntimePreloadSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Cargo/PGCargoDropSet.h"
#include "RunEvents/PGRunEventPresentationSet.h"
#include "RunEvents/PGRunEventSet.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterShopCatalog.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Shop/PGMapDefinition.h"
#include "Shop/PGMapShopCatalog.h"
#include "Sound/SoundBase.h"
#include "Tutorial/PGTutorialPageSet.h"
#include "UI/PGRunResultWidget.h"

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
	RequestedResourcePaths.Reset();

	TArray<FSoftObjectPath> ResourcePaths;
	CollectPrimaryResourcePaths(ResourcePaths);
	RequestedResourcePaths.Append(ResourcePaths);
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
	AddUniquePath(OutPaths, Settings->Shop.MapShopItemWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->Shop.MapShopCatalog.ToSoftObjectPath());
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

	const UPGMapShopCatalog* MapCatalog = Settings->Shop.MapShopCatalog.Get();
	if (MapCatalog)
	{
		for (const UPGMapDefinition* MapDefinition : MapCatalog->Maps)
		{
			if (!MapDefinition)
			{
				continue;
			}

			AddUniquePath(OutPaths, FSoftObjectPath(MapDefinition));
			if (MapDefinition->Thumbnail)
			{
				AddUniquePath(OutPaths, FSoftObjectPath(MapDefinition->Thumbnail.Get()));
			}
			AddUniquePath(OutPaths, MapDefinition->CargoDropSet.ToSoftObjectPath());
		}
	}
}

bool UPGRuntimePreloadSubsystem::IsReadyForRun() const
{
	return bIsReady && bAreRequiredRunResourcesReady && IsCurrentMapCargoDropSetReady(false);
}

void UPGRuntimePreloadSubsystem::LogRunReadinessIssues() const
{
	if (!bAreRequiredRunResourcesReady)
	{
		UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: check Countdown, RunHUD, Pause, RunResult, and RunEventSet settings."));
	}

	IsCurrentMapCargoDropSetReady(true);
}

bool UPGRuntimePreloadSubsystem::IsCurrentMapCargoDropSetReady(bool bLogIssues) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		// GameInstance 예열은 첫 월드 생성보다 먼저 끝날 수 있습니다. START 시점에는 월드가 있으므로
		// 여기서는 설정 오류처럼 경고하지 않고 준비되지 않은 상태로만 처리합니다.
		return false;
	}

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const UPGMapShopCatalog* MapCatalog = Settings ? Settings->Shop.MapShopCatalog.Get() : nullptr;
	if (!MapCatalog)
	{
		if (bLogIssues)
		{
			UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: MapShopCatalog is not assigned or loaded."));
		}
		return false;
	}

	bool bHasDuplicateLevel = false;
	const UPGMapDefinition* CurrentMap = MapCatalog->FindMapForWorld(World, bHasDuplicateLevel);
	if (bHasDuplicateLevel)
	{
		if (bLogIssues)
		{
			UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: the current level is registered more than once in MapShopCatalog."));
		}
		return false;
	}
	if (!CurrentMap)
	{
		if (bLogIssues)
		{
			UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: current level '%s' is not registered in MapShopCatalog."), *UPGMapShopCatalog::GetNormalizedWorldPackageName(World));
		}
		return false;
	}
	if (CurrentMap->CargoDropSet.IsNull())
	{
		if (bLogIssues)
		{
			UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: map '%s' has no CargoDropSet."), *CurrentMap->MapId.ToString());
		}
		return false;
	}
	if (!CurrentMap->CargoDropSet.Get())
	{
		if (bLogIssues)
		{
			UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: map '%s' CargoDropSet '%s' is not loaded."), *CurrentMap->MapId.ToString(), *CurrentMap->CargoDropSet.ToSoftObjectPath().ToString());
		}
		return false;
	}

	return true;
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
	for (const FSoftObjectPath& ResourcePath : ResourcePaths)
	{
		RequestedResourcePaths.AddUnique(ResourcePath);
	}
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
	ValidateLoadedResources();
	bIsReady = true;
	OnPreloadCompleted.Broadcast();
}

void UPGRuntimePreloadSubsystem::ValidateLoadedResources()
{
	TArray<FString> FailedPaths;
	for (const FSoftObjectPath& ResourcePath : RequestedResourcePaths)
	{
		if (ResourcePath.IsValid() && !ResourcePath.ResolveObject())
		{
			FailedPaths.Add(ResourcePath.ToString());
		}
	}

	if (!FailedPaths.IsEmpty())
	{
		UE_LOG(LogPGRuntimePreload, Warning, TEXT("Runtime preload could not load assigned resources: %s"), *FString::Join(FailedPaths, TEXT(", ")));
	}

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	bAreRequiredRunResourcesReady = Settings
		&& Settings->HomeFlow.RunCountdownWidgetClass.Get()
		&& Settings->RunHUD.RunHUDWidgetClass.Get()
		&& Settings->RunHUD.RunPauseWidgetClass.Get()
		&& Settings->RunResult.GameOverWidgetClass.Get()
		&& Settings->RunEvent.DefaultRunEventSet.Get();

	if (!bAreRequiredRunResourcesReady)
	{
		UE_LOG(LogPGRuntimePreload, Warning, TEXT("Runtime preload completed, but one or more required run resources are missing. Check Countdown, RunHUD, Pause, RunResult, and RunEventSet settings."));
	}

	// 맵 전용 CargoDropSet은 월드가 열린 뒤에만 판별할 수 있으므로 정적 필수 리소스와 분리해 검증합니다.
	IsCurrentMapCargoDropSetReady(true);
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
	bAreRequiredRunResourcesReady = false;
	RequestedResourcePaths.Reset();
}
