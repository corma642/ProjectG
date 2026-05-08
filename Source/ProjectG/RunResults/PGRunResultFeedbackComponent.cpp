// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunResults/PGRunResultFeedbackComponent.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/PGRunnerCharacter.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Settings/PGGameSettings.h"
#include "Sound/SoundBase.h"
#include "UI/PGRunResultWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunResultFeedback, Log, All);

UPGRunResultFeedbackComponent::UPGRunResultFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGRunResultFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	StartPreloadingResources();
	BindGameMode();
}

void UPGRunResultFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameMode();
	CleanupResultWidget();
	ReleaseResourceLoadHandle();

	Super::EndPlay(EndPlayReason);
}

void UPGRunResultFeedbackComponent::HandleRunFailed(FName FailureReason)
{
	CleanupResultWidget();

	if (!bResourcesReady)
	{
		UE_LOG(LogPGRunResultFeedback, Warning, TEXT("Run result resources are still loading. Result presentation will be skipped. FailureReason=%s"), *FailureReason.ToString());
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	const APGRunnerCharacter* RunnerCharacter = GetRunnerCharacter();
	const float RunTimeSeconds = RunnerCharacter ? RunnerCharacter->GetRunElapsedTime() : 0.0f;
	const float DistanceMeters = RunnerCharacter ? RunnerCharacter->GetRunDistanceMeters() : 0.0f;
	const int32 Score = RunnerCharacter ? RunnerCharacter->GetRunScore() : 0;
	const int32 Receipts = RunnerCharacter ? RunnerCharacter->GetRunReceipts() : 0;
	const int32 OrdersDelivered = RunnerCharacter ? RunnerCharacter->GetDeliveredCargoCount() : 0;

	TSubclassOf<UUserWidget> WidgetClass = Settings->RunResult.GameOverWidgetClass.Get();
	if (!WidgetClass)
	{
		WidgetClass = UPGRunResultWidget::StaticClass();
	}

	if (WidgetClass)
	{
		ResultWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
		if (ResultWidget)
		{
			if (UPGRunResultWidget* RunResultWidget = Cast<UPGRunResultWidget>(ResultWidget))
			{
				RunResultWidget->ConfigureRunResult(FailureReason, RunTimeSeconds, DistanceMeters, Score, Receipts, OrdersDelivered);
			}

			ResultWidget->AddToViewport(100);
			PlayerController->SetInputMode(FInputModeUIOnly());
			PlayerController->SetShowMouseCursor(true);
		}
	}

	if (USoundBase* GameOverSound = Settings->RunResult.GameOverSound.Get())
	{
		UGameplayStatics::PlaySound2D(PlayerController, GameOverSound);
	}

	if (UNiagaraSystem* GameOverVFX = Settings->RunResult.GameOverVFX.Get())
	{
		const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
		const FVector VFXLocation = RunnerCharacter ? RunnerCharacter->GetActorLocation() : (CameraManager ? CameraManager->GetCameraLocation() : FVector::ZeroVector);
		const FRotator VFXRotation = RunnerCharacter ? RunnerCharacter->GetActorRotation() : (CameraManager ? CameraManager->GetCameraRotation() : FRotator::ZeroRotator);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(PlayerController, GameOverVFX, VFXLocation, VFXRotation);
	}

	if (TSubclassOf<UCameraShakeBase> CameraShakeClass = Settings->RunResult.GameOverCameraShake.Get())
	{
		PlayerController->ClientStartCameraShake(CameraShakeClass);
	}
}

void UPGRunResultFeedbackComponent::StartPreloadingResources()
{
	bResourcesReady = false;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		bResourcesReady = true;
		return;
	}

	TArray<FSoftObjectPath> ResourcePaths;
	if (!Settings->RunResult.GameOverWidgetClass.IsNull())
	{
		ResourcePaths.Add(Settings->RunResult.GameOverWidgetClass.ToSoftObjectPath());
	}
	if (!Settings->RunResult.GameOverSound.IsNull())
	{
		ResourcePaths.Add(Settings->RunResult.GameOverSound.ToSoftObjectPath());
	}
	if (!Settings->RunResult.GameOverVFX.IsNull())
	{
		ResourcePaths.Add(Settings->RunResult.GameOverVFX.ToSoftObjectPath());
	}
	if (!Settings->RunResult.GameOverCameraShake.IsNull())
	{
		ResourcePaths.Add(Settings->RunResult.GameOverCameraShake.ToSoftObjectPath());
	}

	if (ResourcePaths.IsEmpty())
	{
		bResourcesReady = true;
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	ResourceLoadHandle = StreamableManager.RequestAsyncLoad(
		ResourcePaths,
		FStreamableDelegate::CreateUObject(this, &UPGRunResultFeedbackComponent::HandleResourcesLoaded),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("PGRunResultResources"));
}

void UPGRunResultFeedbackComponent::HandleResourcesLoaded()
{
	bResourcesReady = true;

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	if (!Settings)
	{
		return;
	}

	if (!Settings->RunResult.GameOverWidgetClass.IsNull() && !Settings->RunResult.GameOverWidgetClass.Get())
	{
		UE_LOG(LogPGRunResultFeedback, Warning, TEXT("RunResult.GameOverWidgetClass could not be preloaded."));
	}
	if (!Settings->RunResult.GameOverSound.IsNull() && !Settings->RunResult.GameOverSound.Get())
	{
		UE_LOG(LogPGRunResultFeedback, Warning, TEXT("RunResult.GameOverSound could not be preloaded."));
	}
	if (!Settings->RunResult.GameOverVFX.IsNull() && !Settings->RunResult.GameOverVFX.Get())
	{
		UE_LOG(LogPGRunResultFeedback, Warning, TEXT("RunResult.GameOverVFX could not be preloaded."));
	}
	if (!Settings->RunResult.GameOverCameraShake.IsNull() && !Settings->RunResult.GameOverCameraShake.Get())
	{
		UE_LOG(LogPGRunResultFeedback, Warning, TEXT("RunResult.GameOverCameraShake could not be preloaded."));
	}
}

void UPGRunResultFeedbackComponent::ReleaseResourceLoadHandle()
{
	if (ResourceLoadHandle.IsValid())
	{
		ResourceLoadHandle->ReleaseHandle();
		ResourceLoadHandle.Reset();
	}

	bResourcesReady = false;
}

void UPGRunResultFeedbackComponent::BindGameMode()
{
	if (bIsBoundToGameMode)
	{
		return;
	}

	UWorld* World = GetWorld();
	BoundGameMode = World ? World->GetAuthGameMode<APGGameModeBase>() : nullptr;
	if (BoundGameMode)
	{
		BoundGameMode->OnRunFailed.AddDynamic(this, &UPGRunResultFeedbackComponent::HandleRunFailed);
		bIsBoundToGameMode = true;
	}
}

void UPGRunResultFeedbackComponent::UnbindGameMode()
{
	if (BoundGameMode && bIsBoundToGameMode)
	{
		BoundGameMode->OnRunFailed.RemoveDynamic(this, &UPGRunResultFeedbackComponent::HandleRunFailed);
	}

	BoundGameMode = nullptr;
	bIsBoundToGameMode = false;
}

void UPGRunResultFeedbackComponent::CleanupResultWidget()
{
	if (ResultWidget)
	{
		ResultWidget->RemoveFromParent();
		ResultWidget = nullptr;
	}
}

APlayerController* UPGRunResultFeedbackComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

APGRunnerCharacter* UPGRunResultFeedbackComponent::GetRunnerCharacter() const
{
	const APlayerController* PlayerController = GetOwningPlayerController();
	return PlayerController ? Cast<APGRunnerCharacter>(PlayerController->GetPawn()) : nullptr;
}
