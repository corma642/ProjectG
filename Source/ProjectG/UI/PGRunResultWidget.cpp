// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunResultWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "UI/PGUIAnimationUtils.h"

namespace
{
constexpr float BackdropBlurStrength = 8.0f;
constexpr float BackdropDimOpacity = 0.45f;
constexpr float BackdropAnimationDuration = 0.45f;
constexpr float BackdropResultStartAlpha = 0.8f;
constexpr float RootPanelStartYOffset = -1000.0f;
constexpr float RowStartXOffset = -700.0f;
constexpr float ButtonStartYOffset = 420.0f;
constexpr float RootPanelAnimationDuration = 0.45f;
constexpr float BackgroundAnimationDuration = 0.35f;
constexpr float TitleAnimationDuration = 0.35f;
constexpr float TitleAnimationDelay = 0.16f;
constexpr float RowAnimationDuration = 0.28f;
constexpr float RowAnimationDelay = 0.09f;
constexpr float RowAnimationStartDelay = 0.26f;
constexpr float ButtonAnimationDuration = 0.28f;
constexpr float ButtonAnimationDelay = 0.12f;
constexpr float ButtonAnimationStartDelay = 0.3f;
constexpr float ButtonNormalScale = 1.0f;
constexpr float ButtonPressedScale = 0.96f;
constexpr float BackgroundStartScale = 0.96f;
constexpr float TitleStartScale = 1.35f;
constexpr float TitleEndScale = 1.0f;

}

void UPGRunResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// BP 레이아웃에서 개별로 바인딩한 행/버튼을 C++ 애니메이션 순서로 캐시한다.
	// 위젯 계층을 직접 생성하지 않고, 정산 정보가 읽히는 순서만 여기서 고정한다.
	AnimatedRows.Reset();
	AnimatedRows.Add(ScoreRow);
	AnimatedRows.Add(DistanceRow);
	AnimatedRows.Add(ReceiptsRow);
	AnimatedRows.Add(OrdersRow);

	AnimatedButtons.Reset();
	AnimatedButtons.Add(ContinueButtonPanel);
	AnimatedButtons.Add(RetryButtonPanel);
	AnimatedButtons.Add(HomeButtonPanel);

	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UPGRunResultWidget::HandleContinueClicked);
		ContinueButton->OnPressed.AddDynamic(this, &UPGRunResultWidget::HandleContinuePressed);
		ContinueButton->OnReleased.AddDynamic(this, &UPGRunResultWidget::HandleContinueReleased);
	}
	if (RetryButton)
	{
		// 결과 버튼은 진입 애니메이션이 끝날 때까지 비활성화한다.
		// Pressed 이벤트는 BP 버튼 스타일을 덮지 않고 눌림 스케일만 보정한다.
		RetryButton->OnClicked.AddDynamic(this, &UPGRunResultWidget::HandleRetryClicked);
		RetryButton->OnPressed.AddDynamic(this, &UPGRunResultWidget::HandleRetryPressed);
		RetryButton->OnReleased.AddDynamic(this, &UPGRunResultWidget::HandleRetryReleased);
	}
	if (HomeButton)
	{
		HomeButton->OnClicked.AddDynamic(this, &UPGRunResultWidget::HandleHomeClicked);
		HomeButton->OnPressed.AddDynamic(this, &UPGRunResultWidget::HandleHomePressed);
		HomeButton->OnReleased.AddDynamic(this, &UPGRunResultWidget::HandleHomeReleased);
	}

	ApplyCachedResultTexts();
	StartBackdropAnimation();
}

