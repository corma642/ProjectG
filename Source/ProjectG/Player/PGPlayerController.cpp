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

	APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr;
	if (GameMode)
	{
		GameMode->OnRunCountdownStarted.AddDynamic(this, &APGPlayerController::HandleRunCountdownStarted);

		// Retry는 Home 화면을 건너뛰므로 Controller가 델리게이트를 구독하기 전에
		// 카운트다운이 시작됐더라도 현재 상태를 기준으로 입력 초기화를 복구한다.
		if (GameMode->GetRunState() == EPGRunState::Countdown)
		{
			HandleRunCountdownStarted(0.0f);
		}
	}
	if (!GameMode || GameMode->GetRunState() == EPGRunState::Running)
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void APGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		GameMode->OnRunCountdownStarted.RemoveDynamic(this, &APGPlayerController::HandleRunCountdownStarted);
	}

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

void APGPlayerController::HandleRunCountdownStarted(float Duration)
{
	bIsBalanceLeftHeld = false;
	bIsBalanceRightHeld = false;

	// Android 첫 실행 직후 MotionState는 몇 프레임 동안 불안정할 수 있다.
	// 실제 런 시작 전 카운트다운 구간에서 현재 자세를 다시 평균내 첫 판 기준점 틀어짐을 막는다.
	bHasCalibration = false;
	bIsCalibrating = false;
	StartGyroCalibrationSample(false);
}

void APGPlayerController::HandleJumpStarted(const FInputActionValue& ActionValue)
{
	SendJumpRequest();
}

void APGPlayerController::HandleGyroCalibrationStarted(const FInputActionValue& ActionValue)
{
	// 수동 보정도 단일 프레임 값이 아니라 짧은 평균값을 사용한다.
	// 이미 기준값이 있는 경우에는 샘플 완료 후 새 기준까지 부드럽게 보간한다.
	StartGyroCalibrationSample(bHasCalibration);
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
#if !PLATFORM_ANDROID
	// 데스크탑 Q/E 테스트 입력은 모바일 센서 보정 상태와 무관해야 한다.
	// Retry 직후 MotionState 샘플링이 아직 시작되지 않았더라도 키보드 입력을 즉시 전달한다.
	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(GetKeyboardBalanceInput());
	}
	return;
#else

	FVector Tilt = FVector::ZeroVector;
	FVector RotationRate = FVector::ZeroVector;
	FVector Gravity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	GetInputMotionState(Tilt, RotationRate, Gravity, Acceleration);

	// 프로젝트 설정에서 선택한 센서 축을 중심 제어 입력으로 사용한다.
	LastRawTilt = SelectTiltAxisValue(Tilt, CachedGyroSettings.TiltInputAxis);

	if (bIsSamplingCalibration)
	{
		CalibrationSampleElapsedTime += DeltaTime;
		CalibrationSampleSum += LastRawTilt;
		++CalibrationSampleCount;

		if (CalibrationSampleElapsedTime >= CachedGyroSettings.AutoCalibrationSampleDuration)
		{
			FinishGyroCalibrationSample();
		}
		else
		{
			// 기준점 샘플링 중에는 입력을 중립으로 고정한다. 카운트다운 중에는 조작이 잠겨 있고,
			// 수동 보정 중에는 잠깐의 중립 유지가 단일 프레임 튐보다 낫다.
			if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
			{
				RunnerCharacter->SetBalanceInput(0.0f);
			}
			return;
		}
	}

	if (!bHasCalibration)
	{
		if (CachedGyroSettings.AutoCalibrationSampleDuration <= KINDA_SMALL_NUMBER)
		{
			StartGyroCalibrationSample(false);
		}
		else
		{
			// 첫 앱 실행 직후의 단일 MotionState 값은 기준점으로 쓰지 않는다.
			// START/Countdown 또는 수동 보정에서 샘플링이 시작될 때까지 중립 입력만 전달한다.
			if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
			{
				RunnerCharacter->SetBalanceInput(0.0f);
			}
			return;
		}
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

#endif
}

void APGPlayerController::StartGyroCalibrationSample(bool bBlendToSample)
{
	bIsSamplingCalibration = true;
	bBlendCalibrationSample = bBlendToSample;
	CalibrationSampleSum = 0.0f;
	CalibrationSampleCount = 0;
	CalibrationSampleElapsedTime = 0.0f;
	bIsCalibrating = false;

	if (CachedGyroSettings.AutoCalibrationSampleDuration <= KINDA_SMALL_NUMBER)
	{
		CalibrationSampleSum = LastRawTilt;
		CalibrationSampleCount = 1;
		FinishGyroCalibrationSample();
	}
}

void APGPlayerController::FinishGyroCalibrationSample()
{
	const float SampledTilt = CalibrationSampleCount > 0 ? CalibrationSampleSum / static_cast<float>(CalibrationSampleCount) : LastRawTilt;
	bIsSamplingCalibration = false;
	CalibrationSampleElapsedTime = 0.0f;
	CalibrationSampleSum = 0.0f;
	CalibrationSampleCount = 0;

	if (bBlendCalibrationSample && bHasCalibration)
	{
		TargetCalibrationTilt = SampledTilt;
		bIsCalibrating = true;
		return;
	}

	CalibrationTilt = SampledTilt;
	TargetCalibrationTilt = SampledTilt;
	bHasCalibration = true;
	bIsCalibrating = false;
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
