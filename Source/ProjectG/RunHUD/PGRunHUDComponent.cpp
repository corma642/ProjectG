// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunHUD/PGRunHUDComponent.h"

#include "Blueprint/UserWidget.h"
#include "Audio/PGAudioStatics.h"
#include "Audio/PGMusicComponent.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Character/PGRunnerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "Player/PGPlayerController.h"
#include "RunProgress/PGRunProgressComponent.h"
#include "Settings/PGGameUserSettings.h"
#include "UI/PGCargoDeliveryItemWidget.h"
#include "UI/PGCargoScoreIndicatorWidget.h"
#include "UI/PGRunHUDWidget.h"
#include "UI/PGRunPauseWidget.h"
#include "UI/PGSettingsScreenWidget.h"
#include "User/PGUserDataSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunHUDComponent, Log, All);

namespace
{
	// 드래그 안내를 끝내는 중심 입력량(절댓값, 0~1). 작은 떨림이 아니라 실제로 끌었다고 볼 수 있는 값이다.
	constexpr float RunGuideDragInputThreshold = 0.5f;

	// BP_Bowl에서 테두리 경고 링 메시 컴포넌트를 찾는 컴포넌트 태그와, 링 머티리얼(M_CargoDangerRing)의 파라미터 이름.
	const FName CargoDangerRingTag(TEXT("CargoDangerRing"));
	const FName CargoDangerRingIntensityParam(TEXT("Intensity"));
	const FName CargoDangerRingDirectionParam(TEXT("Direction"));
}

UPGRunHUDComponent::UPGRunHUDComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPGRunHUDComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPlayerController = Cast<APGPlayerController>(GetOwner());
	if (!OwnerPlayerController)
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("PGRunHUDComponent must be owned by APGPlayerController."));
		return;
	}

	LoadSettings();
	BindGameMode();
	HandleRunnerPossessed(Cast<APGRunnerCharacter>(OwnerPlayerController->GetPawn()));

	if (BoundGameMode && (BoundGameMode->GetRunState() == EPGRunState::Countdown || BoundGameMode->GetRunState() == EPGRunState::Running))
	{
		// HUD는 카운트다운부터 보여준다. 점수/거리 증가는 GO 이후 RunProgress가 시작되면서 반영된다.
		ShowHUD();
	}
}

void UPGRunHUDComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameMode();
	HandleRunnerUnpossessed();
	CleanupSettingsScreen();
	CleanupPauseScreen(false);
	CleanupHUD();

	Super::EndPlay(EndPlayReason);
}

void UPGRunHUDComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RunGuideWidget && RunGuideStep == EPGRunGuideStep::Drag)
	{
		const APGRunnerCharacter* Runner = BoundRunnerCharacter.Get();
		if (Runner && FMath::Abs(Runner->GetBalanceInput()) >= RunGuideDragInputThreshold)
		{
			AdvanceRunGuide(EPGRunGuideStep::Flick);
		}
	}

	if (!HUDWidget || !BoundCargoStackComponent)
	{
		return;
	}

	CargoRefreshElapsed += DeltaTime;
	const float RefreshInterval = FMath::Max(0.03f, CachedRunHUDSettings.CargoListRefreshInterval);
	if (CargoRefreshElapsed >= RefreshInterval)
	{
		// 적재물 남은 시간/게이지는 계속 변하지만, 모바일 UI 비용을 줄이기 위해 설정 간격으로만 갱신한다.
		CargoRefreshElapsed = 0.0f;
		RefreshCargoList();
		UpdateCargoDangerWarning();
	}
}

void UPGRunHUDComponent::HandleRunnerPossessed(APGRunnerCharacter* RunnerCharacter)
{
	BindRunProgress(RunnerCharacter ? RunnerCharacter->GetRunProgressComponent() : nullptr);
	BindCargoStack(RunnerCharacter ? RunnerCharacter->GetCargoStackComponent() : nullptr);
	BindRunner(RunnerCharacter);
}

void UPGRunHUDComponent::HandleRunnerUnpossessed()
{
	UnbindRunProgress();
	UnbindCargoStack();
	UnbindRunner();
}

