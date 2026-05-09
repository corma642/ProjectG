// Copyright Epic Games, Inc. All Rights Reserved.

#include "Loading/PGRuntimePreloadSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RunEvents/PGRunEventPresentationSet.h"
#include "Settings/PGGameSettings.h"

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

	AddUniquePath(OutPaths, Settings->RunHUD.RunHUDWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunHUD.RunPauseWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunHUD.CargoDeliveryItemWidgetClass.ToSoftObjectPath());

	AddUniquePath(OutPaths, Settings->RunResult.GameOverWidgetClass.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunResult.GameOverSound.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunResult.GameOverVFX.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunResult.GameOverCameraShake.ToSoftObjectPath());

	AddUniquePath(OutPaths, Settings->RunEvent.DefaultRunEventSet.ToSoftObjectPath());
	AddUniquePath(OutPaths, Settings->RunEvent.DefaultPresentationSet.ToSoftObjectPath());

	AddUniquePath(OutPaths, Settings->Cargo.DefaultCargoType.ToSoftObjectPath());
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
