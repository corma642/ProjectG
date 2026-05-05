// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PGRunResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/PGGameSettings.h"
#include "Styling/SlateTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGRunResultWidget, Log, All);

namespace
{
constexpr float PanelStartYOffset = -1000.0f;
constexpr float RowStartXOffset = -700.0f;
constexpr float ButtonStartYOffset = 420.0f;
constexpr float PanelAnimationDuration = 0.45f;
constexpr float RowAnimationDuration = 0.28f;
constexpr float RowAnimationDelay = 0.09f;
constexpr float ButtonAnimationDuration = 0.28f;
constexpr float ButtonAnimationDelay = 0.12f;
constexpr float ButtonAnimationStartDelay = 0.55f;

float EaseOutCubic(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return 1.0f - FMath::Pow(1.0f - ClampedAlpha, 3.0f);
}

FSlateBrush MakeColorBrush(const FLinearColor& Color, const FVector2D& ImageSize = FVector2D(64.0f, 64.0f))
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(Color);
	Brush.ImageSize = ImageSize;
	return Brush;
}
}

UPGRunResultWidget::UPGRunResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitializeDefaultStyle();
}

void UPGRunResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildDefaultLayout();
	StartBackdropAnimation();
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

void UPGRunResultWidget::ConfigureRunResult(FName FailureReason, float RunTimeSeconds, float DistanceMeters, int32 Score)
{
	BuildDefaultLayout();

	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	const int32 Coins = Settings ? Settings->RunResult.TemporaryCoins : 2366;
	const int32 OrdersDelivered = Settings ? Settings->RunResult.TemporaryOrdersDelivered : 3;

	if (ScoreValueText)
	{
		ScoreValueText->SetText(FormatNumber(Score));
	}
	if (DistanceValueText)
	{
		DistanceValueText->SetText(FText::Format(NSLOCTEXT("PGRunResult", "DistanceMetersFormat", "{0} m"), FormatNumber(FMath::RoundToInt(DistanceMeters))));
	}
	if (ReceiptsValueText)
	{
		ReceiptsValueText->SetText(FormatNumber(Coins));
	}
	if (OrdersValueText)
	{
		OrdersValueText->SetText(FormatNumber(OrdersDelivered));
	}

	StartBackdropAnimation();
	OnRunResultConfigured(FailureReason, RunTimeSeconds, DistanceMeters, Score, Coins, OrdersDelivered);
}

