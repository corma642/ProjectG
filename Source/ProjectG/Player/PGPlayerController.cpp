// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/PGPlayerController.h"

#include "Character/PGRunnerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "RunEvents/PGRunEventFeedbackComponent.h"
#include "Settings/PGGameSettings.h"

APGPlayerController::APGPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	RunEventFeedbackComponent = CreateDefaultSubobject<UPGRunEventFeedbackComponent>(TEXT("RunEventFeedback"));
}

void APGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyGameSettings();
	EnsureInputAssets();
	CacheRunnerPawn(GetPawn());

	if ((DefaultMappingContext || TouchLaneAction || LaneLeftAction || LaneRightAction || BalanceLeftAction || BalanceRightAction || GyroCalibrationAction) && !HasCompleteConfiguredInputAssets())
	{
		// 입력 에셋은 일부만 지정하면 액션/매핑 불일치가 생기므로 안전한 C++ 기본 매핑으로 되돌린다.
		UE_LOG(LogTemp, Warning, TEXT("PGPlayerController input assets are incomplete. Falling back to native Sprint 1 input mapping."));
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(GetResolvedMappingContext(), DefaultMappingPriority);
		}
	}

	SetInputMode(FInputModeGameOnly());
}

void APGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->RemoveMappingContext(GetResolvedMappingContext());
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

	EnsureInputAssets();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("PGPlayerController requires EnhancedInputComponent. Check DefaultInput.ini."));
		return;
	}

	// Sprint 1 입력은 Enhanced Input 액션을 기준으로 통일한다.
	EnhancedInputComponent->BindAction(GetResolvedTouchLaneAction(), ETriggerEvent::Started, this, &APGPlayerController::HandleTouchLaneStarted);
	EnhancedInputComponent->BindAction(GetResolvedLaneLeftAction(), ETriggerEvent::Started, this, &APGPlayerController::HandleLaneLeftStarted);
	EnhancedInputComponent->BindAction(GetResolvedLaneRightAction(), ETriggerEvent::Started, this, &APGPlayerController::HandleLaneRightStarted);
	EnhancedInputComponent->BindAction(GetResolvedBalanceLeftAction(), ETriggerEvent::Started, this, &APGPlayerController::HandleBalanceLeftStarted);
	EnhancedInputComponent->BindAction(GetResolvedBalanceLeftAction(), ETriggerEvent::Completed, this, &APGPlayerController::HandleBalanceLeftCompleted);
	EnhancedInputComponent->BindAction(GetResolvedBalanceLeftAction(), ETriggerEvent::Canceled, this, &APGPlayerController::HandleBalanceLeftCompleted);
	EnhancedInputComponent->BindAction(GetResolvedBalanceRightAction(), ETriggerEvent::Started, this, &APGPlayerController::HandleBalanceRightStarted);
	EnhancedInputComponent->BindAction(GetResolvedBalanceRightAction(), ETriggerEvent::Completed, this, &APGPlayerController::HandleBalanceRightCompleted);
	EnhancedInputComponent->BindAction(GetResolvedBalanceRightAction(), ETriggerEvent::Canceled, this, &APGPlayerController::HandleBalanceRightCompleted);
	EnhancedInputComponent->BindAction(GetResolvedGyroCalibrationAction(), ETriggerEvent::Started, this, &APGPlayerController::HandleGyroCalibrationStarted);
}

void APGPlayerController::ApplyGameSettings()
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	CachedGyroSettings = Settings->Gyro;
	CachedDebugSettings = Settings->Debug;
}

void APGPlayerController::CacheRunnerPawn(APawn* InPawn)
{
	CachedRunnerCharacter = Cast<APGRunnerCharacter>(InPawn);
}

