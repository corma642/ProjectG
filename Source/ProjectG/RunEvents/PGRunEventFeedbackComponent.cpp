// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunEvents/PGRunEventFeedbackComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RunEvents/PGRunEventSubsystem.h"
#include "Settings/PGGameSettings.h"
#include "Sound/SoundBase.h"
#include "UI/PGRunEventFeedbackWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunEventFeedback, Log, All);

UPGRunEventFeedbackComponent::UPGRunEventFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGRunEventFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolvePresentationSet();
	BindRunEventSubsystem();
}

void UPGRunEventFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindRunEventSubsystem();
	CleanupAllPresentations();

	Super::EndPlay(EndPlayReason);
}

void UPGRunEventFeedbackComponent::HandleRunEventPreviewStarted(EPGRunEventType EventType, float TimeUntilTrigger, float Direction)
{
	CleanupPreviewPresentation();

	const FPGRunEventPresentationEntry* Entry = FindPresentationEntry(EventType);
	if (!Entry)
	{
		UE_LOG(LogPGRunEventFeedback, Warning, TEXT("No presentation entry for run event preview. EventType=%d"), static_cast<int32>(EventType));
		return;
	}

	if (!ArePresentationResourcesReady())
	{
		return;
	}

	PreviewEventType = EventType;
	PlayPreviewPresentation(*Entry, EventType, TimeUntilTrigger, Direction);
}

void UPGRunEventFeedbackComponent::HandleRunEventTriggered(EPGRunEventType EventType, float Duration, float Direction)
{
	CleanupPreviewPresentation();
	CleanupActivePresentation();

	const FPGRunEventPresentationEntry* Entry = FindPresentationEntry(EventType);
	if (!Entry)
	{
		UE_LOG(LogPGRunEventFeedback, Warning, TEXT("No presentation entry for triggered run event. EventType=%d"), static_cast<int32>(EventType));
		return;
	}

	if (!ArePresentationResourcesReady())
	{
		return;
	}

	ActiveEventType = EventType;
	PlayActivePresentation(*Entry, EventType, Duration, Direction);
}

void UPGRunEventFeedbackComponent::HandleRunEventFinished(EPGRunEventType EventType)
{
	if (ActiveEventType == EventType)
	{
		CleanupActivePresentation();
	}
}

void UPGRunEventFeedbackComponent::HandleRunEventCanceled(EPGRunEventType EventType)
{
	if (PreviewEventType == EventType || ActiveEventType == EventType)
	{
		CleanupAllPresentations();
		return;
	}

	if (EventType == EPGRunEventType::None)
	{
		CleanupAllPresentations();
	}
}

void UPGRunEventFeedbackComponent::ResolvePresentationSet()
{
	PresentationSet = nullptr;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	if (Settings->RunEvent.DefaultPresentationSet.IsNull())
	{
		return;
	}

	PresentationSet = Settings ? Settings->RunEvent.DefaultPresentationSet.Get() : nullptr;

	if (!PresentationSet)
	{
		return;
	}

	ValidatePresentationSet();
}

bool UPGRunEventFeedbackComponent::ArePresentationResourcesReady() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	return !PreloadSubsystem || PreloadSubsystem->IsReady();
}

void UPGRunEventFeedbackComponent::ValidatePresentationSet() const
{
	if (!PresentationSet)
	{
		return;
	}

	if (PresentationSet->Entries.IsEmpty())
	{
		UE_LOG(LogPGRunEventFeedback, Warning, TEXT("DefaultPresentationSet has no presentation entries. Run event presentation will be skipped."));
		return;
	}

	for (const FPGRunEventPresentationEntry& Entry : PresentationSet->Entries)
	{
		if (Entry.EventType == EPGRunEventType::None)
		{
			UE_LOG(LogPGRunEventFeedback, Warning, TEXT("DefaultPresentationSet contains an entry with EventType=None."));
		}

		const bool bHasAnyResource =
			!Entry.PreviewWidgetClass.IsNull() ||
			!Entry.ActiveWidgetClass.IsNull() ||
			!Entry.PreviewNiagaraSystem.IsNull() ||
			!Entry.ActiveNiagaraSystem.IsNull() ||
			!Entry.PreviewSound.IsNull() ||
			!Entry.TriggerSound.IsNull() ||
			!Entry.ActiveLoopSound.IsNull();

		if (!bHasAnyResource)
		{
			UE_LOG(LogPGRunEventFeedback, Warning, TEXT("DefaultPresentationSet entry has no presentation resources. EventType=%d"), static_cast<int32>(Entry.EventType));
		}
	}
}

void UPGRunEventFeedbackComponent::BindRunEventSubsystem()
{
	if (!GetWorld())
	{
		return;
	}

	BoundRunEventSubsystem = GetWorld()->GetSubsystem<UPGRunEventSubsystem>();
	if (!BoundRunEventSubsystem)
	{
		UE_LOG(LogPGRunEventFeedback, Warning, TEXT("RunEventSubsystem is unavailable. Run event presentation will not be automatic."));
		return;
	}

	BoundRunEventSubsystem->OnRunEventPreviewStarted.AddDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventPreviewStarted);
	BoundRunEventSubsystem->OnRunEventTriggered.AddDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventTriggered);
	BoundRunEventSubsystem->OnRunEventFinished.AddDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventFinished);
	BoundRunEventSubsystem->OnRunEventCanceled.AddDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventCanceled);
}