void UPGRunResultWidget::InitializeDefaultStyle()
{
	BackgroundBrush = MakeColorBrush(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), FVector2D(600.0f, 1200.0f));
	ScoreIconBrush = MakeColorBrush(FLinearColor(0.05f, 0.22f, 0.36f, 1.0f));
	DistanceIconBrush = MakeColorBrush(FLinearColor(0.04f, 0.28f, 0.54f, 1.0f));
	ReceiptsIconBrush = MakeColorBrush(FLinearColor(0.95f, 0.60f, 0.08f, 1.0f));
	OrdersIconBrush = MakeColorBrush(FLinearColor(0.55f, 0.28f, 0.07f, 1.0f));
	RetryIconBrush = MakeColorBrush(FLinearColor(0.72f, 0.18f, 0.05f, 1.0f));
	HomeIconBrush = MakeColorBrush(FLinearColor(0.02f, 0.22f, 0.48f, 1.0f));

	TitleFont.Size = 46;
	LabelFont.Size = 24;
	ValueFont.Size = 32;
	ButtonFont.Size = 30;

	TitleColor = FSlateColor(FLinearColor(0.70f, 0.12f, 0.03f, 1.0f));
	LabelColor = FSlateColor(FLinearColor(0.07f, 0.05f, 0.03f, 1.0f));
	ValueColor = FSlateColor(FLinearColor(0.07f, 0.05f, 0.03f, 1.0f));
	RetryButtonColor = FSlateColor(FLinearColor(0.70f, 0.12f, 0.03f, 1.0f));
	HomeButtonColor = FSlateColor(FLinearColor(0.02f, 0.20f, 0.43f, 1.0f));
	ButtonNormalBrush = MakeColorBrush(FLinearColor(0.86f, 0.78f, 0.62f, 0.95f), FVector2D(460.0f, 96.0f));
	ButtonHoveredBrush = MakeColorBrush(FLinearColor(0.78f, 0.68f, 0.50f, 1.0f), FVector2D(460.0f, 96.0f));
	ButtonPressedBrush = MakeColorBrush(FLinearColor(0.62f, 0.50f, 0.34f, 1.0f), FVector2D(460.0f, 96.0f));
	ButtonNormalTint = FSlateColor(FLinearColor(0.86f, 0.78f, 0.62f, 0.95f));
	ButtonHoveredTint = FSlateColor(FLinearColor(0.78f, 0.68f, 0.50f, 1.0f));
	ButtonPressedTint = FSlateColor(FLinearColor(0.62f, 0.50f, 0.34f, 1.0f));
	ButtonNormalScale = 1.0f;
	ButtonHoveredScale = 1.0f;
	ButtonPressedScale = 0.96f;
	BackdropBlurStrength = 8.0f;
	BackdropTintColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);
	BackdropAnimationDuration = 0.45f;
	BackdropResultStartAlpha = 0.8f;
	PanelDesignSize = FVector2D(600.0f, 1200.0f);
	PanelViewportPadding = FMargin(16.0f);
	PanelContentPadding = FMargin(64.0f, 130.0f, 64.0f, 95.0f);
	TitleBottomPadding = 90.0f;
	RowsBottomPadding = 100.0f;
	ResultRowBottomPadding = 50.0f;
	ButtonBottomPadding = 24.0f;
	ActionButtonHeight = 96.0f;
	ResultRowIconSize = FVector2D(56.0f, 56.0f);
	ResultRowIconLabelSpacing = 24.0f;
	ResultRowLabelValueMinSpacing = 12.0f;
	ActionButtonIconSize = FVector2D(54.0f, 54.0f);
	ActionButtonHorizontalPadding = 20.0f;
	ActionButtonVerticalPadding = 8.0f;
	ActionButtonIconTextSpacing = 24.0f;
}