void UPGRunResultWidget::NativeDestruct()
{
	if (ContinueButton)
	{
		ContinueButton->OnClicked.RemoveDynamic(this, &UPGRunResultWidget::HandleContinueClicked);
		ContinueButton->OnPressed.RemoveDynamic(this, &UPGRunResultWidget::HandleContinuePressed);
		ContinueButton->OnReleased.RemoveDynamic(this, &UPGRunResultWidget::HandleContinueReleased);
	}
	if (RetryButton)
	{
		RetryButton->OnClicked.RemoveDynamic(this, &UPGRunResultWidget::HandleRetryClicked);
		RetryButton->OnPressed.RemoveDynamic(this, &UPGRunResultWidget::HandleRetryPressed);
		RetryButton->OnReleased.RemoveDynamic(this, &UPGRunResultWidget::HandleRetryReleased);
	}
	if (HomeButton)
	{
		HomeButton->OnClicked.RemoveDynamic(this, &UPGRunResultWidget::HandleHomeClicked);
		HomeButton->OnPressed.RemoveDynamic(this, &UPGRunResultWidget::HandleHomePressed);
		HomeButton->OnReleased.RemoveDynamic(this, &UPGRunResultWidget::HandleHomeReleased);
	}

	Super::NativeDestruct();
}

void UPGRunResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBackdropAnimationFinished)
	{
		UpdateBackdropAnimation(InDeltaTime);
	}

	if (bResultEntryStarted && !bEntryAnimationFinished)
	{
		UpdateEntryAnimation(InDeltaTime);
	}
}

void UPGRunResultWidget::ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score, int32 Receipts, int32 OrdersDelivered)
{
	CachedDistanceMeters = DistanceMeters;
	CachedScore = Score;
	CachedReceipts = Receipts;
	CachedOrdersDelivered = OrdersDelivered;

	ApplyCachedResultTexts();
	if (FailureReasonText)
	{
		FailureReasonText->SetText(GetFailureReasonText(FailureReason));
	}
	StartBackdropAnimation();
	OnRunResultConfigured(FailureReason, RunTimeSeconds, DistanceMeters, Score, Receipts, OrdersDelivered);
}

void UPGRunResultWidget::ConfigureContinueOption(bool bOffered, bool bAffordable, int32 ReceiptCost)
{
	bCanContinue = bOffered && bAffordable;
	if (ContinueButtonPanel)
	{
		ContinueButtonPanel->SetVisibility(bOffered ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ContinueButton)
	{
		// 잔액이 모자라면 비용은 보여 주되 버튼을 흐리게 잠가 왜 못 누르는지 드러낸다. 패널 투명도는 진입 애니메이션이 쓰므로 버튼 쪽에 건다.
		ContinueButton->SetRenderOpacity(bAffordable ? 1.0f : 0.5f);
		ContinueButton->SetIsEnabled(ContinueButton->GetIsEnabled() && bCanContinue);
	}
	if (ContinueCostText)
	{
		ContinueCostText->SetText(FormatNumber(ReceiptCost));
	}
}

FText UPGRunResultWidget::GetFailureReasonText(FName FailureReason)
{
	// 원인 이름은 러너가 GameMode에 넘기는 값(CargoLost/Fall)과 같다. 실패가 억울하지 않도록 무엇 때문에 끝났는지 한 줄로 알린다.
	if (FailureReason == TEXT("CargoLost"))
	{
		return NSLOCTEXT("PGRunResult", "FailureCargoLost", "머리 위 배달물이 떨어졌어요!");
	}
	if (FailureReason == TEXT("Fall"))
	{
		return NSLOCTEXT("PGRunResult", "FailureFall", "길 밖으로 떨어졌어요!");
	}
	return FText::GetEmpty();
}

void UPGRunResultWidget::ApplyCachedResultTexts()
{
	if (ScoreValueText)
	{
		ScoreValueText->SetText(FormatNumber(CachedScore));
	}
	if (DistanceValueText)
	{
		DistanceValueText->SetText(FText::Format(NSLOCTEXT("PGRunResult", "DistanceMetersFormat", "{0} m"), FormatNumber(FMath::RoundToInt(CachedDistanceMeters))));
	}
	if (ReceiptsValueText)
	{
		ReceiptsValueText->SetText(FormatNumber(CachedReceipts));
	}
	if (OrdersValueText)
	{
		OrdersValueText->SetText(FormatNumber(CachedOrdersDelivered));
	}
}

void UPGRunResultWidget::StartBackdropAnimation()
{
	BackdropAnimationElapsed = 0.0f;
	EntryAnimationElapsed = 0.0f;
	bBackdropAnimationFinished = false;
	bResultEntryStarted = false;
	bEntryAnimationFinished = true;

	// 실패 직후에는 먼저 게임 화면을 흐리고 어둡게 만든다.
	// 결과 패널은 이후 별도 타이밍에서 시작해 화면 전환이 갑자기 튀지 않게 한다.
	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(0.0f);
	}
	if (BackdropDim)
	{
		BackdropDim->SetRenderOpacity(0.0f);
	}

	ResetEntryAnimationState();
}

