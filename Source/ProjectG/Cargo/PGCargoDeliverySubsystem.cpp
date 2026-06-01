// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoDeliverySubsystem.h"

#include "Cargo/PGCargoDropSet.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Cargo/PGCargoTypeDefinition.h"
#include "Character/PGRunnerCharacter.h"
#include "GameModes/PGGameModeBase.h"
#include "Shop/PGMapDefinition.h"
#include "Shop/PGMapShopCatalog.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGCargoDeliverySubsystem, Log, All);

void UPGCargoDeliverySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadSettings();
	const int32 Seed = CachedDeliverySettings.bUseRandomSeedEachRun ? FMath::Rand() : CachedDeliverySettings.RandomSeed;
	RandomStream.Initialize(Seed);

	InitializeRunnerReferences();
	BindGameMode();
}

void UPGCargoDeliverySubsystem::Deinitialize()
{
	if (BoundGameMode && bIsBoundToGameMode)
	{
		BoundGameMode->OnRunStarted.RemoveDynamic(this, &UPGCargoDeliverySubsystem::HandleRunStarted);
		BoundGameMode->OnRunFailed.RemoveDynamic(this, &UPGCargoDeliverySubsystem::HandleRunFailed);
	}

	BoundGameMode = nullptr;
	bIsBoundToGameMode = false;
	bIsRunActive = false;
	Super::Deinitialize();
}

void UPGCargoDeliverySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindGameMode();
}

bool UPGCargoDeliverySubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UPGCargoDeliverySubsystem::Tick(float DeltaTime)
{
	if (!bIsBoundToGameMode)
	{
		BindGameMode();
		if (!bIsBoundToGameMode)
		{
			return;
		}
	}

	if (!bIsRunActive)
	{
		return;
	}

	TickAccumulator += DeltaTime;
	if (TickAccumulator < 0.2f)
	{
		return;
	}

	const float ProcessDeltaSeconds = TickAccumulator;
	TickAccumulator = 0.0f;
	ProcessCargoDeliveryTick(ProcessDeltaSeconds);
}

TStatId UPGCargoDeliverySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPGCargoDeliverySubsystem, STATGROUP_Tickables);
}

bool UPGCargoDeliverySubsystem::IsTickable() const
{
	return !IsTemplate() && (bIsRunActive || !bIsBoundToGameMode);
}

float UPGCargoDeliverySubsystem::GetDifficultyAlpha() const
{
	return CachedDeliverySettings.DifficultyRampDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(RunElapsedTime / CachedDeliverySettings.DifficultyRampDuration, 0.0f, 1.0f)
		: 1.0f;
}

void UPGCargoDeliverySubsystem::HandleRunStarted()
{
	bIsRunActive = true;
	RunElapsedTime = 0.0f;
	TickAccumulator = 0.0f;
	bWarnedMissingCargoStack = false;
	bWarnedMissingDropSet = false;
	bWarnedMissingCargoType = false;
	bWarnedInvalidCargoMesh = false;
	ResolveCurrentMapDropSet();
	InitializeRunnerReferences();

	if (CachedDeliverySettings.FirstDropDelay <= KINDA_SMALL_NUMBER)
	{
		if (TryStartCargoDrop())
		{
			ScheduleNextDrop(CalculateCurrentDropInterval());
			return;
		}
		ScheduleNextDrop(0.5f);
		return;
	}

	ScheduleNextDrop(CachedDeliverySettings.FirstDropDelay);
}

void UPGCargoDeliverySubsystem::HandleRunFailed(FName FailureReason)
{
	bIsRunActive = false;
	RunElapsedTime = 0.0f;
	NextDropTime = 0.0f;
	TickAccumulator = 0.0f;
}

void UPGCargoDeliverySubsystem::LoadSettings()
{
	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedDeliverySettings = Settings->CargoDelivery;
	}
}

bool UPGCargoDeliverySubsystem::ResolveCurrentMapDropSet()
{
	LoadedDropSet = nullptr;
	bHasCurrentMapCargoLossWorldZ = false;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const UPGMapShopCatalog* MapCatalog = Settings ? Settings->Shop.MapShopCatalog.Get() : nullptr;
	if (!MapCatalog)
	{
		return false;
	}

	bool bHasDuplicateLevel = false;
	const UPGMapDefinition* MapDefinition = MapCatalog->FindMapForWorld(GetWorld(), bHasDuplicateLevel);
	if (!MapDefinition || MapDefinition->CargoDropSet.IsNull())
	{
		return false;
	}

	// 적재물 종류는 장착 맵 저장값이 아니라 실제로 플레이 중인 레벨을 기준으로 선택합니다.
	// 이 기준을 사용해야 PIE 직접 실행과 Retry에서도 현재 맵 전용 적재물이 유지됩니다.
	LoadedDropSet = MapDefinition->CargoDropSet.Get();
	CurrentMapCargoLossWorldZ = MapDefinition->CargoLossWorldZ;
	bHasCurrentMapCargoLossWorldZ = true;
	return LoadedDropSet != nullptr;
}

