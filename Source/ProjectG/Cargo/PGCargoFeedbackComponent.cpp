// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoFeedbackComponent.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraShakeBase.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Character/PGRunnerCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Settings/PGGameSettings.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UI/PGCargoFeedbackWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGCargoFeedback, Log, All);

UPGCargoFeedbackComponent::UPGCargoFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGCargoFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	StartLoadingPresentationSet();

	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		HandleRunnerPossessed(Cast<APGRunnerCharacter>(PlayerController->GetPawn()));
	}
}

void UPGCargoFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HandleRunnerUnpossessed();
	ReleasePresentationLoadHandles();

	Super::EndPlay(EndPlayReason);
}

void UPGCargoFeedbackComponent::HandleRunnerPossessed(APGRunnerCharacter* RunnerCharacter)
{
	BindCargoStack(RunnerCharacter ? RunnerCharacter->GetCargoStackComponent() : nullptr);
}

void UPGCargoFeedbackComponent::HandleRunnerUnpossessed()
{
	UnbindCargoStack();
	CleanupAllPresentations();
}

void UPGCargoFeedbackComponent::HandleCargoDropHoldStarted()
{
	CleanupPersistentPresentation();

	const FPGCargoPresentationEntry* Entry = FindPresentationEntry(EPGCargoFeedbackType::CargoDropHoldStarted);
	if (!Entry)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("No presentation entry for cargo drop hold start."));
		return;
	}

	if (!bPresentationResourcesReady)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo presentation resources are not ready. Drop hold start presentation skipped."));
		return;
	}

	const float HoldDuration = BoundCargoStackComponent ? BoundCargoStackComponent->GetCargoDropHoldRemainingTime() : 0.0f;
	PlayPersistentPresentation(*Entry, EPGCargoFeedbackType::CargoDropHoldStarted, INDEX_NONE, HoldDuration);
}

void UPGCargoFeedbackComponent::HandleCargoDropHoldFinished()
{
	CleanupPersistentPresentation();

	const FPGCargoPresentationEntry* Entry = FindPresentationEntry(EPGCargoFeedbackType::CargoDropHoldFinished);
	if (!Entry)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("No presentation entry for cargo drop hold finish."));
		return;
	}

	if (!bPresentationResourcesReady)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo presentation resources are not ready. Drop hold finish presentation skipped."));
		return;
	}

	PlayTransientPresentation(*Entry, EPGCargoFeedbackType::CargoDropHoldFinished, INDEX_NONE);
}

void UPGCargoFeedbackComponent::HandleCargoLost(int32 CargoIndex)
{
	CleanupAllPresentations();

	const FPGCargoPresentationEntry* Entry = FindPresentationEntry(EPGCargoFeedbackType::CargoLost);
	if (!Entry)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("No presentation entry for cargo lost. CargoIndex=%d"), CargoIndex);
		return;
	}

	if (!bPresentationResourcesReady)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo presentation resources are not ready. Cargo lost presentation skipped. CargoIndex=%d"), CargoIndex);
		return;
	}

	PlayTransientPresentation(*Entry, EPGCargoFeedbackType::CargoLost, CargoIndex);
}

void UPGCargoFeedbackComponent::HandleCargoDelivered(int32 CargoInstanceId, int32 Score)
{
	const FPGCargoPresentationEntry* Entry = FindPresentationEntry(EPGCargoFeedbackType::CargoDelivered);
	if (!Entry)
	{
		return;
	}

	if (!bPresentationResourcesReady)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo presentation resources are not ready. Cargo delivered presentation skipped. CargoInstanceId=%d"), CargoInstanceId);
		return;
	}

	PlayTransientPresentation(*Entry, EPGCargoFeedbackType::CargoDelivered, CargoInstanceId);
}

void UPGCargoFeedbackComponent::StartLoadingPresentationSet()
{
	bPresentationSetLoaded = false;
	bPresentationResourcesReady = false;
	PresentationSet = nullptr;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		bPresentationSetLoaded = true;
		bPresentationResourcesReady = true;
		return;
	}

	if (Settings->CargoFeedback.DefaultPresentationSet.IsNull())
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("CargoFeedback.DefaultPresentationSet is not assigned. Cargo presentation will be skipped."));
		bPresentationSetLoaded = true;
		bPresentationResourcesReady = true;
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	PresentationSetLoadHandle = StreamableManager.RequestAsyncLoad(
		Settings->CargoFeedback.DefaultPresentationSet.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UPGCargoFeedbackComponent::HandlePresentationSetLoaded),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("PGCargoPresentationSet"));
}