void UPGRunResultWidget::BuildDefaultLayout()
{
	if (bLayoutBuilt || !WidgetTree)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RunResultRoot"));
	WidgetTree->RootWidget = RootCanvas;

	BackdropBlur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("RunResultBackdropBlur"));
	BackdropBlur->SetBlurStrength(0.0f);
	BackdropBlur->SetVisibility(ESlateVisibility::HitTestInvisible);
	RootCanvas->AddChildToCanvas(BackdropBlur);
	if (UCanvasPanelSlot* BlurSlot = Cast<UCanvasPanelSlot>(BackdropBlur->Slot))
	{
		BlurSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BlurSlot->SetOffsets(FMargin(0.0f));
	}

	BackdropDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunResultBackdropDim"));
	BackdropDim->SetBrush(MakeColorBrush(FLinearColor::Black));
	BackdropDim->SetBrushColor(FLinearColor(BackdropTintColor.R, BackdropTintColor.G, BackdropTintColor.B, 0.0f));
	BackdropDim->SetVisibility(ESlateVisibility::HitTestInvisible);
	RootCanvas->AddChildToCanvas(BackdropDim);
	if (UCanvasPanelSlot* DimSlot = Cast<UCanvasPanelSlot>(BackdropDim->Slot))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	PanelScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("RunResultScaleBox"));
	PanelScaleBox->SetStretch(EStretch::ScaleToFit);
	PanelScaleBox->SetStretchDirection(EStretchDirection::Both);
	RootCanvas->AddChildToCanvas(PanelScaleBox);
	if (UCanvasPanelSlot* ScaleSlot = Cast<UCanvasPanelSlot>(PanelScaleBox->Slot))
	{
		ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ScaleSlot->SetOffsets(PanelViewportPadding);
	}

	USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RunResultDesignSize"));
	PanelSizeBox->SetWidthOverride(FMath::Max(1.0f, PanelDesignSize.X));
	PanelSizeBox->SetHeightOverride(FMath::Max(1.0f, PanelDesignSize.Y));
	PanelScaleBox->SetContent(PanelSizeBox);
	if (UScaleBoxSlot* ScaleBoxSlot = Cast<UScaleBoxSlot>(PanelSizeBox->Slot))
	{
		ScaleBoxSlot->SetHorizontalAlignment(HAlign_Center);
		ScaleBoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	BackgroundPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunResultBackground"));
	BackgroundPanel->SetBrush(BackgroundBrush);
	BackgroundPanel->SetPadding(PanelContentPadding);
	PanelSizeBox->SetContent(BackgroundPanel);

	UVerticalBox* PanelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RunResultPanelBox"));
	BackgroundPanel->SetContent(PanelBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeliveryFailedTitle"));
	TitleText->SetText(NSLOCTEXT("PGRunResult", "DeliveryFailed", "DELIVERY FAILED"));
	TitleText->SetFont(TitleFont);
	TitleText->SetColorAndOpacity(TitleColor);
	TitleText->SetJustification(ETextJustify::Center);
	PanelBox->AddChildToVerticalBox(TitleText);
	if (UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(TitleText->Slot))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TitleBottomPadding));
	}

	ResultRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RunResultRows"));
	PanelBox->AddChildToVerticalBox(ResultRowsBox);
	if (UVerticalBoxSlot* RowsSlot = Cast<UVerticalBoxSlot>(ResultRowsBox->Slot))
	{
		RowsSlot->SetHorizontalAlignment(HAlign_Fill);
		RowsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowsBottomPadding));
		RowsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	ScoreValueText = CreateResultRow(ResultRowsBox, ScoreIconBrush, NSLOCTEXT("PGRunResult", "ScoreLabel", "Score"));
	DistanceValueText = CreateResultRow(ResultRowsBox, DistanceIconBrush, NSLOCTEXT("PGRunResult", "DistanceLabel", "Distance"));
	ReceiptsValueText = CreateResultRow(ResultRowsBox, ReceiptsIconBrush, NSLOCTEXT("PGRunResult", "ReceiptsLabel", "Receipts"));
	OrdersValueText = CreateResultRow(ResultRowsBox, OrdersIconBrush, NSLOCTEXT("PGRunResult", "OrdersLabel", "Orders Delivered"));

	RetryButton = CreateActionButton(PanelBox, RetryIconBrush, NSLOCTEXT("PGRunResult", "RetryDelivery", "RETRY DELIVERY"), RetryButtonColor);
	HomeButton = CreateActionButton(PanelBox, HomeIconBrush, NSLOCTEXT("PGRunResult", "Home", "HOME"), HomeButtonColor);

	if (RetryButton)
	{
		RetryButton->OnClicked.AddDynamic(this, &UPGRunResultWidget::HandleRetryClicked);
		RetryButton->OnHovered.AddDynamic(this, &UPGRunResultWidget::HandleRetryHovered);
		RetryButton->OnUnhovered.AddDynamic(this, &UPGRunResultWidget::HandleRetryUnhovered);
		RetryButton->OnPressed.AddDynamic(this, &UPGRunResultWidget::HandleRetryPressed);
		RetryButton->OnReleased.AddDynamic(this, &UPGRunResultWidget::HandleRetryReleased);
		RetryButton->SetIsEnabled(false);
	}
	if (HomeButton)
	{
		HomeButton->OnClicked.AddDynamic(this, &UPGRunResultWidget::HandleHomeClicked);
		HomeButton->OnHovered.AddDynamic(this, &UPGRunResultWidget::HandleHomeHovered);
		HomeButton->OnUnhovered.AddDynamic(this, &UPGRunResultWidget::HandleHomeUnhovered);
		HomeButton->OnPressed.AddDynamic(this, &UPGRunResultWidget::HandleHomePressed);
		HomeButton->OnReleased.AddDynamic(this, &UPGRunResultWidget::HandleHomeReleased);
		HomeButton->SetIsEnabled(false);
	}

	bLayoutBuilt = true;
}