void APGPlayerController::EnsureInputAssets()
{
	if (!NativeTouchLaneAction)
	{
		NativeTouchLaneAction = NewObject<UInputAction>(this, TEXT("IA_PG_NativeTouchLane"));
		NativeTouchLaneAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!NativeLaneLeftAction)
	{
		NativeLaneLeftAction = NewObject<UInputAction>(this, TEXT("IA_PG_NativeLaneLeft"));
		NativeLaneLeftAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!NativeLaneRightAction)
	{
		NativeLaneRightAction = NewObject<UInputAction>(this, TEXT("IA_PG_NativeLaneRight"));
		NativeLaneRightAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!NativeBalanceLeftAction)
	{
		NativeBalanceLeftAction = NewObject<UInputAction>(this, TEXT("IA_PG_NativeBalanceLeft"));
		NativeBalanceLeftAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!NativeBalanceRightAction)
	{
		NativeBalanceRightAction = NewObject<UInputAction>(this, TEXT("IA_PG_NativeBalanceRight"));
		NativeBalanceRightAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!NativeGyroCalibrationAction)
	{
		NativeGyroCalibrationAction = NewObject<UInputAction>(this, TEXT("IA_PG_NativeGyroCalibration"));
		NativeGyroCalibrationAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!NativeMappingContext)
	{
		NativeMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_PG_NativeDefault"));

		// 에셋이 없어도 Sprint 1 검증이 가능하도록 C++ 기본 키 매핑을 제공한다.
		NativeMappingContext->MapKey(NativeTouchLaneAction.Get(), EKeys::TouchKeys[static_cast<int32>(ETouchIndex::Touch1)]);
		NativeMappingContext->MapKey(NativeLaneLeftAction.Get(), EKeys::A);
		NativeMappingContext->MapKey(NativeLaneLeftAction.Get(), EKeys::Left);
		NativeMappingContext->MapKey(NativeLaneRightAction.Get(), EKeys::D);
		NativeMappingContext->MapKey(NativeLaneRightAction.Get(), EKeys::Right);
		NativeMappingContext->MapKey(NativeBalanceLeftAction.Get(), EKeys::Q);
		NativeMappingContext->MapKey(NativeBalanceRightAction.Get(), EKeys::E);
		NativeMappingContext->MapKey(NativeGyroCalibrationAction.Get(), EKeys::C);
	}
}

UInputMappingContext* APGPlayerController::GetResolvedMappingContext() const
{
	return HasCompleteConfiguredInputAssets() ? DefaultMappingContext.Get() : NativeMappingContext.Get();
}

bool APGPlayerController::HasCompleteConfiguredInputAssets() const
{
	return DefaultMappingContext && TouchLaneAction && LaneLeftAction && LaneRightAction && BalanceLeftAction && BalanceRightAction && GyroCalibrationAction;
}

UInputAction* APGPlayerController::GetResolvedTouchLaneAction() const
{
	return HasCompleteConfiguredInputAssets() ? TouchLaneAction.Get() : NativeTouchLaneAction.Get();
}

UInputAction* APGPlayerController::GetResolvedLaneLeftAction() const
{
	return HasCompleteConfiguredInputAssets() ? LaneLeftAction.Get() : NativeLaneLeftAction.Get();
}

UInputAction* APGPlayerController::GetResolvedLaneRightAction() const
{
	return HasCompleteConfiguredInputAssets() ? LaneRightAction.Get() : NativeLaneRightAction.Get();
}

UInputAction* APGPlayerController::GetResolvedBalanceLeftAction() const
{
	return HasCompleteConfiguredInputAssets() ? BalanceLeftAction.Get() : NativeBalanceLeftAction.Get();
}

UInputAction* APGPlayerController::GetResolvedBalanceRightAction() const
{
	return HasCompleteConfiguredInputAssets() ? BalanceRightAction.Get() : NativeBalanceRightAction.Get();
}

UInputAction* APGPlayerController::GetResolvedGyroCalibrationAction() const
{
	return HasCompleteConfiguredInputAssets() ? GyroCalibrationAction.Get() : NativeGyroCalibrationAction.Get();
}

void APGPlayerController::HandleTouchLaneStarted(const FInputActionValue& ActionValue)
{
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);

	float TouchX = 0.0f;
	float TouchY = 0.0f;
	bool bIsTouchPressed = false;
	GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bIsTouchPressed);

	if (ViewportSizeX <= 0 || !bIsTouchPressed)
	{
		return;
	}

	// 화면 중앙을 기준으로 왼쪽 터치는 -1, 오른쪽 터치는 +1 라인 이동으로 해석한다.
	SendLaneChange(TouchX < static_cast<float>(ViewportSizeX) * 0.5f ? -1 : 1);
}

