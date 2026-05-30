// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunResultWidget.h"

#include "Audio/PGAudioStatics.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"

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
constexpr float ButtonAnimationStartDelay = 0.72f;
constexpr float ButtonNormalScale = 1.0f;
constexpr float ButtonPressedScale = 0.96f;
constexpr float BackgroundStartScale = 0.96f;
constexpr float TitleStartScale = 1.35f;
constexpr float TitleEndScale = 1.0f;

float RunResultEaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}
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
	AnimatedButtons.Add(RetryButtonPanel);
	AnimatedButtons.Add(HomeButtonPanel);

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
	CachedFailureReason = FailureReason;
	CachedRunTimeSeconds = RunTimeSeconds;
	CachedDistanceMeters = DistanceMeters;
	CachedScore = Score;
	CachedReceipts = Receipts;
	CachedOrdersDelivered = OrdersDelivered;

	ApplyCachedResultTexts();
	StartBackdropAnimation();
	OnRunResultConfigured(FailureReason, RunTimeSeconds, DistanceMeters, Score, Receipts, OrdersDelivered);
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
	const float EasedAlpha = RunResultEaseOutCubic(Alpha);

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
	ApplyWidgetScale(RetryButtonPanel ? RetryButtonPanel.Get() : RetryButton.Get(), ButtonNormalScale);
	ApplyWidgetScale(HomeButtonPanel ? HomeButtonPanel.Get() : HomeButton.Get(), ButtonNormalScale);
}

void UPGRunResultWidget::UpdateEntryAnimation(float DeltaTime)
{
	EntryAnimationElapsed += DeltaTime;

	const float RootAlpha = RunResultEaseOutCubic(EntryAnimationElapsed / RootPanelAnimationDuration);
	ApplyWidgetAnimationState(ResultRootPanel, FVector2D(0.0f, FMath::Lerp(RootPanelStartYOffset, 0.0f, RootAlpha)), RootAlpha);

	const float BackgroundAlpha = RunResultEaseOutCubic(EntryAnimationElapsed / BackgroundAnimationDuration);
	ApplyWidgetAnimationState(ResultPanel, FVector2D::ZeroVector, BackgroundAlpha);
	ApplyWidgetScale(ResultPanel, FMath::Lerp(BackgroundStartScale, 1.0f, BackgroundAlpha));

	const float TitleTime = EntryAnimationElapsed - TitleAnimationDelay;
	const float TitleAlpha = RunResultEaseOutCubic(TitleTime / TitleAnimationDuration);
	ApplyWidgetAnimationState(TitleText, FVector2D::ZeroVector, TitleAlpha);
	ApplyWidgetScale(TitleText, FMath::Lerp(TitleStartScale, TitleEndScale, TitleAlpha));

	bool bAllRowsFinished = true;
	for (int32 Index = 0; Index < AnimatedRows.Num(); ++Index)
	{
		// 점수 행은 위에서 아래 순서로 차례대로 들어온다.
		// 각 행이 같은 방향으로 움직이면 결과 정산이 누적되는 느낌이 난다.
		const float RowTime = EntryAnimationElapsed - RowAnimationStartDelay - static_cast<float>(Index) * RowAnimationDelay;
		const float RowAlpha = RunResultEaseOutCubic(RowTime / RowAnimationDuration);
		bAllRowsFinished &= RowAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedRows[Index], FVector2D(FMath::Lerp(RowStartXOffset, 0.0f, RowAlpha), 0.0f), RowAlpha);
	}

	bool bAllButtonsFinished = true;
	for (int32 Index = 0; Index < AnimatedButtons.Num(); ++Index)
	{
		// Retry/Home은 결과값이 모두 보인 뒤 마지막 행동 선택지로 등장한다.
		// 애니메이션 완료 전 입력을 막아 투명 버튼 클릭을 방지한다.
		const float ButtonTime = EntryAnimationElapsed - ButtonAnimationStartDelay - static_cast<float>(Index) * ButtonAnimationDelay;
		const float ButtonAlpha = RunResultEaseOutCubic(ButtonTime / ButtonAnimationDuration);
		bAllButtonsFinished &= ButtonAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedButtons[Index], FVector2D(0.0f, FMath::Lerp(ButtonStartYOffset, 0.0f, ButtonAlpha)), ButtonAlpha);
	}

	if (RootAlpha >= 1.0f && BackgroundAlpha >= 1.0f && TitleAlpha >= 1.0f && bAllRowsFinished && bAllButtonsFinished)
	{
		bEntryAnimationFinished = true;
		SetActionButtonsEnabled(true);
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
		// 현재는 레벨 리로드가 가장 확실한 런 상태 초기화 경로다.
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