void UPGRunHUDComponent::HandleRunStarted()
{
	HandleRunnerPossessed(Cast<APGRunnerCharacter>(OwnerPlayerController ? OwnerPlayerController->GetPawn() : nullptr));
	ShowHUD();
	UpdateHUDInteractionState();
	RefreshHUDValues();
	RefreshCargoList();
	ShowRunGuideIfNeeded();
}

void UPGRunHUDComponent::HandleRunCountdownStarted(float Duration)
{
	HandleRunnerPossessed(Cast<APGRunnerCharacter>(OwnerPlayerController ? OwnerPlayerController->GetPawn() : nullptr));
	ShowHUD();
	UpdateHUDInteractionState();
	RefreshHUDValues();
	RefreshCargoList();
}

void UPGRunHUDComponent::HandleRunFailed(FName FailureReason)
{
	CleanupSettingsScreen();
	CleanupPauseScreen(false);
	RefreshHUDValues();
	RefreshCargoList();
	UpdateHUDInteractionState();
	SetComponentTickEnabled(false);
	CargoRefreshElapsed = 0.0f;

	// 바구니 테두리 링은 슬로모션 동안 마지막 상태로 남겨 실패 원인이 보이게 두고, 결과 화면이 뜰 때 바구니와 함께 사라진다.
	bIsCargoDangerActive = false;
	// 가이드를 끝내지 못한 채 실패하면 완료로 저장하지 않아 다음 런에서 다시 보여준다.
	CleanupRunGuide(true);
}

void UPGRunHUDComponent::HandleDisplayedDistanceChanged(float DisplayedDistanceMeters)
{
	if (HUDWidget)
	{
		HUDWidget->SetDistanceMeters(DisplayedDistanceMeters);
	}
}

void UPGRunHUDComponent::HandleScoreChanged(int32 Score, FName Reason)
{
	if (HUDWidget)
	{
		HUDWidget->SetScore(Score);
	}
}

void UPGRunHUDComponent::HandleReceiptsChanged(int32 Receipts, FName Reason)
{
	if (HUDWidget)
	{
		HUDWidget->SetReceipts(Receipts);
	}
}

void UPGRunHUDComponent::HandleCargoListChanged()
{
	RefreshCargoList();
}

void UPGRunHUDComponent::HandleCargoDelivered(int32 CargoInstanceId, int32 Score, int32 Receipts, FVector DeliveryLocation)
{
	UWorld* World = GetWorld();
	if (World)
	{
		if (UNiagaraSystem* DeliveredVFX = CachedRunHUDSettings.CargoDeliveredVFX.Get())
		{
			if (BoundCargoStackComponent)
			{
				// 완료 VFX는 CargoStack에 부착해 러너가 전진해도 적재물 위치 연출이 뒤에 남지 않게 한다.
				const FVector RelativeDeliveryLocation = BoundCargoStackComponent->GetComponentTransform().InverseTransformPosition(DeliveryLocation);
				UNiagaraFunctionLibrary::SpawnSystemAttached(
					DeliveredVFX,
					BoundCargoStackComponent,
					NAME_None,
					RelativeDeliveryLocation,
					FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset,
					true);
			}
			else
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, DeliveredVFX, DeliveryLocation);
			}
		}

		if (USoundBase* DeliveredSound = CachedRunHUDSettings.CargoDeliveredSound.Get())
		{
			UGameplayStatics::PlaySoundAtLocation(World, DeliveredSound, DeliveryLocation, UPGGameUserSettings::GetCurrentSFXVolumeScale());
		}
	}

	if (HUDWidget)
	{
		HUDWidget->ShowCargoScoreIndicator(CargoInstanceId, Score);
	}
}

void UPGRunHUDComponent::HandleRunnerJumpStarted()
{
	if (!RunGuideWidget || RunGuideStep != EPGRunGuideStep::Flick)
	{
		return;
	}

	UPGUserDataSubsystem* UserData = GetUserDataSubsystem();
	if (!UserData || !UserData->TryCompleteRunGuide())
	{
		// 저장이 실패해도 이번 런의 안내는 끝낸다. 플래그가 남지 않아 다음 런에서 다시 보여준다.
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("Run guide completion could not be saved; it will be shown again next run."));
	}
	CleanupRunGuide(true);
}