void UPGCargoFeedbackComponent::HandlePresentationSetLoaded()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	PresentationSet = Settings ? Settings->CargoFeedback.DefaultPresentationSet.Get() : nullptr;
	bPresentationSetLoaded = true;

	if (!PresentationSet)
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("CargoFeedback.DefaultPresentationSet could not be loaded. Cargo presentation will be skipped."));
		bPresentationResourcesReady = true;
		return;
	}

	ValidatePresentationSet();
	StartPreloadingPresentationResources();
}

void UPGCargoFeedbackComponent::StartPreloadingPresentationResources()
{
	if (!PresentationSet || PresentationSet->Entries.IsEmpty())
	{
		bPresentationResourcesReady = true;
		return;
	}

	TSet<FSoftObjectPath> UniqueResourcePaths;
	for (const FPGCargoPresentationEntry& Entry : PresentationSet->Entries)
	{
		if (!Entry.WidgetClass.IsNull())
		{
			UniqueResourcePaths.Add(Entry.WidgetClass.ToSoftObjectPath());
		}
		if (!Entry.NiagaraSystem.IsNull())
		{
			UniqueResourcePaths.Add(Entry.NiagaraSystem.ToSoftObjectPath());
		}
		if (!Entry.Sound.IsNull())
		{
			UniqueResourcePaths.Add(Entry.Sound.ToSoftObjectPath());
		}
		if (!Entry.LoopSound.IsNull())
		{
			UniqueResourcePaths.Add(Entry.LoopSound.ToSoftObjectPath());
		}
		if (!Entry.CameraShakeClass.IsNull())
		{
			UniqueResourcePaths.Add(Entry.CameraShakeClass.ToSoftObjectPath());
		}
	}

	if (UniqueResourcePaths.IsEmpty())
	{
		bPresentationResourcesReady = true;
		return;
	}

	TArray<FSoftObjectPath> ResourcePaths = UniqueResourcePaths.Array();
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	PresentationResourcesLoadHandle = StreamableManager.RequestAsyncLoad(
		ResourcePaths,
		FStreamableDelegate::CreateUObject(this, &UPGCargoFeedbackComponent::HandlePresentationResourcesLoaded),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("PGCargoPresentationResources"));
}

void UPGCargoFeedbackComponent::HandlePresentationResourcesLoaded()
{
	bPresentationResourcesReady = true;

	if (!PresentationSet)
	{
		return;
	}

	for (const FPGCargoPresentationEntry& Entry : PresentationSet->Entries)
	{
		if (!Entry.WidgetClass.IsNull() && !Entry.WidgetClass.Get())
		{
			UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo WidgetClass could not be preloaded. FeedbackType=%d"), static_cast<int32>(Entry.FeedbackType));
		}
		if (!Entry.NiagaraSystem.IsNull() && !Entry.NiagaraSystem.Get())
		{
			UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo NiagaraSystem could not be preloaded. FeedbackType=%d"), static_cast<int32>(Entry.FeedbackType));
		}
		if (!Entry.Sound.IsNull() && !Entry.Sound.Get())
		{
			UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo Sound could not be preloaded. FeedbackType=%d"), static_cast<int32>(Entry.FeedbackType));
		}
		if (!Entry.LoopSound.IsNull() && !Entry.LoopSound.Get())
		{
			UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo LoopSound could not be preloaded. FeedbackType=%d"), static_cast<int32>(Entry.FeedbackType));
		}
		if (!Entry.CameraShakeClass.IsNull() && !Entry.CameraShakeClass.Get())
		{
			UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo CameraShakeClass could not be preloaded. FeedbackType=%d"), static_cast<int32>(Entry.FeedbackType));
		}
	}
}

void UPGCargoFeedbackComponent::ValidatePresentationSet() const
{
	if (!PresentationSet)
	{
		return;
	}

	if (PresentationSet->Entries.IsEmpty())
	{
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("CargoFeedback.DefaultPresentationSet has no presentation entries. Cargo presentation will be skipped."));
		return;
	}

	for (const FPGCargoPresentationEntry& Entry : PresentationSet->Entries)
	{
		if (Entry.FeedbackType == EPGCargoFeedbackType::None)
		{
			UE_LOG(LogPGCargoFeedback, Warning, TEXT("CargoFeedback.DefaultPresentationSet contains an entry with FeedbackType=None."));
		}

		const bool bHasAnyResource =
			!Entry.WidgetClass.IsNull() ||
			!Entry.NiagaraSystem.IsNull() ||
			!Entry.Sound.IsNull() ||
			!Entry.LoopSound.IsNull() ||
			!Entry.CameraShakeClass.IsNull();

		if (!bHasAnyResource)
		{
			UE_LOG(LogPGCargoFeedback, Warning, TEXT("Cargo presentation entry has no resources. FeedbackType=%d"), static_cast<int32>(Entry.FeedbackType));
		}
	}
}

