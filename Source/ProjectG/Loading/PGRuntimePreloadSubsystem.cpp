// Copyright Epic Games, Inc. All Rights Reserved.

#include "Loading/PGRuntimePreloadSubsystem.h"

#include "Cargo/PGCargoDropSet.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "RunEvents/PGRunEventPresentationSet.h"
#include "Settings/PGGameSettings.h"
#include "Shop/PGCharacterShopCatalog.h"
#include "Shop/PGCharacterSkinDefinition.h"
#include "Shop/PGMapDefinition.h"
#include "Shop/PGMapShopCatalog.h"
#include "Sound/SoundBase.h"
#include "Tutorial/PGTutorialPageSet.h"
#include "UI/PGRunResultWidget.h"
#include "UObject/UObjectGlobals.h"
#include "User/PGUserDataSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRuntimePreload, Log, All);

namespace
{
FName GetNormalizedTravelLevelName(const FString& LevelName)
{
	FString LevelPackageName = LevelName;
	int32 OptionsIndex = INDEX_NONE;
	if (LevelPackageName.FindChar(TEXT('?'), OptionsIndex))
	{
		LevelPackageName.LeftInline(OptionsIndex);
	}

	return FName(*UWorld::RemovePIEPrefix(FPackageName::GetShortName(LevelPackageName)));
}
}

void UPGRuntimePreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Home nested 예열은 UserData의 장착 스킨을 읽습니다. Subsystem 생성 순서에 기대지 않고
	// 저장 데이터 로드가 끝난 뒤 Home 예열을 시작하도록 UE 초기화 의존성을 명시합니다.
	Collection.InitializeDependency<UPGUserDataSubsystem>();
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UPGRuntimePreloadSubsystem::HandlePreLoadMap);

	// 앱 시작 시에는 Home 표시까지 필요한 최소 리소스만 준비합니다.
	// Run 리소스는 Home 표시 직후 백그라운드에서 이어서 요청합니다.
	RequestPreload(EPGRuntimePreloadGroup::Home);
}

void UPGRuntimePreloadSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	ReleaseHandles();
	Super::Deinitialize();
}

void UPGRuntimePreloadSubsystem::RequestPreload(EPGRuntimePreloadGroup Group)
{
	FPGPreloadGroupState& GroupState = GetOrAddGroupState(Group);

	// GameInstance는 맵 이동 후에도 유지됩니다. Run 그룹만 현재 열린 맵의 DropSet이 바뀌면 다시 준비합니다.
	if (Group == EPGRuntimePreloadGroup::Run && GroupState.bIsReady && !IsCurrentMapCargoDropSetReady(false))
	{
		ReleaseGroupHandles(GroupState);
	}

	if (GroupState.bIsReady || GroupState.bIsPreloading)
	{
		return;
	}

	GroupState.bIsPreloading = true;
	GroupState.RequestedResourcePaths.Reset();

	TArray<FSoftObjectPath> ResourcePaths;
	CollectPrimaryResourcePaths(Group, ResourcePaths);
	GroupState.RequestedResourcePaths.Append(ResourcePaths);
	if (ResourcePaths.IsEmpty())
	{
		HandlePrimaryResourcesLoaded(Group);
		return;
	}

	GroupState.PrimaryLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ResourcePaths,
		FStreamableDelegate::CreateUObject(this, &UPGRuntimePreloadSubsystem::HandlePrimaryResourcesLoaded, Group),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		FString::Printf(TEXT("PGRuntimePreloadPrimary_%d"), static_cast<int32>(Group)));
}

bool UPGRuntimePreloadSubsystem::IsGroupReady(EPGRuntimePreloadGroup Group) const
{
	const FPGPreloadGroupState* GroupState = FindGroupState(Group);
	return GroupState && GroupState->bIsReady;
}

bool UPGRuntimePreloadSubsystem::IsReadyForRun() const
{
	return IsGroupReady(EPGRuntimePreloadGroup::Run)
		&& AreRequiredRunResourcesReady(false)
		&& IsCurrentMapCargoDropSetReady(false);
}

