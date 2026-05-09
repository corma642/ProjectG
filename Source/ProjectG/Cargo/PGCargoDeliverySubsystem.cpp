// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoDeliverySubsystem.h"

#include "Cargo/PGCargoDropSet.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Cargo/PGCargoTypeDefinition.h"
#include "Character/PGRunnerCharacter.h"
#include "DrawDebugHelpers.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"

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
	bWarnedActiveCargoCount = false;
	bWarnedMissingCargoStack = false;
	bWarnedMissingCargoType = false;
	bWarnedInvalidCargoMesh = false;
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
		CachedCargoSettings = Settings->Cargo;
		LoadedDropSet = CachedDeliverySettings.DefaultCargoDropSet.Get();
	}
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
	RunnerCharacter = Cast<APGRunnerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	CargoStackComponent = RunnerCharacter ? RunnerCharacter->FindComponentByClass<UPGCargoStackComponent>() : nullptr;
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

	if (CargoStackComponent && CachedDeliverySettings.ActiveCargoWarningCount > 0)
	{
		const int32 CurrentCargoCount = CargoStackComponent->GetCurrentCargoCount();
		if (!bWarnedActiveCargoCount && CurrentCargoCount >= CachedDeliverySettings.ActiveCargoWarningCount)
		{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
			UE_LOG(LogPGCargoDeliverySubsystem, Warning, TEXT("Active cargo count reached %d. Check CargoDelivery interval and DeliveryDuration tuning."), CurrentCargoCount);
#endif
			bWarnedActiveCargoCount = true;
		}
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

	DrawCargoDeliveryDebug();
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
		if (!bWarnedMissingCargoType)
		{
			UE_LOG(LogPGCargoDeliverySubsystem, Warning, TEXT("Cargo drop failed: no eligible CargoType was selected and DefaultCargoType fallback could not be loaded."));
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
		LoadedDropSet = CachedDeliverySettings.DefaultCargoDropSet.Get();
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

	return CachedCargoSettings.DefaultCargoType.Get();
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

void UPGCargoDeliverySubsystem::DrawCargoDeliveryDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!CachedDeliverySettings.bDrawCargoDeliveryDebug || !GetWorld())
	{
		return;
	}

	const FVector DebugLocation = RunnerCharacter ? RunnerCharacter->GetActorLocation() + FVector::UpVector * 380.0f : FVector::UpVector * 380.0f;
	const int32 ActiveCargoCount = CargoStackComponent ? CargoStackComponent->GetCurrentCargoCount() : 0;
	const FString DebugText = FString::Printf(
		TEXT("CargoDelivery Time:%.1f Difficulty:%.2f Next:%.1f Active:%d Pending:%d"),
		RunElapsedTime,
		GetDifficultyAlpha(),
		FMath::Max(0.0f, NextDropTime - RunElapsedTime),
		ActiveCargoCount,
		CargoStackComponent && CargoStackComponent->HasPendingCargoDrop() ? 1 : 0);
	DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, FColor::Green, 0.0f, true);
#endif
}
