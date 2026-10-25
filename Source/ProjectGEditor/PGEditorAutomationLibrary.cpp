// Copyright Epic Games, Inc. All Rights Reserved.

#include "PGEditorAutomationLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Loading/PGLoadingScreenSubsystem.h"
#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "RenderingThread.h"
#include "Settings/PGGameSettings.h"
#include "Slate/WidgetRenderer.h"
#include "Tutorial/PGTutorialPageSet.h"
#include "UI/PGCargoDeliveryItemWidget.h"
#include "UI/PGCargoScoreIndicatorWidget.h"
#include "UI/PGReceiptShopWidget.h"
#include "UI/PGRunPauseWidget.h"
#include "UI/PGScreenTransitionFadeWidget.h"
#include "UI/PGSettingsScreenWidget.h"
#include "UI/PGShopScreenWidget.h"
#include "UI/PGTutorialScreenWidget.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

namespace
{
	void ApplySafeZoneScreenSize(const UWidgetTree* Tree, const FVector2D& ScreenSize)
	{
		if (!Tree)
		{
			return;
		}
		Tree->ForEachWidget([&ScreenSize](UWidget* Child)
		{
			if (const UUserWidget* Nested = Cast<UUserWidget>(Child))
			{
				ApplySafeZoneScreenSize(Nested->WidgetTree, ScreenSize);
				return;
			}
			if (const USafeZone* SafeZone = Cast<USafeZone>(Child))
			{
				// 에디터의 SSafeZone은 게임 뷰포트가 없으면 여백을 계산하지 않는다. 디자이너처럼 화면 크기를 넘겨 SetSafeZoneEmulation의 여백이 적용되게 한다.
				if (const TSharedPtr<SSafeZone> SlateSafeZone = StaticCastSharedPtr<SSafeZone>(SafeZone->GetCachedWidget()))
				{
					SlateSafeZone->SetOverrideScreenInformation(ScreenSize, 1.0f);
				}
			}
		});
	}

	/** Slate 위젯 하나를 가상 창에 올려 PNG로 저장합니다. */
	bool RenderSlateWidgetToPng(const TSharedRef<SWidget>& Content, const FIntPoint Size, const FString& PngPath)
	{
		if (Size.X <= 0 || Size.Y <= 0 || !FApp::CanEverRender())
		{
			return false;
		}
		const FVector2D DrawSize(Size);
		TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(DrawSize);
		Window->SetContent(Content);
		Window->Resize(DrawSize);

		constexpr bool bUseGammaCorrection = true;
		UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, bUseGammaCorrection);
		FWidgetRenderer* Renderer = new FWidgetRenderer(bUseGammaCorrection);
		FHittestGrid HitTestGrid;
		Renderer->DrawWindow(Target, HitTestGrid, Window, 1.0f, DrawSize, 0.0f);
		FlushRenderingCommands();
		BeginCleanup(Renderer);