void UPGRunResultWidget::UpdateBackdropAnimation(float DeltaTime)
{
	BackdropAnimationElapsed += DeltaTime;

	const float Alpha = FMath::Clamp(BackdropAnimationElapsed / BackdropAnimationDuration, 0.0f, 1.0f);
	const float EasedAlpha = PGUIAnimation::EaseOutCubic(Alpha);

	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(BackdropBlurStrength * EasedAlpha);
	}
	if (BackdropDim)
	{
		BackdropDim->SetRenderOpacity(BackdropDimOpacity * EasedAlpha);
	}

	if (!bResultEntryStarted && Alpha >= BackdropResultStartAlpha)
	{
		// 블러/딤이 대부분 들어온 뒤 결과 패널을 시작한다.
		// HUD와 게임 화면이 먼저 뒤로 물러나야 결과창에 시선이 집중된다.
		StartEntryAnimation();
	}

	if (Alpha >= 1.0f)
	{
		bBackdropAnimationFinished = true;
	}
}

void UPGRunResultWidget::StartEntryAnimation()
{
	EntryAnimationElapsed = 0.0f;
	bResultEntryStarted = true;
	bEntryAnimationFinished = false;
	SetActionButtonsEnabled(false);
	ResetEntryAnimationState();
}

void UPGRunResultWidget::ResetEntryAnimationState()
{
	// 모든 결과 요소를 투명한 시작 위치로 되돌린다. 실패 직후 한 프레임이라도
	// BP의 최종 배치가 노출되면 애니메이션이 이미 끝난 것처럼 보인다.
	ApplyWidgetAnimationState(ResultRootPanel, FVector2D(0.0f, RootPanelStartYOffset), 0.0f);
	ApplyWidgetAnimationState(ResultPanel, FVector2D::ZeroVector, 0.0f);
	ApplyWidgetAnimationState(TitleText, FVector2D::ZeroVector, 0.0f);
	ApplyWidgetScale(ResultPanel, BackgroundStartScale);
	ApplyWidgetScale(TitleText, TitleStartScale);

	for (UWidget* RowWidget : AnimatedRows)
	{
		ApplyWidgetAnimationState(RowWidget, FVector2D(RowStartXOffset, 0.0f), 0.0f);
		ApplyWidgetScale(RowWidget, 1.0f);
	}

	for (UWidget* ButtonWidget : AnimatedButtons)
	{
		ApplyWidgetAnimationState(ButtonWidget, FVector2D(0.0f, ButtonStartYOffset), 0.0f);
	}

	SetActionButtonsEnabled(false);
	ApplyWidgetScale(ContinueButtonPanel ? ContinueButtonPanel.Get() : ContinueButton.Get(), ButtonNormalScale);
	ApplyWidgetScale(RetryButtonPanel ? RetryButtonPanel.Get() : RetryButton.Get(), ButtonNormalScale);
	ApplyWidgetScale(HomeButtonPanel ? HomeButtonPanel.Get() : HomeButton.Get(), ButtonNormalScale);
}