void UPGRuntimePreloadSubsystem::LogRunReadinessIssues() const
{
	AreRequiredRunResourcesReady(true);
	IsCurrentMapCargoDropSetReady(true);
}

void UPGRuntimePreloadSubsystem::PreserveRunResourcesForNextSameMapTravel(FName ExpectedLevelName)
{
	// Retry 호출부는 짧은 레벨명을 전달하지만 PreLoadMap은 패키지 경로를 전달할 수 있다.
	// 비교 기준을 통일해 PIE와 패키징 실행 모두에서 동일 맵 Retry만 정확히 허용한다.
	PreservedRunResourcesExpectedLevelName = GetNormalizedTravelLevelName(ExpectedLevelName.ToString());
}

UPGRuntimePreloadSubsystem::FPGPreloadGroupState& UPGRuntimePreloadSubsystem::GetOrAddGroupState(EPGRuntimePreloadGroup Group)
{
	return GroupStates.FindOrAdd(Group);
}

const UPGRuntimePreloadSubsystem::FPGPreloadGroupState* UPGRuntimePreloadSubsystem::FindGroupState(EPGRuntimePreloadGroup Group) const
{
	return GroupStates.Find(Group);
}

void UPGRuntimePreloadSubsystem::CollectPrimaryResourcePaths(EPGRuntimePreloadGroup Group, TArray<FSoftObjectPath>& OutPaths) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	switch (Group)
	{
	case EPGRuntimePreloadGroup::Home:
		AddUniquePath(OutPaths, Settings->HomeFlow.HomeScreenWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->HomeFlow.SettingsScreenWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->HomeFlow.ScreenTransitionFadeWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->DebugUI.FPSDisplayWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Shop.CharacterShopCatalog.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Shop.MapShopCatalog.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Music.HomeMusic.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->SFX.CommonButtonClickSound.ToSoftObjectPath());
		// Receipt Shop이 닫혀 있어도 + 버튼의 구매 불가 피드백음은 Home에서 즉시 필요하다.
		AddUniquePath(OutPaths, Settings->SFX.ShopPurchaseFailSound.ToSoftObjectPath());
		break;

	case EPGRuntimePreloadGroup::Run:
		AddUniquePath(OutPaths, Settings->HomeFlow.RunCountdownWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.RunHUDWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.RunPauseWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.CargoDeliveryItemWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.CargoScoreIndicatorWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.RunGuideWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.CargoDeliveredVFX.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.CargoDeliveredSound.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunHUD.CargoDangerWarningSound.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunResult.GameOverWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunResult.GameOverSound.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunResult.GameOverVFX.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunResult.GameOverCameraShake.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunEvent.DefaultRunEventSet.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->RunEvent.DefaultPresentationSet.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->MovingHazard.HazardActorClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->MovingHazard.HitSound.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->MovingHazard.HitCameraShake.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Shop.MapShopCatalog.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Music.RunMusic.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->SFX.CountdownTickSound.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->SFX.CargoDropSound.ToSoftObjectPath());
		for (const TSoftObjectPtr<USoundBase>& JumpSound : Settings->SFX.MaleJumpSounds)
		{
			AddUniquePath(OutPaths, JumpSound.ToSoftObjectPath());
		}
		for (const TSoftObjectPtr<USoundBase>& JumpSound : Settings->SFX.FemaleJumpSounds)
		{
			AddUniquePath(OutPaths, JumpSound.ToSoftObjectPath());
		}
		break;

	case EPGRuntimePreloadGroup::Shop:
		AddUniquePath(OutPaths, Settings->Shop.ShopScreenWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Shop.CharacterShopItemWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Shop.CharacterShopCatalog.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Shop.MapShopItemWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->Shop.MapShopCatalog.ToSoftObjectPath());
		if (Settings->Shop.IsReceiptShopTestGrantEnabled())
		{
			AddUniquePath(OutPaths, Settings->Shop.ReceiptShopWidgetClass.ToSoftObjectPath());
			AddUniquePath(OutPaths, Settings->Shop.ReceiptShopPackItemWidgetClass.ToSoftObjectPath());
			for (const FPGReceiptShopPack& ReceiptPack : Settings->Shop.ReceiptPacks)
			{
				AddUniquePath(OutPaths, ReceiptPack.Thumbnail.ToSoftObjectPath());
			}
		}
		AddUniquePath(OutPaths, Settings->SFX.ShopPurchaseSuccessSound.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->SFX.ShopPurchaseFailSound.ToSoftObjectPath());
		break;

	case EPGRuntimePreloadGroup::Tutorial:
		AddUniquePath(OutPaths, Settings->HomeFlow.TutorialScreenWidgetClass.ToSoftObjectPath());
		AddUniquePath(OutPaths, Settings->HomeFlow.TutorialPageSet.ToSoftObjectPath());
		break;
	}
}

