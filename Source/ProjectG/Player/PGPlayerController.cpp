// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/PGPlayerController.h"

#include "Character/PGRunnerCharacter.h"
#include "Audio/PGMusicComponent.h"
#include "Debug/PGFPSDisplayComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "GameModes/PGGameModeBase.h"
#include "Home/PGHomeFlowComponent.h"
#include "InputActionValue.h"
#include "RunEvents/PGRunEventFeedbackComponent.h"
#include "RunHUD/PGRunHUDComponent.h"
#include "Settings/PGGameSettings.h"
#include "Settings/PGGameUserSettings.h"
#include "RunResults/PGRunResultFeedbackComponent.h"

namespace
{
float SelectTiltAxisValue(const FVector& Tilt, EPGTiltInputAxis Axis)
{
	switch (Axis)
	{
	case EPGTiltInputAxis::X:
		return Tilt.X;
	case EPGTiltInputAxis::Y:
		return Tilt.Y;
	case EPGTiltInputAxis::Z:
		return Tilt.Z;
	default:
		return Tilt.X;
	}
}

}

APGPlayerController::APGPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	RunEventFeedbackComponent = CreateDefaultSubobject<UPGRunEventFeedbackComponent>(TEXT("RunEventFeedback"));
	RunResultFeedbackComponent = CreateDefaultSubobject<UPGRunResultFeedbackComponent>(TEXT("RunResultFeedback"));
	MusicComponent = CreateDefaultSubobject<UPGMusicComponent>(TEXT("Music"));
	HomeFlowComponent = CreateDefaultSubobject<UPGHomeFlowComponent>(TEXT("HomeFlow"));
	RunHUDComponent = CreateDefaultSubobject<UPGRunHUDComponent>(TEXT("RunHUD"));
	FPSDisplayComponent = CreateDefaultSubobject<UPGFPSDisplayComponent>(TEXT("FPSDisplay"));
}

void APGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UPGGameUserSettings* UserSettings = Cast<UPGGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		UserSettings->ApplySavedMobileQualitySettings(false);
	}

	ApplyGameSettings();
	CacheRunnerPawn(GetPawn());

	if (!HasRequiredInputAssets())
	{
		UE_LOG(LogTemp, Error, TEXT("PGPlayerController requires DefaultMappingContext, JumpAction, BalanceLeftAction, and BalanceRightAction."));
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (DefaultMappingContext)
			{
				InputSubsystem->AddMappingContext(DefaultMappingContext.Get(), DefaultMappingPriority);
			}
		}
	}

	const APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr;
	if (!GameMode || GameMode->GetRunState() == EPGRunState::Running)
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void APGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (DefaultMappingContext)
			{
				InputSubsystem->RemoveMappingContext(DefaultMappingContext.Get());
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void APGPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	CacheRunnerPawn(InPawn);
}

void APGPlayerController::OnUnPossess()
{
	if (RunHUDComponent)
	{
		RunHUDComponent->HandleRunnerUnpossessed();
	}
	CachedRunnerCharacter.Reset();
	Super::OnUnPossess();
}

void APGPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateGyroInput(DeltaTime);
}

void APGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("PGPlayerController requires EnhancedInputComponent. Check DefaultInput.ini."));
		return;
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction.Get(), ETriggerEvent::Started, this, &APGPlayerController::HandleJumpStarted);
	}

	if (GyroCalibrationAction)
	{
		EnhancedInputComponent->BindAction(GyroCalibrationAction.Get(), ETriggerEvent::Started, this, &APGPlayerController::HandleGyroCalibrationStarted);
	}

	if (BalanceLeftAction)
	{
		EnhancedInputComponent->BindAction(BalanceLeftAction.Get(), ETriggerEvent::Started, this, &APGPlayerController::HandleBalanceLeftStarted);
		EnhancedInputComponent->BindAction(BalanceLeftAction.Get(), ETriggerEvent::Completed, this, &APGPlayerController::HandleBalanceLeftCompleted);
		EnhancedInputComponent->BindAction(BalanceLeftAction.Get(), ETriggerEvent::Canceled, this, &APGPlayerController::HandleBalanceLeftCompleted);
	}

	if (BalanceRightAction)
	{
		EnhancedInputComponent->BindAction(BalanceRightAction.Get(), ETriggerEvent::Started, this, &APGPlayerController::HandleBalanceRightStarted);
		EnhancedInputComponent->BindAction(BalanceRightAction.Get(), ETriggerEvent::Completed, this, &APGPlayerController::HandleBalanceRightCompleted);
		EnhancedInputComponent->BindAction(BalanceRightAction.Get(), ETriggerEvent::Canceled, this, &APGPlayerController::HandleBalanceRightCompleted);
	}
}