void UPGCargoFeedbackComponent::ReleasePresentationLoadHandles()
{
	if (PresentationResourcesLoadHandle.IsValid())
	{
		PresentationResourcesLoadHandle->ReleaseHandle();
		PresentationResourcesLoadHandle.Reset();
	}

	if (PresentationSetLoadHandle.IsValid())
	{
		PresentationSetLoadHandle->ReleaseHandle();
		PresentationSetLoadHandle.Reset();
	}

	bPresentationSetLoaded = false;
	bPresentationResourcesReady = false;
}

void UPGCargoFeedbackComponent::BindCargoStack(UPGCargoStackComponent* CargoStackComponent)
{
	if (BoundCargoStackComponent == CargoStackComponent)
	{
		return;
	}

	UnbindCargoStack();
	BoundCargoStackComponent = CargoStackComponent;

	if (!BoundCargoStackComponent)
	{
		return;
	}

	BoundCargoStackComponent->OnCargoDropHoldStarted.AddUObject(this, &UPGCargoFeedbackComponent::HandleCargoDropHoldStarted);
	BoundCargoStackComponent->OnCargoDropHoldFinished.AddUObject(this, &UPGCargoFeedbackComponent::HandleCargoDropHoldFinished);
	BoundCargoStackComponent->OnCargoDelivered.AddUObject(this, &UPGCargoFeedbackComponent::HandleCargoDelivered);
	BoundCargoStackComponent->OnCargoLost.AddUObject(this, &UPGCargoFeedbackComponent::HandleCargoLost);
}

void UPGCargoFeedbackComponent::UnbindCargoStack()
{
	if (!BoundCargoStackComponent)
	{
		return;
	}

	BoundCargoStackComponent->OnCargoDropHoldStarted.RemoveAll(this);
	BoundCargoStackComponent->OnCargoDropHoldFinished.RemoveAll(this);
	BoundCargoStackComponent->OnCargoDelivered.RemoveAll(this);
	BoundCargoStackComponent->OnCargoLost.RemoveAll(this);
	BoundCargoStackComponent = nullptr;
}

const FPGCargoPresentationEntry* UPGCargoFeedbackComponent::FindPresentationEntry(EPGCargoFeedbackType FeedbackType) const
{
	if (!PresentationSet)
	{
		const TCHAR* LoadStateText = bPresentationSetLoaded ? TEXT("not assigned or failed") : TEXT("still loading");
		UE_LOG(LogPGCargoFeedback, Warning, TEXT("CargoFeedback.DefaultPresentationSet is %s. FeedbackType=%d"), LoadStateText, static_cast<int32>(FeedbackType));
		return nullptr;
	}

	return PresentationSet->Entries.FindByPredicate([FeedbackType](const FPGCargoPresentationEntry& Entry)
	{
		return Entry.FeedbackType == FeedbackType;
	});
}

void UPGCargoFeedbackComponent::PlayPersistentPresentation(const FPGCargoPresentationEntry& Entry, EPGCargoFeedbackType FeedbackType, int32 CargoIndex, float DisplayDuration)
{
	PersistentFeedbackType = FeedbackType;

	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		if (TSubclassOf<UUserWidget> WidgetClass = Entry.WidgetClass.Get())
		{
			PersistentWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
			if (PersistentWidget)
			{
				if (UPGCargoFeedbackWidget* FeedbackWidget = Cast<UPGCargoFeedbackWidget>(PersistentWidget))
				{
					FeedbackWidget->ConfigureCargoFeedback(FeedbackType, CargoIndex, DisplayDuration);
				}
				PersistentWidget->AddToViewport();
			}
		}

		if (USoundBase* Sound = Entry.Sound.Get())
		{
			UGameplayStatics::PlaySound2D(PlayerController, Sound);
		}

		if (USoundBase* LoopSound = Entry.LoopSound.Get())
		{
			PersistentLoopAudioComponent = UGameplayStatics::SpawnSound2D(PlayerController, LoopSound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		}

		if (TSubclassOf<UCameraShakeBase> CameraShakeClass = Entry.CameraShakeClass.Get())
		{
			PlayerController->ClientStartCameraShake(CameraShakeClass);
		}
	}

	if (UNiagaraSystem* NiagaraSystem = Entry.NiagaraSystem.Get())
	{
		if (USceneComponent* AttachComponent = GetVfxAttachComponent())
		{
			PersistentNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				NiagaraSystem,
				AttachComponent,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				true);
			if (PersistentNiagaraComponent)
			{
				PersistentNiagaraComponent->SetVariableFloat(TEXT("User.DisplayDuration"), DisplayDuration);
			}
		}
	}
}

