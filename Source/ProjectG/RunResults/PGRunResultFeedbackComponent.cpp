// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunResults/PGRunResultFeedbackComponent.h"

#include "Ads/PGAdsSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Cargo/PGCargoStackComponent.h"
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
#include "TimerManager.h"
#include "UI/PGRunResultWidget.h"

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
	RestoreTimeDilation();
	CleanupResultWidget();

	Super::EndPlay(EndPlayReason);
}

void UPGRunResultFeedbackComponent::HandleRunFailed(FName FailureReason)
{
	CleanupResultWidget();
	RestoreTimeDilation();

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

	const FPGRunResultSettings& ResultSettings = Settings->RunResult;

	// 충돌 체감(사운드·카메라 쉐이크)은 슬로모션과 무관하게 실패 순간에 바로 재생한다.
	// 러너가 사라지는 VFX는 슬로모션이 끝나 메시를 숨기는 ShowResultWidget에서 재생한다.
	if (USoundBase* GameOverSound = ResultSettings.GameOverSound.Get())
	{
		UGameplayStatics::PlaySound2D(PlayerController, GameOverSound, UPGGameUserSettings::GetCurrentSFXVolumeScale());
	}

	if (TSubclassOf<UCameraShakeBase> CameraShakeClass = ResultSettings.GameOverCameraShake.Get())
	{
		PlayerController->ClientStartCameraShake(CameraShakeClass);
	}

	UWorld* World = GetWorld();
	const bool bUseSlowMotion = World
		&& ResultSettings.FailureSlowMotionDuration > KINDA_SMALL_NUMBER
		&& ResultSettings.FailureSlowMotionTimeDilation < 1.0f - KINDA_SMALL_NUMBER;
	if (!bUseSlowMotion)
	{
		ShowResultWidget(FailureReason);
		return;
	}

	// 적재물이 쏟아지는 실패 원인을 플레이어가 볼 수 있게 잠시 느리게 보여준 뒤 결과 화면을 띄운다.
	PendingFailureReason = FailureReason;
	bIsSlowMotionActive = true;
	UGameplayStatics::SetGlobalTimeDilation(World, ResultSettings.FailureSlowMotionTimeDilation);

	// 월드 타이머는 느려진 게임 시간으로 흐르므로 실제 시간(Duration)에 배율을 곱해 예약한다.
	World->GetTimerManager().SetTimer(
		SlowMotionTimerHandle,
		this,
		&UPGRunResultFeedbackComponent::HandleSlowMotionFinished,
		ResultSettings.FailureSlowMotionDuration * ResultSettings.FailureSlowMotionTimeDilation,
		false);
}

void UPGRunResultFeedbackComponent::HandleRunContinueCountdownStarted(float Duration)
{
	CleanupResultWidget();
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
	}
}

void UPGRunResultFeedbackComponent::HandleSlowMotionFinished()
{
	RestoreTimeDilation();
	ShowResultWidget(PendingFailureReason);
}