void UPGCargoDeliverySubsystem::BindGameMode()
{
	if (bIsBoundToGameMode || !GetWorld())
	{
		return;
	}

	BoundGameMode = GetWorld()->GetAuthGameMode<APGGameModeBase>();
	if (!BoundGameMode)
	{
		return;
	}

	BoundGameMode->OnRunStarted.AddDynamic(this, &UPGCargoDeliverySubsystem::HandleRunStarted);
	BoundGameMode->OnRunFailed.AddDynamic(this, &UPGCargoDeliverySubsystem::HandleRunFailed);
	bIsBoundToGameMode = true;

	if (BoundGameMode->GetRunState() == EPGRunState::Running)
	{
		HandleRunStarted();
	}
}

void UPGCargoDeliverySubsystem::InitializeRunnerReferences()
{
	RunnerCharacter = BoundGameMode ? BoundGameMode->GetPlayableRunnerCharacter() : nullptr;
	CargoStackComponent = RunnerCharacter ? RunnerCharacter->FindComponentByClass<UPGCargoStackComponent>() : nullptr;
	if (CargoStackComponent && bHasCurrentMapCargoLossWorldZ)
	{
		// Runner 참조가 늦게 준비되는 Retry 흐름에서도 현재 열린 맵의 고정 손실선을 다시 적용합니다.
		CargoStackComponent->SetCargoLossWorldZ(CurrentMapCargoLossWorldZ);
	}
}

void UPGCargoDeliverySubsystem::ProcessCargoDeliveryTick(float DeltaSeconds)
{
	BindGameMode();
	if (!BoundGameMode || BoundGameMode->GetRunState() != EPGRunState::Running)
	{
		return;
	}

	RunElapsedTime += DeltaSeconds;
	if (!CargoStackComponent)
	{
		InitializeRunnerReferences();
	}

	if (RunElapsedTime >= NextDropTime)
	{
		if (TryStartCargoDrop())
		{
			ScheduleNextDrop(CalculateCurrentDropInterval());
		}
		else
		{
			ScheduleNextDrop(0.5f);
		}
	}

}

bool UPGCargoDeliverySubsystem::TryStartCargoDrop()
{
	if (!CargoStackComponent)
	{
		InitializeRunnerReferences();
	}

	if (!CargoStackComponent || CargoStackComponent->HasPendingCargoDrop())
	{
		if (!CargoStackComponent && !bWarnedMissingCargoStack)
		{
			UE_LOG(LogPGCargoDeliverySubsystem, Warning, TEXT("Cargo drop failed: UPGCargoStackComponent was not found on the runner."));
			bWarnedMissingCargoStack = true;
		}
		return false;
	}

	UPGCargoTypeDefinition* CargoType = SelectCargoType();
	if (!CargoType)
	{
		if (LoadedDropSet && !bWarnedMissingCargoType)
		{
			UE_LOG(LogPGCargoDeliverySubsystem, Warning, TEXT("Cargo drop failed: the current map CargoDropSet has no eligible CargoType for elapsed run time %.2f."), RunElapsedTime);
			bWarnedMissingCargoType = true;
		}
		return false;
	}

	if (!CargoType->CargoMesh)
	{
		if (!bWarnedInvalidCargoMesh)
		{
			UE_LOG(LogPGCargoDeliverySubsystem, Warning, TEXT("Cargo drop failed: selected CargoType '%s' has no CargoMesh."), *GetNameSafe(CargoType));
			bWarnedInvalidCargoMesh = true;
		}
		return false;
	}

	return CargoStackComponent->AddCargoByType(CargoType);
}

UPGCargoTypeDefinition* UPGCargoDeliverySubsystem::SelectCargoType()
{
	if (!LoadedDropSet)
	{
		if (!ResolveCurrentMapDropSet() && !bWarnedMissingDropSet)
		{
			UE_LOG(LogPGCargoDeliverySubsystem, Warning, TEXT("Cargo drop failed: the current level has no loaded map-specific CargoDropSet."));
			bWarnedMissingDropSet = true;
		}
	}

	if (LoadedDropSet && !LoadedDropSet->Entries.IsEmpty())
	{
		float TotalWeight = 0.0f;
		for (const FPGCargoDropEntry& Entry : LoadedDropSet->Entries)
		{
			if (!Entry.CargoType || RunElapsedTime < Entry.MinRunTime)
			{
				continue;
			}
			TotalWeight += FMath::Max(0.0f, Entry.BaseWeight);
		}

		if (TotalWeight > KINDA_SMALL_NUMBER)
		{
			const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
			float AccumulatedWeight = 0.0f;
			for (const FPGCargoDropEntry& Entry : LoadedDropSet->Entries)
			{
				if (!Entry.CargoType || RunElapsedTime < Entry.MinRunTime)
				{
					continue;
				}

				AccumulatedWeight += FMath::Max(0.0f, Entry.BaseWeight);
				if (Roll <= AccumulatedWeight)
				{
					return Entry.CargoType;
				}
			}
		}
	}

	return nullptr;
}

void UPGCargoDeliverySubsystem::ScheduleNextDrop(float DelaySeconds)
{
	NextDropTime = RunElapsedTime + FMath::Max(0.0f, DelaySeconds);
}

float UPGCargoDeliverySubsystem::CalculateCurrentDropInterval() const
{
	const float BaseInterval = FMath::Max(0.5f, CachedDeliverySettings.BaseDropInterval);
	const float MinInterval = FMath::Max(0.5f, CachedDeliverySettings.MinDropInterval);
	return FMath::Lerp(BaseInterval, MinInterval, GetDifficultyAlpha());
}