void UPGCargoFeedbackComponent::PlayTransientPresentation(const FPGCargoPresentationEntry& Entry, EPGCargoFeedbackType FeedbackType, int32 CargoIndex)
{
	CleanupTransientPresentation();

	const float DisplayDuration = FMath::Max(0.0f, Entry.DisplayDuration);

	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		if (TSubclassOf<UUserWidget> WidgetClass = Entry.WidgetClass.Get())
		{
			TransientWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
			if (TransientWidget)
			{
				if (UPGCargoFeedbackWidget* FeedbackWidget = Cast<UPGCargoFeedbackWidget>(TransientWidget))
				{
					FeedbackWidget->ConfigureCargoFeedback(FeedbackType, CargoIndex, DisplayDuration);
				}
				TransientWidget->AddToViewport();
			}
		}

		if (USoundBase* Sound = Entry.Sound.Get())
		{
			UGameplayStatics::PlaySound2D(PlayerController, Sound);
		}

		if (USoundBase* LoopSound = Entry.LoopSound.Get())
		{
			TransientLoopAudioComponent = UGameplayStatics::SpawnSound2D(PlayerController, LoopSound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		}

		if (TSubclassOf<UCameraShakeBase> CameraShakeClass = Entry.CameraShakeClass.Get())
		{
			PlayerController->ClientStartCameraShake(CameraShakeClass);
		}
	}

	if (UNiagaraSystem* NiagaraSystem = Entry.NiagaraSystem.Get())
	{
		if (USceneComponent* AttachComponent = GetVfxAttachComponent())
		{
			TransientNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				NiagaraSystem,
				AttachComponent,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				true);
			if (TransientNiagaraComponent)
			{
				TransientNiagaraComponent->SetVariableFloat(TEXT("User.DisplayDuration"), DisplayDuration);
			}
		}
	}

	if (DisplayDuration > KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(TransientCleanupTimerHandle, this, &UPGCargoFeedbackComponent::CleanupTransientPresentation, DisplayDuration, false);
		}
	}
}

void UPGCargoFeedbackComponent::CleanupPersistentPresentation()
{
	if (PersistentWidget)
	{
		PersistentWidget->RemoveFromParent();
		PersistentWidget = nullptr;
	}

	if (PersistentNiagaraComponent)
	{
		PersistentNiagaraComponent->Deactivate();
		PersistentNiagaraComponent->DestroyComponent();
		PersistentNiagaraComponent = nullptr;
	}

	if (PersistentLoopAudioComponent)
	{
		PersistentLoopAudioComponent->Stop();
		PersistentLoopAudioComponent->DestroyComponent();
		PersistentLoopAudioComponent = nullptr;
	}

	PersistentFeedbackType = EPGCargoFeedbackType::None;
}

void UPGCargoFeedbackComponent::CleanupTransientPresentation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransientCleanupTimerHandle);
	}

	if (TransientWidget)
	{
		TransientWidget->RemoveFromParent();
		TransientWidget = nullptr;
	}

	if (TransientNiagaraComponent)
	{
		TransientNiagaraComponent->Deactivate();
		TransientNiagaraComponent->DestroyComponent();
		TransientNiagaraComponent = nullptr;
	}

	if (TransientLoopAudioComponent)
	{
		TransientLoopAudioComponent->Stop();
		TransientLoopAudioComponent->DestroyComponent();
		TransientLoopAudioComponent = nullptr;
	}
}

void UPGCargoFeedbackComponent::CleanupAllPresentations()
{
	CleanupPersistentPresentation();
	CleanupTransientPresentation();
}

APlayerController* UPGCargoFeedbackComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

USceneComponent* UPGCargoFeedbackComponent::GetVfxAttachComponent() const
{
	const APlayerController* PlayerController = GetOwningPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	return Pawn ? Pawn->GetRootComponent() : nullptr;
}
