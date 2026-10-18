// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/PGPlayerController.h"

#include "Character/PGRunnerCharacter.h"
#include "Audio/PGMusicComponent.h"
#include "Debug/PGFPSDisplayComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GameModes/PGGameModeBase.h"
#include "Home/PGHomeFlowComponent.h"
#include "InputActionValue.h"
#include "RunEvents/PGRunEventFeedbackComponent.h"
#include "RunHUD/PGRunHUDComponent.h"
#include "Settings/PGGameSettings.h"
#include "Settings/PGGameUserSettings.h"
#include "RunResults/PGRunResultFeedbackComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGPlayerController, Log, All);

// 뒤로가기 키를 Slate 라우팅 앞단에서 듣는 입력 전처리기.
// FSlateApplication::OnApplicationPreInputKeyDownListener는 WITH_EDITOR 전용이라 패키징(게임) 빌드에서는 컴파일되지 않는다.
class FPGBackKeyInputProcessor : public IInputProcessor
{
public:
	explicit FPGBackKeyInputProcessor(APGPlayerController* InController)
		: Controller(InController)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		const FKey Key = InKeyEvent.GetKey();
		if (!InKeyEvent.IsRepeat() && (Key == EKeys::Android_Back || Key == EKeys::Escape))
		{
			if (APGPlayerController* PlayerController = Controller.Get())
			{
				PlayerController->HandleBackRequested();
			}
		}
		// 읽기 전용 리스너: 이벤트를 소비하지 않아 위젯의 기존 키 처리는 그대로 진행된다.
		return false;
	}

private:
	TWeakObjectPtr<APGPlayerController> Controller;
};

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

	// 뒤로가기는 입력 모드(UIOnly 결과창 포함)와 무관하게 받아야 하므로 PlayerInput이 아니라 Slate 입력 앞단에서 듣는다.
	if (FSlateApplication::IsInitialized())
	{
		BackKeyInputProcessor = MakeShared<FPGBackKeyInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(BackKeyInputProcessor);
	}

	if (!HasRequiredInputAssets())
	{
		UE_LOG(LogPGPlayerController, Error, TEXT("PGPlayerController requires DefaultMappingContext, JumpAction, BalanceLeftAction, and BalanceRightAction."));
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
	if (BackKeyInputProcessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(BackKeyInputProcessor);
		}
		BackKeyInputProcessor.Reset();
	}
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

void APGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogPGPlayerController, Error, TEXT("PGPlayerController requires EnhancedInputComponent. Check DefaultInput.ini."));
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
	UpdateKeyboardBalanceInput();
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
	ResetSecondaryTouch();
}

void APGPlayerController::HandleJumpStarted(const FInputActionValue& ActionValue)
{
	SendJumpRequest();
}

void APGPlayerController::HandleBalanceLeftStarted(const FInputActionValue& ActionValue)
{
	bIsBalanceLeftHeld = true;
	UpdateKeyboardBalanceInput();
}

void APGPlayerController::HandleBalanceLeftCompleted(const FInputActionValue& ActionValue)
{
	bIsBalanceLeftHeld = false;
	UpdateKeyboardBalanceInput();
}

void APGPlayerController::HandleBalanceRightStarted(const FInputActionValue& ActionValue)
{
	bIsBalanceRightHeld = true;
	UpdateKeyboardBalanceInput();
}

void APGPlayerController::HandleBalanceRightCompleted(const FInputActionValue& ActionValue)
{
	bIsBalanceRightHeld = false;
	UpdateKeyboardBalanceInput();
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

	return bHandledBySuper || bHasActiveTouchGesture || bHasSecondaryTouch;
}

