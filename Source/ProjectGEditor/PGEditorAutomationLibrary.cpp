// Copyright Epic Games, Inc. All Rights Reserved.

#include "PGEditorAutomationLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
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
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

namespace
{
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