void UPGRuntimePreloadSubsystem::CollectNestedResourcePaths(EPGRuntimePreloadGroup Group, TArray<FSoftObjectPath>& OutPaths) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	if (Group == EPGRuntimePreloadGroup::Home)
	{
		// 카탈로그를 읽어도 판매 스킨 전체 메시를 올리지 않습니다. Home에는 현재 장착 스킨 하나만 필요합니다.
		if (const UPGUserDataSubsystem* UserData = GetGameInstance()->GetSubsystem<UPGUserDataSubsystem>())
		{
			if (const UPGCharacterSkinDefinition* EquippedSkin = UserData->GetEquippedCharacterSkin())
			{
				AddUniquePath(OutPaths, EquippedSkin->SkeletalMesh.ToSoftObjectPath());
				AddUniquePath(OutPaths, EquippedSkin->AnimClass.ToSoftObjectPath());
			}
		}
		return;
	}

	if (Group == EPGRuntimePreloadGroup::Run)
	{
		if (const UPGRunEventPresentationSet* PresentationSet = Settings->RunEvent.DefaultPresentationSet.Get())
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

		if (const UPGMapShopCatalog* MapCatalog = Settings->Shop.MapShopCatalog.Get())
		{
			bool bHasDuplicateLevel = false;
			if (const UPGMapDefinition* CurrentMap = MapCatalog->FindMapForWorld(GetWorld(), bHasDuplicateLevel))
			{
				AddUniquePath(OutPaths, CurrentMap->CargoDropSet.ToSoftObjectPath());
			}
		}
		return;
	}

	if (Group == EPGRuntimePreloadGroup::Shop)
	{
		// Shop 진입 전에는 카드에 필요한 가벼운 썸네일만 준비합니다.
		// 판매 스킨의 무거운 SkeletalMesh와 AnimClass는 실제 프리뷰 선택 시점에 별도로 로드합니다.
		if (const UPGCharacterShopCatalog* CharacterCatalog = Settings->Shop.CharacterShopCatalog.Get())
		{
			for (const UPGCharacterSkinDefinition* SkinDefinition : CharacterCatalog->CharacterSkins)
			{
				if (SkinDefinition)
				{
					AddUniquePath(OutPaths, SkinDefinition->Thumbnail.ToSoftObjectPath());
				}
			}
		}

		if (const UPGMapShopCatalog* MapCatalog = Settings->Shop.MapShopCatalog.Get())
		{
			for (const UPGMapDefinition* MapDefinition : MapCatalog->Maps)
			{
				if (MapDefinition)
				{
					AddUniquePath(OutPaths, MapDefinition->Thumbnail.ToSoftObjectPath());
				}
			}
		}
		return;
	}

	if (Group == EPGRuntimePreloadGroup::Tutorial)
	{
		if (const UPGTutorialPageSet* PageSet = Settings->HomeFlow.TutorialPageSet.Get())
		{
			for (const TSoftObjectPtr<UTexture2D>& PageTexture : PageSet->Pages)
			{
				AddUniquePath(OutPaths, PageTexture.ToSoftObjectPath());
			}
		}
	}
}