void UPGRunResultWidget::StartBackdropAnimation()
{
	BackdropAnimationElapsed = 0.0f;
	EntryAnimationElapsed = 0.0f;
	bBackdropAnimationFinished = false;
	bResultEntryStarted = false;
	bEntryAnimationFinished = true;

	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(0.0f);
	}
	if (BackdropDim)
	{
		BackdropDim->SetBrushColor(FLinearColor(BackdropTintColor.R, BackdropTintColor.G, BackdropTintColor.B, 0.0f));
	}

	ResetEntryAnimationOffsets();
}

void UPGRunResultWidget::UpdateBackdropAnimation(float DeltaTime)
{
	BackdropAnimationElapsed += DeltaTime;

	const float Duration = FMath::Max(BackdropAnimationDuration, KINDA_SMALL_NUMBER);
	const float BackdropAlpha = FMath::Clamp(BackdropAnimationElapsed / Duration, 0.0f, 1.0f);
	const float EasedAlpha = EaseOutCubic(BackdropAlpha);

	if (BackdropBlur)
	{
		BackdropBlur->SetBlurStrength(FMath::Lerp(0.0f, FMath::Max(0.0f, BackdropBlurStrength), EasedAlpha));
	}
	if (BackdropDim)
	{
		BackdropDim->SetBrushColor(FLinearColor(BackdropTintColor.R, BackdropTintColor.G, BackdropTintColor.B, BackdropTintColor.A * EasedAlpha));
	}

	if (!bResultEntryStarted && BackdropAlpha >= FMath::Clamp(BackdropResultStartAlpha, 0.0f, 1.0f))
	{
		StartEntryAnimation();
	}

	if (BackdropAlpha >= 1.0f)
	{
		bBackdropAnimationFinished = true;
	}
}