void UPGRunHUDComponent::HandlePauseRequested()
{
	ShowPauseScreen();
}

void UPGRunHUDComponent::HandleResumeRequested()
{
	CleanupPauseScreen(true);
}

void UPGRunHUDComponent::HandlePauseSettingsRequested()
{
	if (PauseWidget)
	{
		PauseWidget->SetPauseButtonsEnabled(false);
	}
	ShowSettingsScreen();
}

void UPGRunHUDComponent::HandlePauseHomeRequested()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!BoundGameMode)
	{
		return;
	}

	// Pause Home은 실패가 아니지만 현재 런을 종료하는 선택이므로 결과창 없이 영수증만 정산한다.
	if (!BoundGameMode->SettleCurrentRunRewards(TEXT("PauseHome")))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("Pause Home was blocked because run rewards could not be saved."));
		return;
	}

	CleanupSettingsScreen();

	if (PauseWidget)
	{
		PauseWidget->OnResumeRequested.RemoveAll(this);
		PauseWidget->OnSettingsRequested.RemoveAll(this);
		PauseWidget->OnHomeRequested.RemoveAll(this);
		PauseWidget->RemoveFromParent();
		PauseWidget = nullptr;
	}

	if (OwnerPlayerController)
	{
		if (UPGMusicComponent* MusicComponent = OwnerPlayerController->FindComponentByClass<UPGMusicComponent>())
		{
			MusicComponent->SetMusicDucked(false);
		}
	}

	UGameplayStatics::SetGamePaused(World, false);

	// Home 복귀는 현재 레벨 리로드로 처리한다. 러너/트랙/적재물/이벤트 상태를 가장 확실하게 초기화하기 위함이다.
	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	UGameplayStatics::OpenLevel(World, CurrentLevelName);
}

void UPGRunHUDComponent::HandleSettingsScreenClosed()
{
	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->OnSettingsScreenClosed.RemoveAll(this);
		SettingsScreenWidget = nullptr;
	}
	if (PauseWidget)
	{
		PauseWidget->SetPauseButtonsEnabled(true);
	}
}

void UPGRunHUDComponent::LoadSettings()
{
	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedRunHUDSettings = Settings->RunHUD;
		CachedHomeFlowSettings = Settings->HomeFlow;
		CargoDangerWarningRatio = Settings->Cargo.CargoDangerWarningRatio;
	}
}

bool UPGRunHUDComponent::AreRuntimeResourcesReady() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	return !PreloadSubsystem || PreloadSubsystem->IsGroupReady(EPGRuntimePreloadGroup::Run);
}

void UPGRunHUDComponent::BindGameMode()
{
	if (bIsBoundToGameMode || !GetWorld())
	{
		return;
	}

	BoundGameMode = GetWorld()->GetAuthGameMode<APGGameModeBase>();
	if (!BoundGameMode)
	{
		return;
	}

	BoundGameMode->OnRunCountdownStarted.AddDynamic(this, &UPGRunHUDComponent::HandleRunCountdownStarted);
	BoundGameMode->OnRunStarted.AddDynamic(this, &UPGRunHUDComponent::HandleRunStarted);
	BoundGameMode->OnRunFailed.AddDynamic(this, &UPGRunHUDComponent::HandleRunFailed);
	bIsBoundToGameMode = true;
}

void UPGRunHUDComponent::UnbindGameMode()
{
	if (BoundGameMode && bIsBoundToGameMode)
	{
		BoundGameMode->OnRunCountdownStarted.RemoveDynamic(this, &UPGRunHUDComponent::HandleRunCountdownStarted);
		BoundGameMode->OnRunStarted.RemoveDynamic(this, &UPGRunHUDComponent::HandleRunStarted);
		BoundGameMode->OnRunFailed.RemoveDynamic(this, &UPGRunHUDComponent::HandleRunFailed);
	}

	BoundGameMode = nullptr;
	bIsBoundToGameMode = false;
}