void UPGRuntimePreloadSubsystem::AddUniquePath(TArray<FSoftObjectPath>& OutPaths, const FSoftObjectPath& Path) const
{
	if (Path.IsValid())
	{
		OutPaths.AddUnique(Path);
	}
}

void UPGRuntimePreloadSubsystem::HandlePrimaryResourcesLoaded(EPGRuntimePreloadGroup Group)
{
	FPGPreloadGroupState& GroupState = GetOrAddGroupState(Group);

	// Home primary 단계에서 카탈로그가 준비됩니다. 장착 스킨 하나를 nested 단계에 넣기 전에 저장 상태를 보정합니다.
	if (Group == EPGRuntimePreloadGroup::Home)
	{
		if (UPGUserDataSubsystem* UserData = GetGameInstance()->GetSubsystem<UPGUserDataSubsystem>())
		{
			UserData->RefreshShopCatalog();
		}
	}

	TArray<FSoftObjectPath> ResourcePaths;
	CollectNestedResourcePaths(Group, ResourcePaths);
	for (const FSoftObjectPath& ResourcePath : ResourcePaths)
	{
		GroupState.RequestedResourcePaths.AddUnique(ResourcePath);
	}

	if (ResourcePaths.IsEmpty())
	{
		CompletePreload(Group);
		return;
	}

	GroupState.NestedLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ResourcePaths,
		FStreamableDelegate::CreateUObject(this, &UPGRuntimePreloadSubsystem::HandleNestedResourcesLoaded, Group),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		FString::Printf(TEXT("PGRuntimePreloadNested_%d"), static_cast<int32>(Group)));
}

void UPGRuntimePreloadSubsystem::HandleNestedResourcesLoaded(EPGRuntimePreloadGroup Group)
{
	CompletePreload(Group);
}

void UPGRuntimePreloadSubsystem::CompletePreload(EPGRuntimePreloadGroup Group)
{
	FPGPreloadGroupState& GroupState = GetOrAddGroupState(Group);
	GroupState.bIsPreloading = false;
	ValidateLoadedResources(Group);
	GroupState.bIsReady = true;
	OnPreloadCompleted.Broadcast(Group);
}

void UPGRuntimePreloadSubsystem::ValidateLoadedResources(EPGRuntimePreloadGroup Group)
{
	const FPGPreloadGroupState* GroupState = FindGroupState(Group);
	if (!GroupState)
	{
		return;
	}

	TArray<FString> FailedPaths;
	for (const FSoftObjectPath& ResourcePath : GroupState->RequestedResourcePaths)
	{
		if (ResourcePath.IsValid() && !ResourcePath.ResolveObject())
		{
			FailedPaths.Add(ResourcePath.ToString());
		}
	}

	if (!FailedPaths.IsEmpty())
	{
		UE_LOG(LogPGRuntimePreload, Warning, TEXT("Runtime preload group %d could not load assigned resources: %s"), static_cast<int32>(Group), *FString::Join(FailedPaths, TEXT(", ")));
	}

	if (Group == EPGRuntimePreloadGroup::Run)
	{
		AreRequiredRunResourcesReady(true);
		IsCurrentMapCargoDropSetReady(true);
	}
}

bool UPGRuntimePreloadSubsystem::AreRequiredRunResourcesReady(bool bLogIssues) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const bool bReady = Settings
		&& Settings->HomeFlow.RunCountdownWidgetClass.Get()
		&& Settings->RunHUD.RunHUDWidgetClass.Get()
		&& Settings->RunHUD.RunPauseWidgetClass.Get()
		&& Settings->RunResult.GameOverWidgetClass.Get()
		&& Settings->RunEvent.DefaultRunEventSet.Get();

	if (!bReady && bLogIssues)
	{
		UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: check Countdown, RunHUD, Pause, RunResult, and RunEventSet settings."));
	}
	return bReady;
}