		FImage Image;
		return Target && FImageUtils::GetRenderTargetImage(Target, Image) && FImageUtils::SaveImageByExtension(*PngPath, Image);
	}

	/**
	 * 위젯을 가상 창에 올려 렌더 타깃에 그리고 히트 테스트 그리드를 채웁니다.
	 * 반환한 창은 그리드가 가리키는 Slate 위젯을 살려 두므로, 그리드를 조회하는 동안 호출 측이 유지해야 합니다.
	 */
	TSharedPtr<SVirtualWindow> DrawWidgetOffscreen(UUserWidget* Widget, const FIntPoint Size, const FLinearColor Background, const float AdvanceSeconds, FHittestGrid& OutHitTestGrid, UTextureRenderTarget2D*& OutTarget)
	{
		OutTarget = nullptr;
		if (!Widget || Size.X <= 0 || Size.Y <= 0 || !FApp::CanEverRender())
		{
			return nullptr;
		}

		const FVector2D DrawSize(Size);
		TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(DrawSize);
		Window->SetContent(
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SColorBlock)
				.Color(Background)
				.UseSRGB(false)
				.Visibility(EVisibility::HitTestInvisible)
			]
			+ SOverlay::Slot()
			[
				Widget->TakeWidget()
			]);
		Window->Resize(DrawSize);
		ApplySafeZoneScreenSize(Widget->WidgetTree, DrawSize);

		// PNG로 바로 내보낼 것이므로 화면 표시용(감마 공간) 출력으로 그린다.
		constexpr bool bUseGammaCorrection = true;
		OutTarget = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, bUseGammaCorrection);
		FWidgetRenderer* Renderer = new FWidgetRenderer(bUseGammaCorrection);
		Renderer->DrawWindow(OutTarget, OutHitTestGrid, Window, 1.0f, DrawSize, FMath::Max(0.0f, AdvanceSeconds));
		if (AdvanceSeconds > 0.0f)
		{
			// Tick은 레이아웃 계산 뒤 그리는 도중에 돈다. Tick이 바꾼 가시성·텍스트가 배치에 반영되도록 시간을 멈춘 채 한 번 더 그린다.
			Renderer->DrawWindow(OutTarget, OutHitTestGrid, Window, 1.0f, DrawSize, 0.0f);
		}
		FlushRenderingCommands();
		BeginCleanup(Renderer);
		return Window;
	}

	void CollectOverflowingTextBlocks(const UWidgetTree* Tree, const float Tolerance, TArray<FString>& OutOverflows)
	{
		if (!Tree)
		{
			return;
		}
		Tree->ForEachWidget([Tolerance, &OutOverflows](UWidget* Child)
		{
			// 배달 카드처럼 중첩된 UserWidget은 자기 트리를 따로 가진다.
			if (const UUserWidget* Nested = Cast<UUserWidget>(Child))
			{
				CollectOverflowingTextBlocks(Nested->WidgetTree, Tolerance, OutOverflows);
				return;
			}
			const UTextBlock* TextBlock = Cast<UTextBlock>(Child);
			if (!TextBlock || !TextBlock->GetCachedWidget().IsValid() || !TextBlock->IsVisible())
			{
				return;
			}
			// 그려진 뒤의 캐시 지오메트리가 배정 크기다. 부모가 접혀 있으면 0이라 건너뛴다.
			const FVector2D Allotted = TextBlock->GetCachedGeometry().GetLocalSize();
			const FVector2D Desired = TextBlock->GetDesiredSize();
			if (Allotted.IsNearlyZero() || (Desired.X <= Allotted.X + Tolerance && Desired.Y <= Allotted.Y + Tolerance))
			{
				return;
			}
			OutOverflows.Add(FString::Printf(TEXT("%s: desired %.0fx%.0f > allotted %.0fx%.0f (\"%s\")"),
				*TextBlock->GetName(), Desired.X, Desired.Y, Allotted.X, Allotted.Y, *TextBlock->GetText().ToString()));
		});
	}
}

UUserWidget* UPGEditorAutomationLibrary::CreateWidgetInstance(TSubclassOf<UUserWidget> WidgetClass)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	return World && WidgetClass ? CreateWidget<UUserWidget>(World, WidgetClass) : nullptr;
}

bool UPGEditorAutomationLibrary::PlayWidgetEntry(UUserWidget* Widget)
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();

	if (UPGRunPauseWidget* PauseWidget = Cast<UPGRunPauseWidget>(Widget))
	{
		// 게임(UPGRunHUDComponent)과 같은 설정값으로 열어 배경 딤 농도까지 실제 화면과 맞춘다.
		PauseWidget->PlayOpenAnimation(Settings->RunHUD.PauseBackdropBlurStrength, Settings->RunHUD.PauseBackdropDimAlpha);
		return true;
	}
	if (UPGSettingsScreenWidget* SettingsWidget = Cast<UPGSettingsScreenWidget>(Widget))
	{
		SettingsWidget->PlayOpenAnimation();
		return true;
	}
	if (UPGShopScreenWidget* ShopWidget = Cast<UPGShopScreenWidget>(Widget))
	{
		// InitializeShop은 부르지 않는다. 유저 데이터와 소유 플레이어가 없어 카드를 만들지 못하고, 스크립트가 넣은 카드까지 비운다.
		ShopWidget->PlayOpenAnimation();
		return true;
	}
	if (UPGReceiptShopWidget* ReceiptShopWidget = Cast<UPGReceiptShopWidget>(Widget))
	{
		ReceiptShopWidget->PlayOpenAnimation();
		return true;
	}
	if (UPGTutorialScreenWidget* TutorialWidget = Cast<UPGTutorialScreenWidget>(Widget))
	{
		UPGTutorialPageSet* PageSet = Settings->HomeFlow.TutorialPageSet.LoadSynchronous();
		if (PageSet)
		{
			// 위젯은 이미 로드된 페이지만 표시한다. 게임에서는 프리로드가 맡는 일을 여기서 대신한다.
			for (const TSoftObjectPtr<UTexture2D>& Page : PageSet->Pages)
			{
				Page.LoadSynchronous();
			}
		}
		TutorialWidget->InitializeTutorial(PageSet);
		return true;
	}
	if (UPGCargoDeliveryItemWidget* CargoItemWidget = Cast<UPGCargoDeliveryItemWidget>(Widget))
	{
		CargoItemWidget->PlayEnterAnimation();
		return true;
	}
	if (UPGCargoScoreIndicatorWidget* ScoreIndicatorWidget = Cast<UPGCargoScoreIndicatorWidget>(Widget))
	{
		ScoreIndicatorWidget->PlayScoreIndicator(100);
		return true;
	}
	if (UPGScreenTransitionFadeWidget* FadeWidget = Cast<UPGScreenTransitionFadeWidget>(Widget))
	{
		// 덮인 구간을 길게 잡아, 어느 시점을 그려도 화면을 가장 짙게 덮은 상태가 나오게 한다.
		constexpr float HoldSecondsForCapture = 3600.0f;
		FadeWidget->PlayCoverThenReveal(0.0f, HoldSecondsForCapture, 0.0f, Settings->HomeFlow.TransitionMaxOpacity);
		return true;
	}
	return false;
}