void UPGRunHUDComponent::BindRunProgress(UPGRunProgressComponent* RunProgressComponent)
{
	if (BoundRunProgressComponent == RunProgressComponent)
	{
		return;
	}

	UnbindRunProgress();
	BoundRunProgressComponent = RunProgressComponent;
	if (!BoundRunProgressComponent)
	{
		return;
	}

	BoundRunProgressComponent->OnDisplayedDistanceChanged.AddDynamic(this, &UPGRunHUDComponent::HandleDisplayedDistanceChanged);
	BoundRunProgressComponent->OnScoreChanged.AddDynamic(this, &UPGRunHUDComponent::HandleScoreChanged);
	BoundRunProgressComponent->OnReceiptsChanged.AddDynamic(this, &UPGRunHUDComponent::HandleReceiptsChanged);
}

void UPGRunHUDComponent::UnbindRunProgress()
{
	if (BoundRunProgressComponent)
	{
		BoundRunProgressComponent->OnDisplayedDistanceChanged.RemoveDynamic(this, &UPGRunHUDComponent::HandleDisplayedDistanceChanged);
		BoundRunProgressComponent->OnScoreChanged.RemoveDynamic(this, &UPGRunHUDComponent::HandleScoreChanged);
		BoundRunProgressComponent->OnReceiptsChanged.RemoveDynamic(this, &UPGRunHUDComponent::HandleReceiptsChanged);
	}
	BoundRunProgressComponent = nullptr;
}

void UPGRunHUDComponent::BindCargoStack(UPGCargoStackComponent* CargoStackComponent)
{
	if (BoundCargoStackComponent == CargoStackComponent)
	{
		return;
	}

	UnbindCargoStack();
	BoundCargoStackComponent = CargoStackComponent;
	if (BoundCargoStackComponent)
	{
		BoundCargoStackComponent->OnCargoListChanged.AddUObject(this, &UPGRunHUDComponent::HandleCargoListChanged);
		BoundCargoStackComponent->OnCargoDelivered.AddUObject(this, &UPGRunHUDComponent::HandleCargoDelivered);
	}
}

void UPGRunHUDComponent::UnbindCargoStack()
{
	if (BoundCargoStackComponent)
	{
		BoundCargoStackComponent->OnCargoListChanged.RemoveAll(this);
		BoundCargoStackComponent->OnCargoDelivered.RemoveAll(this);
	}
	BoundCargoStackComponent = nullptr;
}

void UPGRunHUDComponent::BindRunner(APGRunnerCharacter* RunnerCharacter)
{
	if (BoundRunnerCharacter.Get() == RunnerCharacter)
	{
		return;
	}

	UnbindRunner();
	BoundRunnerCharacter = RunnerCharacter;
	if (RunnerCharacter)
	{
		RunnerCharacter->OnJumpStarted.AddUObject(this, &UPGRunHUDComponent::HandleRunnerJumpStarted);
	}
}

void UPGRunHUDComponent::UnbindRunner()
{
	CargoDangerRingMaterial = nullptr;
	if (APGRunnerCharacter* Runner = BoundRunnerCharacter.Get())
	{
		Runner->OnJumpStarted.RemoveAll(this);
	}
	BoundRunnerCharacter = nullptr;
}