void APGPlayerController::BeginTouchGesture(uint32 Handle, const FVector2D& TouchLocation)
{
	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bHasActiveTouchGesture)
	{
		// 두 번째 손가락은 플릭 전용이다. 균형 드래그를 쥔 채 다른 손가락으로 점프할 수 있게 한다. 세 번째부터는 무시한다.
		if (!bHasSecondaryTouch && Handle != ActiveTouchHandle)
		{
			bHasSecondaryTouch = true;
			SecondaryTouchHandle = Handle;
			SecondaryTouchStartLocation = TouchLocation;
			SecondaryTouchStartTimeSeconds = NowSeconds;
		}
		return;
	}

	RefreshTouchPixelsPerMm();
	bHasActiveTouchGesture = true;
	ActiveTouchHandle = Handle;
	TouchStartLocation = TouchLocation;
	TouchStartTimeSeconds = NowSeconds;
	ClearTouchSamples();
	PushTouchSample(NowSeconds, TouchLocation);

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

	// 드래그 입력은 터치 시작점 기준 좌우 이동을 mm로 재서 데드존을 뺀 뒤 FullInputDistance에서 최대 입력에 닿게 한다.
	const float DragDeltaX = TouchLocation.X - TouchStartLocation.X;
	const float DeadZonePx = FMath::Max(0.0f, CachedTouchControlSettings.TouchDragDeadZoneMm) * TouchPixelsPerMm;
	const float FullInputPx = FMath::Max(CachedTouchControlSettings.TouchDragFullInputDistanceMm * TouchPixelsPerMm, KINDA_SMALL_NUMBER);
	const float MaxBalanceInput = FMath::Max(0.0f, CachedTouchControlSettings.MaxBalanceInput);
	float DragInput = 0.0f;
	if (FMath::Abs(DragDeltaX) > DeadZonePx)
	{
		DragInput = (FMath::Abs(DragDeltaX) - DeadZonePx) / FullInputPx * MaxBalanceInput * FMath::Sign(DragDeltaX);
	}

	APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get();
	if (RunnerCharacter)
	{
		// 터치 드래그는 직접 위치를 옮기지 않고 기존 Balance 가속도 경로에 넣습니다.
		RunnerCharacter->SetBalanceInput(FMath::Clamp(DragInput, -MaxBalanceInput, MaxBalanceInput));
	}

	// 드래그 중 위로 튕기면 손가락을 떼지 않고 점프한다. 터치 시작점이 아니라 최근 창을 보므로 긴 드래그 끝에서도 인정된다.
	// 좌우 플릭은 균형 드래그와 구분이 안 돼 드래그 중에는 받지 않는다.
	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : TouchStartTimeSeconds;
	FVector2D WindowDelta = FVector2D::ZeroVector;
	float WindowSeconds = 0.0f;
	if (GetRecentTouchWindow(TouchLocation, NowSeconds, WindowDelta, WindowSeconds)
		&& ClassifyTouchFlick(WindowDelta, WindowSeconds, false) == PGTouchGesture::EFlickDirection::Up)
	{
		if (RunnerCharacter)
		{
			RunnerCharacter->RequestJump();
		}
		// 점프가 거절(쿨타임·공중)돼도 창을 비운다. 프로젝트는 입력 버퍼를 두지 않는다(PGRunnerMovementComponent::RequestJump 주석).
		ClearTouchSamples();
	}
	PushTouchSample(NowSeconds, TouchLocation);
}

void APGPlayerController::EndTouchGesture(uint32 Handle, const FVector2D& TouchLocation)
{
	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bHasSecondaryTouch && Handle == SecondaryTouchHandle)
	{
		// 두 번째 손가락은 새 터치라 시작점 기준으로 세 방향 모두 판정한다.
		DispatchFlick(ClassifyTouchFlick(TouchLocation - SecondaryTouchStartLocation, NowSeconds - SecondaryTouchStartTimeSeconds, true));
		ResetSecondaryTouch();
		return;
	}

	if (!bHasActiveTouchGesture || Handle != ActiveTouchHandle)
	{
		return;
	}

	FVector2D WindowDelta = FVector2D::ZeroVector;
	float WindowSeconds = 0.0f;
	if (GetRecentTouchWindow(TouchLocation, NowSeconds, WindowDelta, WindowSeconds))
	{
		// 좌우 플릭은 짧은 새 터치에서만 받는다. 긴 드래그 끝에 손을 빠르게 떼는 동작을 측면 점프로 오인하지 않기 위해서다.
		const bool bAllowLateral = NowSeconds - TouchStartTimeSeconds <= CachedTouchControlSettings.FlickMaxDuration;
		DispatchFlick(ClassifyTouchFlick(WindowDelta, WindowSeconds, bAllowLateral));
	}
	ResetTouchGesture();
}

void APGPlayerController::ResetTouchGesture()
{
	bHasActiveTouchGesture = false;
	ActiveTouchHandle = 0;
	TouchStartLocation = FVector2D::ZeroVector;
	TouchStartTimeSeconds = 0.0f;
	ClearTouchSamples();

	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(0.0f);
	}
}

void APGPlayerController::ResetSecondaryTouch()
{
	bHasSecondaryTouch = false;
	SecondaryTouchHandle = 0;
	SecondaryTouchStartLocation = FVector2D::ZeroVector;
	SecondaryTouchStartTimeSeconds = 0.0f;
}

void APGPlayerController::RefreshTouchPixelsPerMm()
{
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	const FIntPoint ViewportSize(ViewportX, ViewportY);
	if (ViewportSize == TouchViewportSize)
	{
		return;
	}
	TouchViewportSize = ViewportSize;

	int32 Dpi = 0;
	FPlatformApplicationMisc::GetPhysicalScreenDensity(Dpi);
	if (Dpi <= 0)
	{
		// 밀도를 모르는 기기는 흔한 휴대폰 값으로 본다. 너무 낮게 잡으면 임계값이 픽셀로는 작아져 과민해진다.
		Dpi = 420;
	}

	// 뷰포트가 네이티브 화면보다 작게 렌더되는 기기(r.MobileContentScaleFactor)는 터치 좌표도 그만큼 줄어든다.
	// 세로·가로 보고 차이를 피하려고 긴 변끼리 비교한다.
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	const float DisplayLongSide = static_cast<float>(FMath::Max(DisplayMetrics.PrimaryDisplayWidth, DisplayMetrics.PrimaryDisplayHeight));
	const float ViewportLongSide = static_cast<float>(FMath::Max(ViewportX, ViewportY));
	const float ViewportScale = (DisplayLongSide > 0.0f && ViewportLongSide > 0.0f) ? FMath::Min(1.0f, ViewportLongSide / DisplayLongSide) : 1.0f;
	TouchPixelsPerMm = Dpi / 25.4f * ViewportScale;
}