void UPGRunResultWidget::UpdateEntryAnimation(float DeltaTime)
{
	EntryAnimationElapsed += DeltaTime;

	const float RootAlpha = PGUIAnimation::EaseOutCubic(EntryAnimationElapsed / RootPanelAnimationDuration);
	ApplyWidgetAnimationState(ResultRootPanel, FVector2D(0.0f, FMath::Lerp(RootPanelStartYOffset, 0.0f, RootAlpha)), RootAlpha);

	const float BackgroundAlpha = PGUIAnimation::EaseOutCubic(EntryAnimationElapsed / BackgroundAnimationDuration);
	ApplyWidgetAnimationState(ResultPanel, FVector2D::ZeroVector, BackgroundAlpha);
	ApplyWidgetScale(ResultPanel, FMath::Lerp(BackgroundStartScale, 1.0f, BackgroundAlpha));

	const float TitleTime = EntryAnimationElapsed - TitleAnimationDelay;
	const float TitleAlpha = PGUIAnimation::EaseOutCubic(TitleTime / TitleAnimationDuration);
	ApplyWidgetAnimationState(TitleText, FVector2D::ZeroVector, TitleAlpha);
	ApplyWidgetScale(TitleText, FMath::Lerp(TitleStartScale, TitleEndScale, TitleAlpha));

	bool bAllRowsFinished = true;
	for (int32 Index = 0; Index < AnimatedRows.Num(); ++Index)
	{
		// 점수 행은 위에서 아래 순서로 차례대로 들어온다.
		// 각 행이 같은 방향으로 움직이면 결과 정산이 누적되는 느낌이 난다.
		const float RowTime = EntryAnimationElapsed - RowAnimationStartDelay - static_cast<float>(Index) * RowAnimationDelay;
		const float RowAlpha = PGUIAnimation::EaseOutCubic(RowTime / RowAnimationDuration);
		bAllRowsFinished &= RowAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedRows[Index], FVector2D(FMath::Lerp(RowStartXOffset, 0.0f, RowAlpha), 0.0f), RowAlpha);
	}

	bool bAllButtonsFinished = true;
	for (int32 Index = 0; Index < AnimatedButtons.Num(); ++Index)
	{
		// Retry/Home은 결과 행과 거의 같이 올라온다(2026-10-18 재도전 대기 단축으로 0.72초에서 0.3초로).
		// 애니메이션 완료 전 입력을 막아 투명 버튼 클릭을 방지한다.
		const float ButtonTime = EntryAnimationElapsed - ButtonAnimationStartDelay - static_cast<float>(Index) * ButtonAnimationDelay;
		const float ButtonAlpha = PGUIAnimation::EaseOutCubic(ButtonTime / ButtonAnimationDuration);
		bAllButtonsFinished &= ButtonAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedButtons[Index], FVector2D(0.0f, FMath::Lerp(ButtonStartYOffset, 0.0f, ButtonAlpha)), ButtonAlpha);
	}

	if (bAllButtonsFinished && RetryButton && !RetryButton->GetIsEnabled())
	{
		// 행 애니메이션이 끝나기를 기다리지 않고 버튼이 다 보이는 즉시 재도전을 받는다.
		SetActionButtonsEnabled(true);
	}

	if (RootAlpha >= 1.0f && BackgroundAlpha >= 1.0f && TitleAlpha >= 1.0f && bAllRowsFinished && bAllButtonsFinished)
	{
		bEntryAnimationFinished = true;
	}
}

void UPGRunResultWidget::ApplyWidgetAnimationState(UWidget* Widget, const FVector2D& Offset, float Opacity) const
{
	if (Widget)
	{
		Widget->SetRenderTranslation(Offset);
		Widget->SetRenderOpacity(FMath::Clamp(Opacity, 0.0f, 1.0f));
	}
}