UTextBlock* UPGRunResultWidget::CreateResultRow(UVerticalBox* ParentBox, const FSlateBrush& IconBrush, const FText& LabelText)
{
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ParentBox->AddChildToVerticalBox(RowBox);
	if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(RowBox->Slot))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, ResultRowBottomPadding));
	}
	AnimatedRows.Add(RowBox);

	USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSizeBox->SetWidthOverride(FMath::Max(1.0f, ResultRowIconSize.X));
	IconSizeBox->SetHeightOverride(FMath::Max(1.0f, ResultRowIconSize.Y));
	RowBox->AddChildToHorizontalBox(IconSizeBox);

	UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	IconImage->SetBrush(IconBrush);
	IconSizeBox->SetContent(IconImage);

	UTextBlock* LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelTextBlock->SetText(LabelText);
	LabelTextBlock->SetFont(LabelFont);
	LabelTextBlock->SetColorAndOpacity(LabelColor);
	RowBox->AddChildToHorizontalBox(LabelTextBlock);
	if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(LabelTextBlock->Slot))
	{
		LabelSlot->SetPadding(FMargin(ResultRowIconLabelSpacing, 0.0f, ResultRowLabelValueMinSpacing, 0.0f));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTextBlock* ValueTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ValueTextBlock->SetText(FText::FromString(TEXT("0")));
	ValueTextBlock->SetFont(ValueFont);
	ValueTextBlock->SetColorAndOpacity(ValueColor);
	ValueTextBlock->SetJustification(ETextJustify::Right);
	RowBox->AddChildToHorizontalBox(ValueTextBlock);
	if (UHorizontalBoxSlot* ValueSlot = Cast<UHorizontalBoxSlot>(ValueTextBlock->Slot))
	{
		ValueSlot->SetVerticalAlignment(VAlign_Center);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	return ValueTextBlock;
}

UButton* UPGRunResultWidget::CreateActionButton(UVerticalBox* ParentBox, const FSlateBrush& IconBrush, const FText& ButtonText, const FSlateColor& TextColor)
{
	USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (ActionButtonHeight > 0.0f)
	{
		ButtonSizeBox->SetHeightOverride(ActionButtonHeight);
	}

	ParentBox->AddChildToVerticalBox(ButtonSizeBox);
	if (UVerticalBoxSlot* ButtonSlot = Cast<UVerticalBoxSlot>(ButtonSizeBox->Slot))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, ButtonBottomPadding));
	}
	AnimatedButtons.Add(ButtonSizeBox);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle ButtonStyle;
	FSlateBrush NormalBrush = ButtonNormalBrush;
	FSlateBrush HoveredBrush = ButtonHoveredBrush;
	FSlateBrush PressedBrush = ButtonPressedBrush;
	NormalBrush.TintColor = ButtonNormalTint;
	HoveredBrush.TintColor = ButtonHoveredTint;
	PressedBrush.TintColor = ButtonPressedTint;
	ButtonStyle.SetNormal(NormalBrush);
	ButtonStyle.SetHovered(HoveredBrush);
	ButtonStyle.SetPressed(PressedBrush);
	ButtonStyle.SetDisabled(NormalBrush);
	Button->SetStyle(ButtonStyle);
	ApplyButtonScale(Button, ButtonNormalScale);
	ButtonSizeBox->SetContent(Button);

	UHorizontalBox* ButtonContent = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Button->SetContent(ButtonContent);

	USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSizeBox->SetWidthOverride(FMath::Max(1.0f, ActionButtonIconSize.X));
	IconSizeBox->SetHeightOverride(FMath::Max(1.0f, ActionButtonIconSize.Y));
	ButtonContent->AddChildToHorizontalBox(IconSizeBox);
	if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(IconSizeBox->Slot))
	{
		IconSlot->SetPadding(FMargin(ActionButtonHorizontalPadding, ActionButtonVerticalPadding, ActionButtonIconTextSpacing, ActionButtonVerticalPadding));
		IconSlot->SetHorizontalAlignment(HAlign_Left);
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	IconImage->SetBrush(IconBrush);
	IconSizeBox->SetContent(IconImage);

	UTextBlock* ButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonTextBlock->SetText(ButtonText);
	ButtonTextBlock->SetFont(ButtonFont);
	ButtonTextBlock->SetColorAndOpacity(TextColor);
	ButtonTextBlock->SetJustification(ETextJustify::Center);
	ButtonContent->AddChildToHorizontalBox(ButtonTextBlock);
	if (UHorizontalBoxSlot* TextSlot = Cast<UHorizontalBoxSlot>(ButtonTextBlock->Slot))
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetHorizontalAlignment(HAlign_Fill);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	return Button;
}

void UPGRunResultWidget::StartEntryAnimation()
{
	EntryAnimationElapsed = 0.0f;
	bResultEntryStarted = true;
	bEntryAnimationFinished = false;
	SetActionButtonsEnabled(false);

	ResetEntryAnimationOffsets();
}

void UPGRunResultWidget::ResetEntryAnimationOffsets()
{
	ApplyWidgetAnimationState(BackgroundPanel, FVector2D(0.0f, PanelStartYOffset), 0.0f);
	for (const TWeakObjectPtr<UWidget>& RowWidget : AnimatedRows)
	{
		ApplyWidgetAnimationState(RowWidget.Get(), FVector2D(RowStartXOffset, 0.0f), 0.0f);
	}
	for (const TWeakObjectPtr<UWidget>& ButtonWidget : AnimatedButtons)
	{
		ApplyWidgetAnimationState(ButtonWidget.Get(), FVector2D(0.0f, ButtonStartYOffset), 0.0f);
	}
	SetActionButtonsEnabled(false);
}