void APGPlayerController::ApplyGameSettings()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	CachedGyroSettings = Settings->Gyro;
}

void APGPlayerController::CacheRunnerPawn(APawn* InPawn)
{
	CachedRunnerCharacter = Cast<APGRunnerCharacter>(InPawn);
	if (RunHUDComponent)
	{
		RunHUDComponent->HandleRunnerPossessed(CachedRunnerCharacter.Get());
	}
}

bool APGPlayerController::HasRequiredInputAssets() const
{
	return DefaultMappingContext && JumpAction && BalanceLeftAction && BalanceRightAction;
}

void APGPlayerController::HandleJumpStarted(const FInputActionValue& ActionValue)
{
	SendJumpRequest();
}

void APGPlayerController::HandleGyroCalibrationStarted(const FInputActionValue& ActionValue)
{
	if (!bHasCalibration)
	{
		// 첫 보정은 즉시 적용해 시작 순간 입력 튐을 막는다.
		CalibrationTilt = LastRawTilt;
		TargetCalibrationTilt = LastRawTilt;
		bHasCalibration = true;
		bIsCalibrating = false;
		return;
	}

	// 이미 기준값이 있으면 새 기준까지 부드럽게 보간해 플레이 중 급격한 입력 변화를 줄인다.
	TargetCalibrationTilt = LastRawTilt;
	bIsCalibrating = true;
}

void APGPlayerController::HandleBalanceLeftStarted(const FInputActionValue& ActionValue)
{
	bIsBalanceLeftHeld = true;
}

void APGPlayerController::HandleBalanceLeftCompleted(const FInputActionValue& ActionValue)
{
	bIsBalanceLeftHeld = false;
}

void APGPlayerController::HandleBalanceRightStarted(const FInputActionValue& ActionValue)
{
	bIsBalanceRightHeld = true;
}

void APGPlayerController::HandleBalanceRightCompleted(const FInputActionValue& ActionValue)
{
	bIsBalanceRightHeld = false;
}

void APGPlayerController::SendJumpRequest()
{
	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->RequestJump();
	}
}

void APGPlayerController::UpdateGyroInput(float DeltaTime)
{
	FVector Tilt = FVector::ZeroVector;
	FVector RotationRate = FVector::ZeroVector;
	FVector Gravity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	GetInputMotionState(Tilt, RotationRate, Gravity, Acceleration);

	// 프로젝트 설정에서 선택한 센서 축을 중심 제어 입력으로 사용한다.
	LastRawTilt = SelectTiltAxisValue(Tilt, CachedGyroSettings.TiltInputAxis);

	if (!bHasCalibration)
	{
		// 별도 보정 입력이 없어도 첫 프레임 자세를 기준값으로 삼아 중립 상태를 만든다.
		CalibrationTilt = LastRawTilt;
		TargetCalibrationTilt = LastRawTilt;
		bHasCalibration = true;
	}

	if (bIsCalibrating)
	{
		// 플레이 중 재보정 시 입력이 갑자기 튀지 않도록 설정값 기반 보간을 사용한다.
		CalibrationTilt = FMath::FInterpTo(CalibrationTilt, TargetCalibrationTilt, DeltaTime, CachedGyroSettings.CalibrationBlendSpeed);
		if (FMath::IsNearlyEqual(CalibrationTilt, TargetCalibrationTilt, 0.001f))
		{
			CalibrationTilt = TargetCalibrationTilt;
			bIsCalibrating = false;
		}
	}

	const float MotionBalanceInput = NormalizeTiltInput(LastRawTilt);
	const float FinalBalanceInput = ResolveFinalBalanceInput(MotionBalanceInput);

	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(FinalBalanceInput);
	}

}

float APGPlayerController::NormalizeTiltInput(float RawTilt) const
{
	const float DirectionScale = CachedGyroSettings.bInvertTiltInput ? -1.0f : 1.0f;
	const float CenteredTilt = (RawTilt - CalibrationTilt) * DirectionScale;

	// 민감도 적용 후 설정된 최대 입력 범위 안으로 제한한다.
	return FMath::Clamp(CenteredTilt * CachedGyroSettings.TiltSensitivity, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);
}

float APGPlayerController::GetKeyboardBalanceInput() const
{
	const float LeftInput = bIsBalanceLeftHeld ? -1.0f : 0.0f;
	const float RightInput = bIsBalanceRightHeld ? 1.0f : 0.0f;
	return FMath::Clamp(LeftInput + RightInput, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);
}

float APGPlayerController::ResolveFinalBalanceInput(float MotionBalanceInput) const
{
#if PLATFORM_ANDROID
	return MotionBalanceInput;
#else
	return GetKeyboardBalanceInput();
#endif
}