bool UPGEditorAutomationLibrary::RenderWidgetToPng(UUserWidget* Widget, FIntPoint Size, FLinearColor Background, const FString& PngPath, float AdvanceSeconds)
{
	FHittestGrid HitTestGrid;
	UTextureRenderTarget2D* Target = nullptr;
	if (!DrawWidgetOffscreen(Widget, Size, Background, AdvanceSeconds, HitTestGrid, Target).IsValid() || !Target)
	{
		return false;
	}

	FImage Image;
	return FImageUtils::GetRenderTargetImage(Target, Image) && FImageUtils::SaveImageByExtension(*PngPath, Image);
}

TArray<FName> UPGEditorAutomationLibrary::FindHitWidgetNames(UUserWidget* Widget, FIntPoint Size, const TArray<FVector2D>& Points, float AdvanceSeconds, bool bIgnoreEnabledState)
{
	TArray<FName> HitNames;
	FHittestGrid HitTestGrid;
	UTextureRenderTarget2D* Target = nullptr;
	const TSharedPtr<SVirtualWindow> Window = DrawWidgetOffscreen(Widget, Size, FLinearColor::Black, AdvanceSeconds, HitTestGrid, Target);
	if (!Window.IsValid() || !Widget->WidgetTree)
	{
		return HitNames;
	}

	// 위젯 트리 검색에는 UserWidget 자신이 들어 있지 않다. 화면 전체가 Visible인 모달처럼 루트가 직접 입력을 받는 경우를 따로 잡는다.
	const TSharedPtr<SWidget> RootSlateWidget = Widget->GetCachedWidget();

	HitNames.Reserve(Points.Num());
	for (const FVector2D& Point : Points)
	{
		// 경로는 창에서 시작해 실제로 입력을 받는 최상단 Slate 위젯에서 끝난다. 끝에서부터 UMG 위젯에 대응하는 첫 항목을 찾는다.
		FName HitName = NAME_None;
		const TArray<FWidgetAndPointer> BubblePath = HitTestGrid.GetBubblePath(Point, 0.0f, bIgnoreEnabledState);
		for (int32 Index = BubblePath.Num() - 1; Index >= 0 && HitName.IsNone(); --Index)
		{
			if (BubblePath[Index].Widget == RootSlateWidget)
			{
				HitName = Widget->GetFName();
			}
			else if (const UWidget* HitWidget = Widget->WidgetTree->FindWidget(BubblePath[Index].Widget))
			{
				HitName = HitWidget->GetFName();
			}
		}
		HitNames.Add(HitName);
	}

	return HitNames;
}

TArray<FString> UPGEditorAutomationLibrary::FindOverflowingTextBlocks(UUserWidget* Widget, FIntPoint Size, float AdvanceSeconds, float Tolerance)
{
	TArray<FString> Overflows;
	FHittestGrid HitTestGrid;
	UTextureRenderTarget2D* Target = nullptr;
	const TSharedPtr<SVirtualWindow> Window = DrawWidgetOffscreen(Widget, Size, FLinearColor::Black, AdvanceSeconds, HitTestGrid, Target);
	if (Window.IsValid())
	{
		CollectOverflowingTextBlocks(Widget->WidgetTree, Tolerance, Overflows);
	}
	return Overflows;
}

FText UPGEditorAutomationLibrary::MakeLocalizedText(UObject* PackageOwner, const FString& Key, const FString& SourceString)
{
	const FString PackageNamespace = PackageOwner ? TextNamespaceUtil::EnsurePackageNamespace(PackageOwner) : FString();
	const FString Namespace = TextNamespaceUtil::BuildFullNamespace(FString(), PackageNamespace, /*bAlwaysApplyPackageNamespace*/ true);
	return FText::ChangeKey(Namespace, Key, FText::FromString(SourceString));
}

void UPGEditorAutomationLibrary::SetGameLocalizationPreview(const FString& Culture)
{
	if (Culture.IsEmpty())
	{
		FTextLocalizationManager::Get().DisableGameLocalizationPreview();
	}
	else
	{
		FTextLocalizationManager::Get().EnableGameLocalizationPreview(Culture);
	}
}

