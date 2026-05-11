// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunResults/PGRunResultFeedbackComponent.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/PGRunnerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Settings/PGGameSettings.h"
#include "Settings/PGGameUserSettings.h"
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

	BindGameMode();
}

void UPGRunResultFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameMode();
	CleanupResultWidget();

	Super::EndPlay(EndPlayReason);
}

void UPGRunResultFeedbackComponent::HandleRunFailed(FName FailureReason)
{
	CleanupResultWidget();

	if (!AreRuntimeResourcesReady())
	{
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

	TSubclassOf<UPGRunResultWidget> WidgetClass = Settings->RunResult.GameOverWidgetClass.Get();
	if (WidgetClass)
	{
		ResultWidget = CreateWidget<UPGRunResultWidget>(PlayerController, WidgetClass);
		if (ResultWidget)
		{
			ResultWidget->AddToViewport(100);

			if (UPGRunResultWidget* RunResultWidget = Cast<UPGRunResultWidget>(ResultWidget))
			{
				RunResultWidget->ConfigureRunResult(FailureReason, RunTimeSeconds, DistanceMeters, Score, Receipts, OrdersDelivered);
			}

			PlayerController->SetInputMode(FInputModeUIOnly());
			PlayerController->SetShowMouseCursor(true);
		}
	}

	if (USoundBase* GameOverSound = Settings->RunResult.GameOverSound.Get())
	{
		UGameplayStatics::PlaySound2D(PlayerController, GameOverSound, UPGGameUserSettings::GetCurrentSFXVolumeScale());
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

bool UPGRunResultFeedbackComponent::AreRuntimeResourcesReady() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	return !PreloadSubsystem || PreloadSubsystem->IsReady();
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