void APGPlayerController::ClearTouchSamples()
{
	TouchSampleCount = 0;
	TouchSampleHead = 0;
}

void APGPlayerController::PushTouchSample(float TimeSeconds, const FVector2D& Location)
{
	TouchSamples[TouchSampleHead].TimeSeconds = TimeSeconds;
	TouchSamples[TouchSampleHead].Location = Location;
	TouchSampleHead = (TouchSampleHead + 1) % TouchSampleCapacity;
	TouchSampleCount = FMath::Min(TouchSampleCount + 1, TouchSampleCapacity);
}

bool APGPlayerController::GetRecentTouchWindow(const FVector2D& CurrentLocation, float CurrentTimeSeconds, FVector2D& OutDelta, float& OutDurationSeconds) const
{
	// 오래된 샘플부터 훑어 FlickMaxDuration 안에 드는 첫 샘플을 창의 시작으로 삼는다.
	for (int32 Index = 0; Index < TouchSampleCount; ++Index)
	{
		const FTouchSample& Sample = TouchSamples[(TouchSampleHead - TouchSampleCount + Index + TouchSampleCapacity) % TouchSampleCapacity];
		const float Elapsed = CurrentTimeSeconds - Sample.TimeSeconds;
		if (Elapsed <= CachedTouchControlSettings.FlickMaxDuration)
		{
			OutDelta = CurrentLocation - Sample.Location;
			OutDurationSeconds = FMath::Max(0.0f, Elapsed);
			return true;
		}
	}
	return false;
}

PGTouchGesture::EFlickDirection APGPlayerController::ClassifyTouchFlick(const FVector2D& DeltaPx, float DurationSeconds, bool bAllowLateral) const
{
	return PGTouchGesture::ClassifyFlick(
		DeltaPx,
		DurationSeconds,
		CachedTouchControlSettings.FlickMinDistanceMm * TouchPixelsPerMm,
		CachedTouchControlSettings.FlickMaxDuration,
		CachedTouchControlSettings.FlickMinSpeedMmPerSec * TouchPixelsPerMm,
		CachedTouchControlSettings.FlickDirectionDominance,
		bAllowLateral);
}

bool APGPlayerController::DispatchFlick(PGTouchGesture::EFlickDirection Direction)
{
	APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get();
	if (!RunnerCharacter)
	{
		return false;
	}

	switch (Direction)
	{
	case PGTouchGesture::EFlickDirection::Left:
		RunnerCharacter->RequestFlickJump(-1.0f);
		return true;
	case PGTouchGesture::EFlickDirection::Right:
		RunnerCharacter->RequestFlickJump(1.0f);
		return true;
	case PGTouchGesture::EFlickDirection::Up:
		RunnerCharacter->RequestJump();
		return true;
	default:
		return false;
	}
}

void APGPlayerController::UpdateKeyboardBalanceInput()
{
#if !PLATFORM_ANDROID
	// Q/E 입력은 상태가 바뀔 때만 전달한다. 값이 유지되는 동안 같은 setter를 매 프레임 호출할 필요가 없다.
	if (APGRunnerCharacter* RunnerCharacter = CachedRunnerCharacter.Get())
	{
		RunnerCharacter->SetBalanceInput(GetKeyboardBalanceInput());
	}
#endif
}

float APGPlayerController::GetKeyboardBalanceInput() const
{
	const float LeftInput = bIsBalanceLeftHeld ? -1.0f : 0.0f;
	const float RightInput = bIsBalanceRightHeld ? 1.0f : 0.0f;
	const float MaxBalanceInput = FMath::Max(0.0f, CachedTouchControlSettings.MaxBalanceInput);
	return FMath::Clamp(LeftInput + RightInput, -MaxBalanceInput, MaxBalanceInput);
}


void APGPlayerController::HandleBackRequested()
{
	// Home 모달 → 런 중 Pause → 결과창 Home 순서. 처리할 화면이 없으면(전환 중 등) 무시한다.
	if (HomeFlowComponent && HomeFlowComponent->TryHandleBack())
	{
		return;
	}
	if (RunHUDComponent && RunHUDComponent->TryHandleBack())
	{
		return;
	}
	if (RunResultFeedbackComponent)
	{
		RunResultFeedbackComponent->TryHandleBack();
	}
}
