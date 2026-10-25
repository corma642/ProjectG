// Copyright Epic Games, Inc. All Rights Reserved.

#include "Loading/PGLoadingScreenSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "MoviePlayer.h"
#include "Settings/PGGameSettings.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

void UPGLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 에디터(PIE)·커맨들릿·단일 스레드 렌더링에서는 MoviePlayer가 꺼져 있어 로딩 화면을 띄울 수 없다.
	if (!IsMoviePlayerEnabled())
	{
		return;
	}
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UPGLoadingScreenSubsystem::HandlePreLoadMap);
}

void UPGLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	Super::Deinitialize();
}

void UPGLoadingScreenSubsystem::HandlePreLoadMap(const FString& MapName)
{
	const FPGLoadingScreenSettings& Settings = GetDefault<UPGGameSettings>()->LoadingScreen;
	if (!Settings.bEnabled || GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		return;
	}

	if (!LogoTexture)
	{
		// Home 화면이 같은 로고를 쓰므로 보통 이미 메모리에 있다. 첫 전환에서만 동기 로드가 일어난다.
		LogoTexture = Settings.LogoTexture.LoadSynchronous();
		MakeLogoBrush(Settings, LogoTexture, LogoBrush);
	}

	// MoviePlayer는 UMG DPI 배율을 모르므로 게임 뷰포트 크기로 같은 규칙(짧은 변 기준)의 배율을 구한다.
	float DPIScale = 1.0f;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		DPIScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(FMath::RoundToInt(ViewportSize.X), FMath::RoundToInt(ViewportSize.Y)));
	}

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.MinimumLoadingScreenDisplayTime = Settings.MinimumDisplayTime;
	Attributes.WidgetLoadingScreen = MakeLoadingScreenWidget(Settings, LogoTexture ? &LogoBrush : nullptr, DPIScale);
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
	// 엔진은 PreLoadMap에서 재생을 시작하지 않는다. 여기서 시작해야 LoadMap이 게임 스레드를 막는 동안 로딩 스레드가 화면을 그린다.
	GetMoviePlayer()->PlayMovie();
}

TSharedRef<SWidget> UPGLoadingScreenSubsystem::MakeLoadingScreenWidget(const FPGLoadingScreenSettings& Settings, const FSlateBrush* InLogoBrush, float DPIScale)
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	if (InLogoBrush)
	{
		Content->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SImage).Image(InLogoBrush)
			];
	}
	Content->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, Settings.ThrobberSpacing, 0.0f, 0.0f)
		[
			SNew(SCircularThrobber)
			.Radius(Settings.ThrobberRadius)
			.NumPieces(8)
			.Period(0.9f)
			.ColorAndOpacity(Settings.ThrobberColor)
		];

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			// 선형 색을 그대로 넘기면 UMG 배경과 같은 표시색이 된다.
			SNew(SColorBlock).Color(Settings.BackgroundColor).UseSRGB(false)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SDPIScaler).DPIScale(DPIScale)
			[
				Content
			]
		];
}

bool UPGLoadingScreenSubsystem::MakeLogoBrush(const FPGLoadingScreenSettings& Settings, UTexture2D* InLogoTexture, FSlateBrush& OutBrush)
{
	OutBrush = FSlateBrush();
	if (!InLogoTexture)
	{
		return false;
	}
	const float Width = FMath::Max(1.0f, Settings.LogoWidth);
	const float Height = Width * static_cast<float>(InLogoTexture->GetSizeY()) / static_cast<float>(FMath::Max(1, InLogoTexture->GetSizeX()));
	OutBrush.SetResourceObject(InLogoTexture);
	OutBrush.SetImageSize(FVector2D(Width, Height));
	OutBrush.DrawAs = ESlateBrushDrawType::Image;
	return true;
}