void UPGRunHUDComponent::ShowHUD()
{
	if (!OwnerPlayerController)
	{
		return;
	}

	if (HUDWidget)
	{
		// 결과창 뒤에도 HUD를 유지해야 하므로 실패 시에는 CleanupHUD가 아니라 상호작용만 잠근다.
		RefreshHUDValues();
		RefreshCargoList();
		UpdateHUDInteractionState();
		SetComponentTickEnabled(BoundGameMode && BoundGameMode->GetRunState() != EPGRunState::Failed);
		return;
	}

	if (!AreRuntimeResourcesReady())
	{
		// START 직후 예열이 아직 끝나지 않았다면 HUD를 늦게 만든다. 미로드 위젯을 동기 로드하지 않는다.
		return;
	}

	UClass* WidgetClass = CachedRunHUDSettings.RunHUDWidgetClass.Get();
	UClass* CargoItemClass = CachedRunHUDSettings.CargoDeliveryItemWidgetClass.Get();
	UClass* ScoreIndicatorClass = CachedRunHUDSettings.CargoScoreIndicatorWidgetClass.Get();
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGRunHUDWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("RunHUDWidgetClass must be assigned and inherit UPGRunHUDWidget."));
		return;
	}
	if (!CargoItemClass || !CargoItemClass->IsChildOf(UPGCargoDeliveryItemWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("CargoDeliveryItemWidgetClass must be assigned and inherit UPGCargoDeliveryItemWidget."));
		return;
	}

	HUDWidget = CreateWidget<UPGRunHUDWidget>(OwnerPlayerController, TSubclassOf<UPGRunHUDWidget>(WidgetClass));
	if (!HUDWidget)
	{
		return;
	}

	HUDWidget->SetCargoItemWidgetClass(TSubclassOf<UPGCargoDeliveryItemWidget>(CargoItemClass));
	if (ScoreIndicatorClass && ScoreIndicatorClass->IsChildOf(UPGCargoScoreIndicatorWidget::StaticClass()))
	{
		HUDWidget->SetCargoScoreIndicatorWidgetClass(TSubclassOf<UPGCargoScoreIndicatorWidget>(ScoreIndicatorClass));
	}
	HUDWidget->OnPauseRequested.AddUObject(this, &UPGRunHUDComponent::HandlePauseRequested);
	HUDWidget->AddToViewport(15);
	HUDWidget->PlayEnterAnimation();
	RefreshHUDValues();
	RefreshCargoList();
	UpdateHUDInteractionState();
	SetComponentTickEnabled(true);
}

void UPGRunHUDComponent::CleanupHUD()
{
	SetComponentTickEnabled(false);
	CargoRefreshElapsed = 0.0f;
	bIsCargoDangerActive = false;
	CleanupRunGuide(false);

	if (HUDWidget)
	{
		HUDWidget->OnPauseRequested.RemoveAll(this);
		HUDWidget->PlayExitAnimation();
		HUDWidget = nullptr;
	}
}

UPGUserDataSubsystem* UPGRunHUDComponent::GetUserDataSubsystem() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UPGUserDataSubsystem>() : nullptr;
}

void UPGRunHUDComponent::ShowRunGuideIfNeeded()
{
	if (RunGuideWidget || !OwnerPlayerController || !AreRuntimeResourcesReady())
	{
		return;
	}

	const UPGUserDataSubsystem* UserData = GetUserDataSubsystem();
	if (!UserData || UserData->IsRunGuideCompleted())
	{
		return;
	}

	UClass* WidgetClass = CachedRunHUDSettings.RunGuideWidgetClass.Get();
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGRunGuideWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("RunGuideWidgetClass must be assigned and inherit UPGRunGuideWidget."));
		return;
	}

	RunGuideWidget = CreateWidget<UPGRunGuideWidget>(OwnerPlayerController, TSubclassOf<UPGRunGuideWidget>(WidgetClass));
	if (!RunGuideWidget)
	{
		return;
	}

	// HUD(15) 위, Pause 모달(60) 아래에 둔다. 가이드는 입력을 받지 않으므로 Pause 버튼을 가리지 않는다.
	RunGuideWidget->AddToViewport(16);
	AdvanceRunGuide(EPGRunGuideStep::Drag);
}

void UPGRunHUDComponent::AdvanceRunGuide(EPGRunGuideStep NextStep)
{
	RunGuideStep = NextStep;
	if (RunGuideWidget)
	{
		RunGuideWidget->ShowStep(NextStep);
	}
}

void UPGRunHUDComponent::CleanupRunGuide(bool bFadeOut)
{
	if (RunGuideWidget)
	{
		if (bFadeOut)
		{
			RunGuideWidget->Dismiss();
		}
		else
		{
			RunGuideWidget->RemoveFromParent();
		}
		RunGuideWidget = nullptr;
	}
	RunGuideStep = EPGRunGuideStep::None;
}

void UPGRunHUDComponent::UpdateHUDInteractionState() const
{
	if (!HUDWidget)
	{
		return;
	}

	const bool bCanPause = BoundGameMode && BoundGameMode->GetRunState() == EPGRunState::Running;
	HUDWidget->SetPauseButtonEnabled(bCanPause);
}