void UPGRunResultFeedbackComponent::ShowResultWidget(FName FailureReason)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	TSubclassOf<UPGRunResultWidget> WidgetClass = Settings ? Settings->RunResult.GameOverWidgetClass.Get() : nullptr;
	if (!PlayerController || !WidgetClass)
	{
		return;
	}

	// 이어하기를 살 수 있거나 광고로 이어할 수 있으면 러너와 바구니를 그대로 두고 쏟아진 적재물만 정리한다. 재개하면 이 자리에서 다시 달린다.
	// 아니면 슬로모션 동안 보여 준 러너와 바구니를 결과 화면과 함께 치우고, 사라지는 VFX를 이 순간 러너 위치에 재생한다.
	APGRunnerCharacter* RunnerCharacter = GetRunnerCharacter();
	const UGameInstance* GameInstance = PlayerController->GetGameInstance();
	const UPGAdsSubsystem* Ads = GameInstance ? GameInstance->GetSubsystem<UPGAdsSubsystem>() : nullptr;
	const bool bAdsAvailable = Ads && Ads->IsAdsAvailable();
	const bool bCanContinue = BoundGameMode && (BoundGameMode->CanContinueRun() || (bAdsAvailable && BoundGameMode->IsContinueOffered()));
	if (bCanContinue && RunnerCharacter && RunnerCharacter->GetCargoStackComponent())
	{
		RunnerCharacter->GetCargoStackComponent()->FreezeCargoForContinue();
	}
	else
	{
		if (UNiagaraSystem* GameOverVFX = Settings->RunResult.GameOverVFX.Get())
		{
			const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
			const FVector VFXLocation = RunnerCharacter ? RunnerCharacter->GetActorLocation() : (CameraManager ? CameraManager->GetCameraLocation() : FVector::ZeroVector);
			const FRotator VFXRotation = RunnerCharacter ? RunnerCharacter->GetActorRotation() : (CameraManager ? CameraManager->GetCameraRotation() : FRotator::ZeroRotator);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(PlayerController, GameOverVFX, VFXLocation, VFXRotation);
		}
		if (RunnerCharacter)
		{
			RunnerCharacter->HideForRunResult();
		}
	}

	// 러너 통계는 실패 시 StopRunProgress로 고정되므로 슬로모션 뒤에 읽어도 실패 순간 값과 같다.
	const float RunTimeSeconds = RunnerCharacter ? RunnerCharacter->GetRunElapsedTime() : 0.0f;
	const float DistanceMeters = RunnerCharacter ? RunnerCharacter->GetRunDistanceMeters() : 0.0f;
	const int32 Score = RunnerCharacter ? RunnerCharacter->GetRunScore() : 0;
	const int32 Receipts = RunnerCharacter ? RunnerCharacter->GetRunReceipts() : 0;
	const int32 OrdersDelivered = RunnerCharacter ? RunnerCharacter->GetDeliveredCargoCount() : 0;

	UPGRunResultWidget* RunResultWidget = CreateWidget<UPGRunResultWidget>(PlayerController, WidgetClass);
	if (!RunResultWidget)
	{
		return;
	}

	ResultWidget = RunResultWidget;
	RunResultWidget->AddToViewport(100);
	RunResultWidget->ConfigureRunResult(FailureReason, RunTimeSeconds, DistanceMeters, Score, Receipts, OrdersDelivered);
	RunResultWidget->ConfigureContinueOption(BoundGameMode && BoundGameMode->IsContinueOffered(), BoundGameMode && BoundGameMode->CanContinueRun(), BoundGameMode ? BoundGameMode->GetContinueReceiptCost() : 0);
	RunResultWidget->ConfigureAdOptions(bAdsAvailable, Ads && Ads->IsRewardedAdReady(), BoundGameMode && BoundGameMode->CanDoubleRunReceipts());
	PlayerController->SetInputMode(FInputModeUIOnly());
	PlayerController->SetShowMouseCursor(true);
}

void UPGRunResultFeedbackComponent::RestoreTimeDilation()
{
	if (!bIsSlowMotionActive)
	{
		return;
	}

	bIsSlowMotionActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlowMotionTimerHandle);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
}

bool UPGRunResultFeedbackComponent::AreRuntimeResourcesReady() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	return !PreloadSubsystem || PreloadSubsystem->IsGroupReady(EPGRuntimePreloadGroup::Run);
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
		BoundGameMode->OnRunContinueCountdownStarted.AddDynamic(this, &UPGRunResultFeedbackComponent::HandleRunContinueCountdownStarted);
		bIsBoundToGameMode = true;
	}
}

void UPGRunResultFeedbackComponent::UnbindGameMode()
{
	if (BoundGameMode && bIsBoundToGameMode)
	{
		BoundGameMode->OnRunFailed.RemoveDynamic(this, &UPGRunResultFeedbackComponent::HandleRunFailed);
		BoundGameMode->OnRunContinueCountdownStarted.RemoveDynamic(this, &UPGRunResultFeedbackComponent::HandleRunContinueCountdownStarted);
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

bool UPGRunResultFeedbackComponent::TryHandleBack()
{
	UPGRunResultWidget* RunResultWidget = Cast<UPGRunResultWidget>(ResultWidget);
	return RunResultWidget && RunResultWidget->RequestHome();
}