bool UPGEditorAutomationLibrary::RegisterGameLocalizationTarget(const FString& TargetName, const TArray<FString>& Cultures, int32 NativeCultureIndex, const TArray<FString>& PackageExcludePaths)
{
	ULocalizationTargetSet* TargetSet = ULocalizationSettings::GetGameTargetSet();
	if (!TargetSet || TargetName.IsEmpty() || !Cultures.IsValidIndex(NativeCultureIndex))
	{
		return false;
	}

	ULocalizationTarget* Target = nullptr;
	for (ULocalizationTarget* Existing : TargetSet->TargetObjects)
	{
		if (Existing && Existing->Settings.Name == TargetName)
		{
			Target = Existing;
		}
	}
	if (!Target)
	{
		Target = NewObject<ULocalizationTarget>(TargetSet);
		Target->Settings.Guid = FGuid::NewGuid();
		TargetSet->TargetObjects.Add(Target);
	}

	FLocalizationTargetSettings& Settings = Target->Settings;
	Settings.Name = TargetName;
	Settings.SupportedCulturesStatistics.Reset();
	for (const FString& Culture : Cultures)
	{
		Settings.SupportedCulturesStatistics.Add(FCultureStatistics(Culture));
	}
	Settings.NativeCultureIndex = NativeCultureIndex;
	Settings.GatherFromTextFiles.IsEnabled = true;
	Settings.GatherFromTextFiles.SearchDirectories.Reset();
	for (const TCHAR* Directory : { TEXT("Source"), TEXT("Config") })
	{
		FGatherTextSearchDirectory& SearchDirectory = Settings.GatherFromTextFiles.SearchDirectories.AddDefaulted_GetRef();
		SearchDirectory.Path = Directory;
	}
	Settings.GatherFromTextFiles.FileExtensions = { FGatherTextFileExtension{TEXT("h")}, FGatherTextFileExtension{TEXT("cpp")}, FGatherTextFileExtension{TEXT("ini")} };
	Settings.GatherFromPackages.IsEnabled = true;
	Settings.GatherFromPackages.IncludePathWildcards.Reset();
	Settings.GatherFromPackages.IncludePathWildcards.AddDefaulted_GetRef().Pattern = TEXT("Content/*");
	Settings.GatherFromPackages.ExcludePathWildcards.Reset();
	for (const FString& ExcludePath : PackageExcludePaths)
	{
		Settings.GatherFromPackages.ExcludePathWildcards.AddDefaulted_GetRef().Pattern = ExcludePath;
	}
	Settings.GatherFromPackages.FileExtensions = { FGatherTextFileExtension{TEXT("umap")}, FGatherTextFileExtension{TEXT("uasset")} };

	// Dashboard와 같은 경로: TargetObjects를 config 배열로 복사하고 DefaultEditor.ini를 갱신한다.
	FPropertyChangedEvent Event(nullptr);
	GetMutableDefault<ULocalizationSettings>()->PostEditChangeProperty(Event);
	return true;
}

void UPGEditorAutomationLibrary::SetSafeZoneEmulation(FIntPoint Size, FMargin PixelPadding)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateApplicationBase& Application = FSlateApplicationBase::Get();
	const bool bHasPadding = PixelPadding.Left > 0.0f || PixelPadding.Top > 0.0f || PixelPadding.Right > 0.0f || PixelPadding.Bottom > 0.0f;
	if (!bHasPadding || Size.X <= 0 || Size.Y <= 0)
	{
		Application.ResetCustomSafeZone();
		return;
	}
	// Slate는 여백을 비율 × 화면 크기 / 2로 되돌려 계산하므로 픽셀 여백을 그 역으로 바꿔 넣는다.
	Application.SetCustomSafeZone(FMargin(
		2.0f * PixelPadding.Left / static_cast<float>(Size.X), 2.0f * PixelPadding.Top / static_cast<float>(Size.Y),
		2.0f * PixelPadding.Right / static_cast<float>(Size.X), 2.0f * PixelPadding.Bottom / static_cast<float>(Size.Y)));
}

bool UPGEditorAutomationLibrary::RenderLoadingScreenToPng(FIntPoint Size, const FString& PngPath)
{
	const FPGLoadingScreenSettings& Settings = GetDefault<UPGGameSettings>()->LoadingScreen;
	UTexture2D* LogoTexture = Settings.LogoTexture.LoadSynchronous();
	FSlateBrush LogoBrush;
	const bool bHasLogo = UPGLoadingScreenSubsystem::MakeLogoBrush(Settings, LogoTexture, LogoBrush);
	// 게임과 같은 1080px 기준 배율 1로 그린다. 브러시는 그리는 동안만 살아 있으면 된다.
	return RenderSlateWidgetToPng(UPGLoadingScreenSubsystem::MakeLoadingScreenWidget(Settings, bHasLogo ? &LogoBrush : nullptr, 1.0f), Size, PngPath);
}