void UPGRunHUDComponent::RefreshHUDValues()
{
	if (!HUDWidget || !BoundRunProgressComponent)
	{
		return;
	}

	HUDWidget->SetScore(BoundRunProgressComponent->GetScore());
	HUDWidget->SetDistanceMeters(BoundRunProgressComponent->GetDisplayedDistanceMeters());
	HUDWidget->SetReceipts(BoundRunProgressComponent->GetReceipts());
}

void UPGRunHUDComponent::RefreshCargoList()
{
	if (HUDWidget && BoundCargoStackComponent)
	{
		BoundCargoStackComponent->GetActiveCargoInfos(ActiveCargoInfoBuffer);
		HUDWidget->RefreshCargoList(ActiveCargoInfoBuffer);
	}
}

void UPGRunHUDComponent::UpdateCargoDangerWarning()
{
	if (!BoundCargoStackComponent)
	{
		return;
	}

	// 경고 해제는 진입보다 낮은 비율에서 풀어 경계값 근처에서 깜빡임과 경고음 반복을 막는다.
	constexpr float ExitHysteresis = 0.1f;
	const float DangerRatio = BoundCargoStackComponent->GetCargoDangerRatio();
	const float ThresholdRatio = bIsCargoDangerActive ? CargoDangerWarningRatio - ExitHysteresis : CargoDangerWarningRatio;
	const bool bShouldWarn = CargoDangerWarningRatio < 1.0f - KINDA_SMALL_NUMBER && DangerRatio >= ThresholdRatio;
	const bool bChanged = bShouldWarn != bIsCargoDangerActive;
	bIsCargoDangerActive = bShouldWarn;
	if (bChanged && bShouldWarn)
	{
		UPGAudioStatics::PlaySFX2D(this, CachedRunHUDSettings.CargoDangerWarningSound.Get());
	}

	// 경고 중에만 링을 갱신하고, 꺼지는 순간 한 번 0으로 내린다.
	if (!bShouldWarn && !bChanged)
	{
		return;
	}

	if (UMaterialInstanceDynamic* RingMaterial = ResolveCargoDangerRing())
	{
		// 경고 문턱에서 약하게 켜져 테두리에 닿을수록 밝아지고, 적재물이 밀려난 쪽만 빛난다.
		const float Intensity = bShouldWarn
			? FMath::GetMappedRangeValueClamped(FVector2D(CargoDangerWarningRatio, 1.0f), FVector2D(0.35f, 1.0f), DangerRatio)
			: 0.0f;
		const FVector2D Direction = BoundCargoStackComponent->GetCargoDangerDirection();
		RingMaterial->SetScalarParameterValue(CargoDangerRingIntensityParam, Intensity);
		RingMaterial->SetVectorParameterValue(CargoDangerRingDirectionParam, FLinearColor(Direction.X, Direction.Y, 0.0f, 0.0f));
	}
}

UMaterialInstanceDynamic* UPGRunHUDComponent::ResolveCargoDangerRing()
{
	if (CargoDangerRingMaterial.IsValid())
	{
		return CargoDangerRingMaterial.Get();
	}

	const AActor* BowlActor = BoundRunnerCharacter.IsValid() ? BoundRunnerCharacter->GetAttachedBowlActor() : nullptr;
	UStaticMeshComponent* RingComponent = BowlActor ? Cast<UStaticMeshComponent>(BowlActor->FindComponentByTag(UStaticMeshComponent::StaticClass(), CargoDangerRingTag)) : nullptr;
	if (!RingComponent)
	{
		return nullptr;
	}

	CargoDangerRingMaterial = RingComponent->CreateAndSetMaterialInstanceDynamic(0);
	return CargoDangerRingMaterial.Get();
}

