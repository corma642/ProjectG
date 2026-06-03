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

	UpdateControlInput(DeltaTime);
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

#if !PLATFORM_ANDROID
	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction.Get(), ETriggerEvent::Started, this, &APGPlayerController::HandleJumpStarted);
	}
#endif

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
	CachedTouchControlSettings = Settings->TouchControl;
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
	ResetTouchGesture();
}

void APGPlayerController::HandleJumpStarted(const FInputActionValue& ActionValue)
{
	SendJumpRequest();
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

bool APGPlayerController::InputTouch(const FInputDeviceId DeviceId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, uint32 TouchpadIndex, const uint64 Timestamp)
{
	const bool bHandledBySuper = Super::InputTouch(DeviceId, Handle, Type, TouchLocation, Force, TouchpadIndex, Timestamp);

	switch (Type)
	{
	case ETouchType::Began:
		BeginTouchGesture(Handle, TouchLocation);
		break;
	case ETouchType::Moved:
	case ETouchType::FirstMove:
	case ETouchType::Stationary:
		UpdateTouchGesture(Handle, TouchLocation);
		break;
	case ETouchType::Ended:
		EndTouchGesture(Handle, TouchLocation);
		break;
	default:
		break;
	}

	return bHandledBySuper || bHasActiveTouchGesture;
}

void APGPlayerController::BeginTouchGesture(uint32 Handle, const FVector2D& TouchLocation)
{
	if (bHasActiveTouchGesture)
	{
		return;
	}

	bHasActiveTouchGesture = true;
	ActiveTouchHandle = Handle;
	TouchStartLocation = TouchLocation;
	TouchLastLocation = TouchLocation;
	TouchStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(0.0f);
	}
}

void APGPlayerController::UpdateTouchGesture(uint32 Handle, const FVector2D& TouchLocation)
{
	if (!bHasActiveTouchGesture || Handle != ActiveTouchHandle)
	{
		return;
	}

	TouchLastLocation = TouchLocation;

	const float DragDeltaX = TouchLocation.X - TouchStartLocation.X;
	const float DeadZone = FMath::Max(0.0f, CachedTouchControlSettings.TouchDragDeadZone);
	float DragInput = 0.0f;
	if (FMath::Abs(DragDeltaX) > DeadZone)
	{
		DragInput = (DragDeltaX - FMath::Sign(DragDeltaX) * DeadZone) * CachedTouchControlSettings.TouchDragSensitivity;
	}

	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		// 터치 드래그는 직접 위치를 옮기지 않고 기존 Balance 가속도 경로에 넣습니다.
		const float MaxBalanceInput = FMath::Max(0.0f, CachedTouchControlSettings.MaxBalanceInput);
		RunnerCharacter->SetBalanceInput(FMath::Clamp(DragInput, -MaxBalanceInput, MaxBalanceInput));
	}
}

void APGPlayerController::EndTouchGesture(uint32 Handle, const FVector2D& TouchLocation)
{
	if (!bHasActiveTouchGesture || Handle != ActiveTouchHandle)
	{
		return;
	}

	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : TouchStartTimeSeconds;
	const float GestureDurationSeconds = FMath::Max(0.0f, CurrentTimeSeconds - TouchStartTimeSeconds);
	TryHandleFlickGesture(TouchLocation, GestureDurationSeconds);
	ResetTouchGesture();
}

void APGPlayerController::ResetTouchGesture()
{
	bHasActiveTouchGesture = false;
	ActiveTouchHandle = 0;
	TouchStartLocation = FVector2D::ZeroVector;
	TouchLastLocation = FVector2D::ZeroVector;
	TouchStartTimeSeconds = 0.0f;

	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(0.0f);
	}
}

bool APGPlayerController::TryHandleFlickGesture(const FVector2D& EndLocation, float GestureDurationSeconds)
{
	const FVector2D GestureDelta = EndLocation - TouchStartLocation;
	const float Distance = GestureDelta.Size();
	const float Duration = FMath::Max(GestureDurationSeconds, KINDA_SMALL_NUMBER);
	const float Speed = Distance / Duration;

	if (GestureDurationSeconds > CachedTouchControlSettings.FlickMaxDuration
		|| Distance < CachedTouchControlSettings.FlickMinDistance
		|| Speed < CachedTouchControlSettings.FlickMinSpeed)
	{
		return false;
	}

	APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get();
	if (!RunnerCharacter)
	{
		return false;
	}

	const float AbsX = FMath::Abs(GestureDelta.X);
	const float AbsY = FMath::Abs(GestureDelta.Y);
	const float DirectionDominance = FMath::Max(1.0f, CachedTouchControlSettings.FlickDirectionDominance);

	if (AbsX >= AbsY * DirectionDominance)
	{
		RunnerCharacter->RequestFlickJump(GestureDelta.X > 0.0f ? 1.0f : -1.0f);
		return true;
	}

	// UE 화면 좌표는 Y가 아래로 증가하므로 위쪽 플릭은 음수 Y 이동입니다.
	if (GestureDelta.Y < 0.0f && AbsY >= AbsX * DirectionDominance)
	{
		RunnerCharacter->RequestJump();
		return true;
	}

	return false;
}

void APGPlayerController::UpdateControlInput(float DeltaTime)
{
#if !PLATFORM_ANDROID
	// 데스크탑 Q/E 테스트 입력은 모바일 센서 보정 상태와 무관해야 한다.
	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(GetKeyboardBalanceInput());
	}
	return;
#else
	// Android 조작은 MotionState 센서 입력을 사용하지 않는다.
	// Balance 입력은 InputTouch()에서 드래그 제스처를 기존 중심 보정값으로 변환해 전달한다.
	return;

#endif
}

float APGPlayerController::GetKeyboardBalanceInput() const
{
	const float LeftInput = bIsBalanceLeftHeld ? -1.0f : 0.0f;
	const float RightInput = bIsBalanceRightHeld ? 1.0f : 0.0f;
	const float MaxBalanceInput = FMath::Max(0.0f, CachedTouchControlSettings.MaxBalanceInput);
	return FMath::Clamp(LeftInput + RightInput, -MaxBalanceInput, MaxBalanceInput);
}