void UPGRunResultWidget::UpdateEntryAnimation(float DeltaTime)
{
	EntryAnimationElapsed += DeltaTime;

	const float PanelAlpha = EaseOutCubic(EntryAnimationElapsed / PanelAnimationDuration);
	ApplyWidgetAnimationState(BackgroundPanel, FVector2D(0.0f, FMath::Lerp(PanelStartYOffset, 0.0f, PanelAlpha)), PanelAlpha);

	bool bAllRowsFinished = true;
	for (int32 Index = 0; Index < AnimatedRows.Num(); ++Index)
	{
		const float RowTime = EntryAnimationElapsed - static_cast<float>(Index) * RowAnimationDelay;
		const float RowAlpha = EaseOutCubic(RowTime / RowAnimationDuration);
		bAllRowsFinished &= RowAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedRows[Index].Get(), FVector2D(FMath::Lerp(RowStartXOffset, 0.0f, RowAlpha), 0.0f), RowAlpha);
	}

	bool bAllButtonsFinished = true;
	for (int32 Index = 0; Index < AnimatedButtons.Num(); ++Index)
	{
		const float ButtonTime = EntryAnimationElapsed - ButtonAnimationStartDelay - static_cast<float>(Index) * ButtonAnimationDelay;
		const float ButtonAlpha = EaseOutCubic(ButtonTime / ButtonAnimationDuration);
		bAllButtonsFinished &= ButtonAlpha >= 1.0f;
		ApplyWidgetAnimationState(AnimatedButtons[Index].Get(), FVector2D(0.0f, FMath::Lerp(ButtonStartYOffset, 0.0f, ButtonAlpha)), ButtonAlpha);
	}

	if (PanelAlpha >= 1.0f && bAllRowsFinished && bAllButtonsFinished)
	{
		bEntryAnimationFinished = true;
		SetActionButtonsEnabled(true);
	}
}

void UPGRunResultWidget::ApplyWidgetOffset(UWidget* Widget, const FVector2D& Offset) const
{
	if (Widget)
	{
		Widget->SetRenderTranslation(Offset);
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

void UPGRunResultWidget::ApplyButtonScale(UButton* Button, float Scale) const
{
	if (Button)
	{
		const float ClampedScale = FMath::Max(0.01f, Scale);
		Button->SetRenderScale(FVector2D(ClampedScale, ClampedScale));
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
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (!CurrentLevelName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
	}
}

void UPGRunResultWidget::HandleRetryHovered()
{
	ApplyButtonScale(RetryButton, ButtonHoveredScale);
}

void UPGRunResultWidget::HandleRetryUnhovered()
{
	ApplyButtonScale(RetryButton, ButtonNormalScale);
}

void UPGRunResultWidget::HandleRetryPressed()
{
	ApplyButtonScale(RetryButton, ButtonPressedScale);
}

void UPGRunResultWidget::HandleRetryReleased()
{
	ApplyButtonScale(RetryButton, RetryButton && RetryButton->IsHovered() ? ButtonHoveredScale : ButtonNormalScale);
}

void UPGRunResultWidget::HandleHomeClicked()
{
	UE_LOG(LogPGRunResultWidget, Log, TEXT("TODO: Connect Run Result HOME button to the future home screen flow."));
}

void UPGRunResultWidget::HandleHomeHovered()
{
	ApplyButtonScale(HomeButton, ButtonHoveredScale);
}

void UPGRunResultWidget::HandleHomeUnhovered()
{
	ApplyButtonScale(HomeButton, ButtonNormalScale);
}

void UPGRunResultWidget::HandleHomePressed()
{
	ApplyButtonScale(HomeButton, ButtonPressedScale);
}

void UPGRunResultWidget::HandleHomeReleased()
{
	ApplyButtonScale(HomeButton, HomeButton && HomeButton->IsHovered() ? ButtonHoveredScale : ButtonNormalScale);
}