void UPGRunResultWidget::SetActionButtonsEnabled(bool bEnabled) const
{
	if (ContinueButton)
	{
		ContinueButton->SetIsEnabled(bEnabled && bCanContinue);
	}
	if (RetryButton)
	{
		RetryButton->SetIsEnabled(bEnabled);
	}
	if (HomeButton)
	{
		HomeButton->SetIsEnabled(bEnabled);
	}
}

void UPGRunResultWidget::ApplyWidgetScale(UWidget* Widget, float Scale) const
{
	if (Widget)
	{
		const float ClampedScale = FMath::Max(0.01f, Scale);
		Widget->SetRenderScale(FVector2D(ClampedScale, ClampedScale));
	}
}

FText UPGRunResultWidget::FormatNumber(int32 Value) const
{
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.UseGrouping = true;
	FormattingOptions.MaximumFractionalDigits = 0;
	return FText::AsNumber(Value, &FormattingOptions);
}

void UPGRunResultWidget::HandleContinueClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		// 재개가 시작되면 결과 화면은 이어하기 카운트다운 delegate를 받은 RunResultFeedbackComponent가 닫는다.
		GameMode->RequestContinueRun();
	}
}

void UPGRunResultWidget::HandleContinuePressed()
{
	ApplyWidgetScale(ContinueButtonPanel ? ContinueButtonPanel.Get() : ContinueButton.Get(), ButtonPressedScale);
}

void UPGRunResultWidget::HandleContinueReleased()
{
	ApplyWidgetScale(ContinueButtonPanel ? ContinueButtonPanel.Get() : ContinueButton.Get(), ButtonNormalScale);
}

void UPGRunResultWidget::HandleRetryClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		if (!GameMode->SettleCurrentRunRewards(TEXT("ResultRetry")))
		{
			return;
		}
	}
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (!CurrentLevelName.IsEmpty())
	{
		// Retry는 같은 레벨을 다시 열되 Home을 건너뛰고 카운트다운부터 시작한다.
		// 월드 Actor는 다시 만들지만 이미 준비한 Run 에셋은 한 번 재사용해 재구축 대기를 줄인다.
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>())
			{
				PreloadSubsystem->PreserveRunResourcesForNextSameMapTravel(FName(*CurrentLevelName));
			}
		}
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName), true, TEXT("StartFlow=RetryCountdown"));
	}
}

void UPGRunResultWidget::HandleRetryPressed()
{
	ApplyWidgetScale(RetryButtonPanel ? RetryButtonPanel.Get() : RetryButton.Get(), ButtonPressedScale);
}

void UPGRunResultWidget::HandleRetryReleased()
{
	ApplyWidgetScale(RetryButtonPanel ? RetryButtonPanel.Get() : RetryButton.Get(), ButtonNormalScale);
}

void UPGRunResultWidget::HandleHomeClicked()
{
	UPGAudioStatics::PlayCommonButtonClick(this);
	if (APGGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APGGameModeBase>() : nullptr)
	{
		if (!GameMode->SettleCurrentRunRewards(TEXT("ResultHome")))
		{
			return;
		}
	}
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (!CurrentLevelName.IsEmpty())
	{
		// Home은 URL 옵션 없이 열어 앱 최초 진입과 같은 Home 상태로 복구한다.
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
	}
}

void UPGRunResultWidget::HandleHomePressed()
{
	ApplyWidgetScale(HomeButtonPanel ? HomeButtonPanel.Get() : HomeButton.Get(), ButtonPressedScale);
}

void UPGRunResultWidget::HandleHomeReleased()
{
	ApplyWidgetScale(HomeButtonPanel ? HomeButtonPanel.Get() : HomeButton.Get(), ButtonNormalScale);
}

bool UPGRunResultWidget::RequestHome()
{
	if (!HomeButton || !HomeButton->GetIsEnabled())
	{
		return false;
	}

	HandleHomeClicked();
	return true;
}