void APGPlayerController::HandleLaneLeftStarted(const FInputActionValue& ActionValue)
{
	SendLaneChange(-1);
}

void APGPlayerController::HandleLaneRightStarted(const FInputActionValue& ActionValue)
{
	SendLaneChange(1);
}

void APGPlayerController::HandleBalanceLeftStarted(const FInputActionValue& ActionValue)
{
	bIsKeyboardBalanceLeftHeld = true;
	KeyboardBalanceInput = static_cast<float>(static_cast<int32>(bIsKeyboardBalanceRightHeld) - static_cast<int32>(bIsKeyboardBalanceLeftHeld));
}

void APGPlayerController::HandleBalanceLeftCompleted(const FInputActionValue& ActionValue)
{
	bIsKeyboardBalanceLeftHeld = false;
	KeyboardBalanceInput = static_cast<float>(static_cast<int32>(bIsKeyboardBalanceRightHeld) - static_cast<int32>(bIsKeyboardBalanceLeftHeld));
}

void APGPlayerController::HandleBalanceRightStarted(const FInputActionValue& ActionValue)
{
	bIsKeyboardBalanceRightHeld = true;
	KeyboardBalanceInput = static_cast<float>(static_cast<int32>(bIsKeyboardBalanceRightHeld) - static_cast<int32>(bIsKeyboardBalanceLeftHeld));
}

void APGPlayerController::HandleBalanceRightCompleted(const FInputActionValue& ActionValue)
{
	bIsKeyboardBalanceRightHeld = false;
	KeyboardBalanceInput = static_cast<float>(static_cast<int32>(bIsKeyboardBalanceRightHeld) - static_cast<int32>(bIsKeyboardBalanceLeftHeld));
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

void APGPlayerController::SendLaneChange(int32 Direction)
{
	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->RequestLaneChange(Direction);
	}
}

void APGPlayerController::UpdateGyroInput(float DeltaTime)
{
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
		CalibrationTilt = FMath::FInterpTo(CalibrationTilt, TargetCalibrationTilt, DeltaTime, CachedGyroSettings.CalibrationBlendSpeed);
		if (FMath::IsNearlyEqual(CalibrationTilt, TargetCalibrationTilt, 0.001f))
		{
			CalibrationTilt = TargetCalibrationTilt;
			bIsCalibrating = false;
		}
	}

	float FinalBalanceInput = NormalizeTiltInput(LastRawTilt);

#if WITH_EDITOR
	if (CachedDebugSettings.bUseKeyboardFallbackInEditor && !FMath::IsNearlyZero(KeyboardBalanceInput))
	{
		// 에디터 테스트에서는 키보드 입력을 자이로보다 우선해 반복 검증을 쉽게 한다.
		FinalBalanceInput = KeyboardBalanceInput;
	}
#endif

	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(FinalBalanceInput);
	}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (CachedDebugSettings.bDrawGyroDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			reinterpret_cast<uint64>(this),
			0.0f,
			FColor::Cyan,
			FString::Printf(TEXT("Tilt Raw: %.3f | Balance: %.3f | Keyboard(Q=-1/E=+1): %.1f | Calibration: %.3f"), LastRawTilt, FinalBalanceInput, KeyboardBalanceInput, CalibrationTilt));
	}
#endif
}

float APGPlayerController::NormalizeTiltInput(float RawTilt) const
{
	const float CenteredTilt = RawTilt - CalibrationTilt;

	if (FMath::Abs(CenteredTilt) <= CachedGyroSettings.TiltDeadZone)
	{
		// 손 떨림이나 센서 미세 노이즈는 중심 입력으로 보지 않는다.
		return 0.0f;
	}

	// 민감도 적용 후 설정된 최대 입력 범위 안으로 제한한다.
	return FMath::Clamp(CenteredTilt * CachedGyroSettings.TiltSensitivity, -CachedGyroSettings.MaxTiltInput, CachedGyroSettings.MaxTiltInput);
}
