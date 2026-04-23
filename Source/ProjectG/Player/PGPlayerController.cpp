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

	// 실제 모바일 조작 기준: 화면 좌/우 터치로 인접 라인 이동.
	InputComponent->BindTouch(IE_Pressed, this, &APGPlayerController::HandleTouchPressed);

	// 에디터 PIE에서 모바일 터치 없이 라인 이동을 확인하기 위한 폴백 입력.
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &APGPlayerController::MoveLeft);
	InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &APGPlayerController::MoveLeft);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &APGPlayerController::MoveRight);
	InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &APGPlayerController::MoveRight);

	// 에디터 PIE에서 자이로 없이 중심 기울기 반응을 확인하기 위한 폴백 입력.
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

	// 화면 중앙을 기준으로 왼쪽 터치는 -1, 오른쪽 터치는 +1 라인 이동으로 해석한다.
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

	// 현재는 기기를 좌우로 기울이는 값을 중심 제어 입력으로 사용한다.
	LastRawTilt = Tilt.Y;

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
		// 에디터 테스트에서는 키보드 입력을 자이로보다 우선해 반복 검증을 쉽게 한다.
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
		// 손 떨림이나 센서 미세 노이즈는 중심 입력으로 보지 않는다.
		return 0.0f;
	}

	// 민감도 적용 후 설정된 최대 입력 범위 안으로 제한한다.
	return FMath::Clamp(CenteredTilt * Settings->Gyro.TiltSensitivity, -Settings->Gyro.MaxTiltInput, Settings->Gyro.MaxTiltInput);
}
