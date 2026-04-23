// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/PGPlayerController.h"

#include "Character/PGRunnerCharacter.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "Settings/PGGameSettings.h"

APGPlayerController::APGPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
}

void APGPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateGyroInput(DeltaTime);
}

void APGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindTouch(IE_Pressed, this, &APGPlayerController::HandleTouchPressed);

	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &APGPlayerController::MoveLeft);
	InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &APGPlayerController::MoveLeft);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &APGPlayerController::MoveRight);
	InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &APGPlayerController::MoveRight);

	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &APGPlayerController::StartKeyboardBalanceLeft);
	InputComponent->BindKey(EKeys::Q, IE_Released, this, &APGPlayerController::StopKeyboardBalanceLeft);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APGPlayerController::StartKeyboardBalanceRight);
	InputComponent->BindKey(EKeys::E, IE_Released, this, &APGPlayerController::StopKeyboardBalanceRight);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &APGPlayerController::RequestGyroCalibration);
}

void APGPlayerController::HandleTouchPressed(ETouchIndex::Type FingerIndex, FVector TouchLocation)
{
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);

	if (ViewportSizeX <= 0)
	{
		return;
	}

	SendLaneChange(TouchLocation.X < static_cast<float>(ViewportSizeX) * 0.5f ? -1 : 1);
}

void APGPlayerController::MoveLeft()
{
	SendLaneChange(-1);
}

void APGPlayerController::MoveRight()
{
	SendLaneChange(1);
}

void APGPlayerController::StartKeyboardBalanceLeft()
{
	KeyboardBalanceInput = -1.0f;
}

void APGPlayerController::StopKeyboardBalanceLeft()
{
	if (KeyboardBalanceInput < 0.0f)
	{
		KeyboardBalanceInput = 0.0f;
	}
}

void APGPlayerController::StartKeyboardBalanceRight()
{
	KeyboardBalanceInput = 1.0f;
}

void APGPlayerController::StopKeyboardBalanceRight()
{
	if (KeyboardBalanceInput > 0.0f)
	{
		KeyboardBalanceInput = 0.0f;
	}
}

void APGPlayerController::RequestGyroCalibration()
{
	if (!bHasCalibration)
	{
		CalibrationTilt = LastRawTilt;
		TargetCalibrationTilt = LastRawTilt;
		bHasCalibration = true;
		bIsCalibrating = false;
		return;
	}

	TargetCalibrationTilt = LastRawTilt;
	bIsCalibrating = true;
}

void APGPlayerController::SendLaneChange(int32 Direction)
{
	if (APGRunnerCharacter* RunnerCharacter = Cast<APGRunnerCharacter>(GetPawn()))
	{
		RunnerCharacter->RequestLaneChange(Direction);
	}
}

void APGPlayerController::UpdateGyroInput(float DeltaTime)
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();

	FVector Tilt = FVector::ZeroVector;
	FVector RotationRate = FVector::ZeroVector;
	FVector Gravity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	GetInputMotionState(Tilt, RotationRate, Gravity, Acceleration);

	LastRawTilt = Tilt.Y;

	if (!bHasCalibration)
	{
		CalibrationTilt = LastRawTilt;
		TargetCalibrationTilt = LastRawTilt;
		bHasCalibration = true;
	}

	if (bIsCalibrating)
	{
		CalibrationTilt = FMath::FInterpTo(CalibrationTilt, TargetCalibrationTilt, DeltaTime, Settings->Gyro.CalibrationBlendSpeed);
		if (FMath::IsNearlyEqual(CalibrationTilt, TargetCalibrationTilt, 0.001f))
		{
			CalibrationTilt = TargetCalibrationTilt;
			bIsCalibrating = false;
		}
	}

	float FinalBalanceInput = NormalizeTiltInput(LastRawTilt);

#if WITH_EDITOR
	if (Settings->Debug.bUseKeyboardFallbackInEditor && !FMath::IsNearlyZero(KeyboardBalanceInput))
	{
		FinalBalanceInput = KeyboardBalanceInput;
	}
#endif

	if (APGRunnerCharacter* RunnerCharacter = Cast<APGRunnerCharacter>(GetPawn()))
	{
		RunnerCharacter->SetBalanceInput(FinalBalanceInput);
	}

	if (Settings->Debug.bDrawGyroDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			reinterpret_cast<uint64>(this),
			0.0f,
			FColor::Cyan,
			FString::Printf(TEXT("Tilt Raw: %.3f | Balance: %.3f | Calibration: %.3f"), LastRawTilt, FinalBalanceInput, CalibrationTilt));
	}
}

float APGPlayerController::NormalizeTiltInput(float RawTilt) const
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const float CenteredTilt = RawTilt - CalibrationTilt;

	if (FMath::Abs(CenteredTilt) <= Settings->Gyro.TiltDeadZone)
	{
		return 0.0f;
	}

	return FMath::Clamp(CenteredTilt * Settings->Gyro.TiltSensitivity, -Settings->Gyro.MaxTiltInput, Settings->Gyro.MaxTiltInput);
}