bool UPGRuntimePreloadSubsystem::IsCurrentMapCargoDropSetReady(bool bLogIssues) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
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
	if (bHasDuplicateLevel || !CurrentMap)
	{
		if (bLogIssues)
		{
			UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: current level '%s' must be registered exactly once in MapShopCatalog."), *UPGMapShopCatalog::GetNormalizedWorldPackageName(World));
		}
		return false;
	}
	if (CurrentMap->CargoDropSet.IsNull() || !CurrentMap->CargoDropSet.Get())
	{
		if (bLogIssues)
		{
			UE_LOG(LogPGRuntimePreload, Warning, TEXT("Run start blocked: map '%s' CargoDropSet is missing or not loaded."), *CurrentMap->MapId.ToString());
		}
		return false;
	}
	return true;
}

void UPGRuntimePreloadSubsystem::ReleaseGroupHandles(FPGPreloadGroupState& GroupState)
{
	if (GroupState.NestedLoadHandle.IsValid())
	{
		if (GroupState.NestedLoadHandle->IsLoadingInProgress())
		{
			GroupState.NestedLoadHandle->CancelHandle();
		}
		else
		{
			GroupState.NestedLoadHandle->ReleaseHandle();
		}
		GroupState.NestedLoadHandle.Reset();
	}
	if (GroupState.PrimaryLoadHandle.IsValid())
	{
		if (GroupState.PrimaryLoadHandle->IsLoadingInProgress())
		{
			GroupState.PrimaryLoadHandle->CancelHandle();
		}
		else
		{
			GroupState.PrimaryLoadHandle->ReleaseHandle();
		}
		GroupState.PrimaryLoadHandle.Reset();
	}

	GroupState.RequestedResourcePaths.Reset();
	GroupState.bIsReady = false;
	GroupState.bIsPreloading = false;
}

void UPGRuntimePreloadSubsystem::HandlePreLoadMap(const FString& MapName)
{
	// Home 최소 리소스는 다음 레벨에서도 바로 재사용한다. 나머지 그룹은 이전 월드의 무거운 리소스를
	// 새 월드 로드와 겹치지 않게 먼저 해제하고, 실제 진입 흐름에서 필요한 그룹만 다시 예열한다.
	const FName LoadedLevelName = GetNormalizedTravelLevelName(MapName);
	const bool bIsExpectedSameMapRetry = !PreservedRunResourcesExpectedLevelName.IsNone()
		&& LoadedLevelName == PreservedRunResourcesExpectedLevelName;
	PreservedRunResourcesExpectedLevelName = NAME_None;

	const EPGRuntimePreloadGroup GroupsToRelease[] =
	{
		EPGRuntimePreloadGroup::Run,
		EPGRuntimePreloadGroup::Shop,
		EPGRuntimePreloadGroup::Tutorial
	};

	for (const EPGRuntimePreloadGroup Group : GroupsToRelease)
	{
		if (FPGPreloadGroupState* GroupState = GroupStates.Find(Group))
		{
			// 같은 맵 Retry는 이미 준비된 에셋 핸들만 다음 월드에서 재사용한다.
			// 예열이 끝나지 않았다면 이전 월드의 비동기 요청을 넘기지 않고 기존 재예열 경로를 사용한다.
			if (Group == EPGRuntimePreloadGroup::Run
				&& bIsExpectedSameMapRetry
				&& GroupState->bIsReady
				&& !GroupState->bIsPreloading)
			{
				continue;
			}

			ReleaseGroupHandles(*GroupState);
		}
	}
}

void UPGRuntimePreloadSubsystem::ReleaseHandles()
{
	PreservedRunResourcesExpectedLevelName = NAME_None;
	for (TPair<EPGRuntimePreloadGroup, FPGPreloadGroupState>& Pair : GroupStates)
	{
		ReleaseGroupHandles(Pair.Value);
	}
	GroupStates.Reset();
}