void UPGRunHUDComponent::ShowPauseScreen()
{
	if (!OwnerPlayerController || PauseWidget || !AreRuntimeResourcesReady())
	{
		return;
	}

	if (BoundGameMode && BoundGameMode->GetRunState() != EPGRunState::Running)
	{
		// 카운트다운과 실패 상태에서는 HUD가 보여도 Pause 입력은 허용하지 않는다.
		return;
	}

	UClass* WidgetClass = CachedRunHUDSettings.RunPauseWidgetClass.Get();
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGRunPauseWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("RunPauseWidgetClass must be assigned and inherit UPGRunPauseWidget."));
		return;
	}

	PauseWidget = CreateWidget<UPGRunPauseWidget>(OwnerPlayerController, TSubclassOf<UPGRunPauseWidget>(WidgetClass));
	if (!PauseWidget)
	{
		return;
	}

	if (UPGMusicComponent* MusicComponent = OwnerPlayerController->FindComponentByClass<UPGMusicComponent>())
	{
		MusicComponent->SetMusicDucked(true);
	}

	PauseWidget->OnResumeRequested.AddUObject(this, &UPGRunHUDComponent::HandleResumeRequested);
	PauseWidget->OnSettingsRequested.AddUObject(this, &UPGRunHUDComponent::HandlePauseSettingsRequested);
	PauseWidget->OnHomeRequested.AddUObject(this, &UPGRunHUDComponent::HandlePauseHomeRequested);
	PauseWidget->AddToViewport(60);
	PauseWidget->PlayOpenAnimation(CachedRunHUDSettings.PauseBackdropBlurStrength, CachedRunHUDSettings.PauseBackdropDimAlpha);
	UGameplayStatics::SetGamePaused(GetWorld(), true);
	ApplyPauseInputMode();
}

void UPGRunHUDComponent::CleanupPauseScreen(bool bResumeGame)
{
	CleanupSettingsScreen();

	if (PauseWidget)
	{
		PauseWidget->OnResumeRequested.RemoveAll(this);
		PauseWidget->OnSettingsRequested.RemoveAll(this);
		PauseWidget->OnHomeRequested.RemoveAll(this);
		PauseWidget->RemoveFromParent();
		PauseWidget = nullptr;
	}

	if (bResumeGame)
	{
		// Resume은 Pause 위젯 정리 후에 게임을 풀어 입력 모드와 BGM duck 상태가 한 프레임 어긋나지 않게 한다.
		if (OwnerPlayerController)
		{
			if (UPGMusicComponent* MusicComponent = OwnerPlayerController->FindComponentByClass<UPGMusicComponent>())
			{
				MusicComponent->SetMusicDucked(false);
			}
		}

		UGameplayStatics::SetGamePaused(GetWorld(), false);
		ApplyGameInputMode();
	}
}

void UPGRunHUDComponent::ShowSettingsScreen()
{
	if (!OwnerPlayerController || SettingsScreenWidget)
	{
		return;
	}

	UClass* WidgetClass = CachedHomeFlowSettings.SettingsScreenWidgetClass.Get();
	if (!WidgetClass || !WidgetClass->IsChildOf(UPGSettingsScreenWidget::StaticClass()))
	{
		UE_LOG(LogPGRunHUDComponent, Warning, TEXT("SettingsScreenWidgetClass must be assigned and inherit UPGSettingsScreenWidget."));
		if (PauseWidget)
		{
			PauseWidget->SetPauseButtonsEnabled(true);
		}
		return;
	}

	SettingsScreenWidget = CreateWidget<UPGSettingsScreenWidget>(OwnerPlayerController, TSubclassOf<UPGSettingsScreenWidget>(WidgetClass));
	if (!SettingsScreenWidget)
	{
		if (PauseWidget)
		{
			PauseWidget->SetPauseButtonsEnabled(true);
		}
		return;
	}

	SettingsScreenWidget->OnSettingsScreenClosed.AddUObject(this, &UPGRunHUDComponent::HandleSettingsScreenClosed);
	SettingsScreenWidget->AddToViewport(70);
	SettingsScreenWidget->PlayOpenAnimation();
}

void UPGRunHUDComponent::CleanupSettingsScreen()
{
	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->OnSettingsScreenClosed.RemoveAll(this);
		SettingsScreenWidget->RemoveFromParent();
		SettingsScreenWidget = nullptr;
	}
}

void UPGRunHUDComponent::ApplyGameInputMode() const
{
	if (!OwnerPlayerController)
	{
		return;
	}

	OwnerPlayerController->SetInputMode(FInputModeGameOnly());
	OwnerPlayerController->SetShowMouseCursor(false);
}

void UPGRunHUDComponent::ApplyPauseInputMode() const
{
	if (!OwnerPlayerController)
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	OwnerPlayerController->SetInputMode(InputMode);
	OwnerPlayerController->SetShowMouseCursor(true);
}