void UPGRunEventFeedbackComponent::UnbindRunEventSubsystem()
{
	if (!BoundRunEventSubsystem)
	{
		return;
	}

	BoundRunEventSubsystem->OnRunEventPreviewStarted.RemoveDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventPreviewStarted);
	BoundRunEventSubsystem->OnRunEventTriggered.RemoveDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventTriggered);
	BoundRunEventSubsystem->OnRunEventFinished.RemoveDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventFinished);
	BoundRunEventSubsystem->OnRunEventCanceled.RemoveDynamic(this, &UPGRunEventFeedbackComponent::HandleRunEventCanceled);
	BoundRunEventSubsystem = nullptr;
}

const FPGRunEventPresentationEntry* UPGRunEventFeedbackComponent::FindPresentationEntry(EPGRunEventType EventType)
{
	if (!PresentationSet)
	{
		ResolvePresentationSet();
	}

	if (!PresentationSet)
	{
		UE_LOG(LogPGRunEventFeedback, Warning, TEXT("DefaultPresentationSet is not assigned or could not be preloaded. EventType=%d"), static_cast<int32>(EventType));
		return nullptr;
	}

	return PresentationSet->Entries.FindByPredicate([EventType](const FPGRunEventPresentationEntry& Entry)
	{
		return Entry.EventType == EventType;
	});
}

void UPGRunEventFeedbackComponent::PlayPreviewPresentation(const FPGRunEventPresentationEntry& Entry, EPGRunEventType EventType, float TimeUntilTrigger, float Direction)
{
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		if (TSubclassOf<UUserWidget> WidgetClass = Entry.PreviewWidgetClass.Get())
		{
			PreviewWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
			if (PreviewWidget)
			{
				if (UPGRunEventFeedbackWidget* FeedbackWidget = Cast<UPGRunEventFeedbackWidget>(PreviewWidget))
				{
					FeedbackWidget->ConfigureRunEventFeedback(EventType, EPGRunEventState::Preview, TimeUntilTrigger, 0.0f, Direction);
				}

				PreviewWidget->AddToViewport();
			}
		}

		if (USoundBase* PreviewSound = Entry.PreviewSound.Get())
		{
			UGameplayStatics::PlaySound2D(PlayerController, PreviewSound);
		}
	}

	if (UNiagaraSystem* PreviewSystem = Entry.PreviewNiagaraSystem.Get())
	{
		if (USceneComponent* AttachComponent = GetVfxAttachComponent())
		{
			PreviewNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				PreviewSystem,
				AttachComponent,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				true);
			if (PreviewNiagaraComponent)
			{
				PreviewNiagaraComponent->SetVariableFloat(TEXT("User.EventDirection"), Direction);
				PreviewNiagaraComponent->SetVariableFloat(TEXT("User.TimeUntilTrigger"), TimeUntilTrigger);
			}
		}
	}
}

void UPGRunEventFeedbackComponent::PlayActivePresentation(const FPGRunEventPresentationEntry& Entry, EPGRunEventType EventType, float Duration, float Direction)
{
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		if (TSubclassOf<UUserWidget> WidgetClass = Entry.ActiveWidgetClass.Get())
		{
			ActiveWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
			if (ActiveWidget)
			{
				if (UPGRunEventFeedbackWidget* FeedbackWidget = Cast<UPGRunEventFeedbackWidget>(ActiveWidget))
				{
					FeedbackWidget->ConfigureRunEventFeedback(EventType, EPGRunEventState::Triggered, 0.0f, Duration, Direction);
				}

				ActiveWidget->AddToViewport();
			}
		}

		if (USoundBase* TriggerSound = Entry.TriggerSound.Get())
		{
			UGameplayStatics::PlaySound2D(PlayerController, TriggerSound);
		}

		if (USoundBase* ActiveLoopSound = Entry.ActiveLoopSound.Get())
		{
			ActiveLoopAudioComponent = UGameplayStatics::SpawnSound2D(PlayerController, ActiveLoopSound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		}
	}

	if (UNiagaraSystem* ActiveSystem = Entry.ActiveNiagaraSystem.Get())
	{
		if (USceneComponent* AttachComponent = GetVfxAttachComponent())
		{
			ActiveNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				ActiveSystem,
				AttachComponent,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				true);
			if (ActiveNiagaraComponent)
			{
				ActiveNiagaraComponent->SetVariableFloat(TEXT("User.EventDirection"), Direction);
				ActiveNiagaraComponent->SetVariableFloat(TEXT("User.Duration"), Duration);
			}
		}
	}
}

void UPGRunEventFeedbackComponent::CleanupPreviewPresentation()
{
	if (PreviewWidget)
	{
		PreviewWidget->RemoveFromParent();
		PreviewWidget = nullptr;
	}

	if (PreviewNiagaraComponent)
	{
		PreviewNiagaraComponent->Deactivate();
		PreviewNiagaraComponent->DestroyComponent();
		PreviewNiagaraComponent = nullptr;
	}

	PreviewEventType = EPGRunEventType::None;
}

void UPGRunEventFeedbackComponent::CleanupActivePresentation()
{
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}

	if (ActiveNiagaraComponent)
	{
		ActiveNiagaraComponent->Deactivate();
		ActiveNiagaraComponent->DestroyComponent();
		ActiveNiagaraComponent = nullptr;
	}

	if (ActiveLoopAudioComponent)
	{
		ActiveLoopAudioComponent->Stop();
		ActiveLoopAudioComponent->DestroyComponent();
		ActiveLoopAudioComponent = nullptr;
	}

	ActiveEventType = EPGRunEventType::None;
}

void UPGRunEventFeedbackComponent::CleanupAllPresentations()
{
	CleanupPreviewPresentation();
	CleanupActivePresentation();
}

APlayerController* UPGRunEventFeedbackComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

USceneComponent* UPGRunEventFeedbackComponent::GetVfxAttachComponent() const
{
	const APlayerController* PlayerController = GetOwningPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	return Pawn ? Pawn->GetRootComponent() : nullptr;
}
