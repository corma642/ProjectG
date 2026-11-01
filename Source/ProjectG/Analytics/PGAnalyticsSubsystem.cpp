// Copyright Epic Games, Inc. All Rights Reserved.

#include "Analytics/PGAnalyticsSubsystem.h"

#include "Analytics.h"
#include "AnalyticsEventAttribute.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Character/PGRunnerCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameModes/PGGameModeBase.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "RunEvents/PGRunEventSubsystem.h"
#include "RunProgress/PGRunProgressComponent.h"
#include "Tutorial/PGTutorialSubsystem.h"
#include "User/PGUserDataSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGAnalytics, Log, All);

namespace
{
	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	// 지표에는 소수 한두 자리면 충분하고, 제공자마다 실수 표기가 달라 길어지는 것을 막는다.
	float Round1(float Value) { return FMath::RoundToFloat(Value * 10.0f) / 10.0f; }
	float Round2(float Value) { return FMath::RoundToFloat(Value * 100.0f) / 100.0f; }
}

void UPGAnalyticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 첫 실행 판정은 저장 데이터 로드 결과를 쓰므로 UserData를 먼저 초기화한다.
	UPGUserDataSubsystem* UserData = Collection.InitializeDependency<UPGUserDataSubsystem>();

	// 제공자 모듈은 빌드 종류별 섹션이 우선이고, 없으면 엔진 기본 위치([Analytics] ProviderModuleName)를 쓴다.
	FAnalytics::ConfigFromIni Config;
	FString ProviderModuleName = Config.GetValue(TEXT("ProviderModuleName"), false);
	if (ProviderModuleName.IsEmpty())
	{
		ProviderModuleName = FAnalytics::ConfigFromIni::GetDefaultProviderModuleName().ToString();
	}
	if (ProviderModuleName.IsEmpty() || ProviderModuleName == TEXT("None"))
	{
		UE_LOG(LogPGAnalytics, Log, TEXT("No analytics provider is configured for [%s]; events are dropped."), *Config.SectionName);
		return;
	}

	Provider = FAnalytics::Get().CreateAnalyticsProvider(FName(*ProviderModuleName),
		FAnalyticsProviderConfigurationDelegate::CreateRaw(&Config, &FAnalytics::ConfigFromIni::GetValue));
	if (!Provider.IsValid())
	{
		UE_LOG(LogPGAnalytics, Warning, TEXT("Analytics provider '%s' could not be created; events are dropped."), *ProviderModuleName);
		return;
	}

	FString ProjectVersion;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), ProjectVersion, GGameIni);
	Provider->SetBuildInfo(ProjectVersion);
	Provider->StartSession(TArray<FAnalyticsEventAttribute>());

	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UPGAnalyticsSubsystem::HandleWorldInitializedActors);
	// 모바일은 앱이 뒤로 가면 그대로 종료될 수 있어 그 순간 버퍼를 비운다.
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UPGAnalyticsSubsystem::HandleApplicationWillEnterBackground);

	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("first_open"), UserData ? !UserData->WasSaveDataFoundOnLoad() : false);
	Attributes.Emplace(TEXT("language"), FInternationalization::Get().GetCurrentCulture()->GetName());
	RecordEvent(TEXT("app_open"), MoveTemp(Attributes), true);
}

void UPGAnalyticsSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	if (Provider.IsValid())
	{
		Provider->EndSession();
		Provider.Reset();
	}
	Super::Deinitialize();
}

void UPGAnalyticsSubsystem::RecordShopPurchase(FName ItemId, int32 PriceReceipts, int32 NewBalance, FName BalanceReason)
{
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("reason"), BalanceReason.ToString());
	Attributes.Emplace(TEXT("item"), ItemId.ToString());
	Attributes.Emplace(TEXT("price"), PriceReceipts);
	Attributes.Emplace(TEXT("balance"), NewBalance);
	RecordEvent(TEXT("shop_purchase"), MoveTemp(Attributes), true);
}

void UPGAnalyticsSubsystem::RecordAdEvent(const TCHAR* EventName, FName Placement, bool bRewarded)
{
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("placement"), Placement.ToString());
	Attributes.Emplace(TEXT("rewarded"), bRewarded);
	RecordEvent(EventName, MoveTemp(Attributes), true);
}

void UPGAnalyticsSubsystem::HandleWorldInitializedActors(const FActorsInitializedParams& Params)
{
	// PIE는 GameInstance마다 서브시스템이 따로 있으므로 자기 GameInstance의 게임 월드만 본다.
	UWorld* World = Params.World;
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	LastTutorialStep = EPGTutorialStep::None;
	BoundGameMode = World->GetAuthGameMode<APGGameModeBase>();
	if (APGGameModeBase* GameMode = BoundGameMode.Get())
	{
		GameMode->OnRunStarted.AddUniqueDynamic(this, &UPGAnalyticsSubsystem::HandleRunStarted);
		GameMode->OnRunFailed.AddUniqueDynamic(this, &UPGAnalyticsSubsystem::HandleRunFailed);
		GameMode->OnRunContinueCountdownStarted.AddUniqueDynamic(this, &UPGAnalyticsSubsystem::HandleRunContinueCountdownStarted);
		GameMode->OnRunRewindStarted.AddUniqueDynamic(this, &UPGAnalyticsSubsystem::HandleRunRewindStarted);
	}
	if (UPGTutorialSubsystem* Tutorial = World->GetSubsystem<UPGTutorialSubsystem>())
	{
		Tutorial->OnTutorialStepChanged.AddUniqueDynamic(this, &UPGAnalyticsSubsystem::HandleTutorialStepChanged);
		Tutorial->OnTutorialCompleted.AddUniqueDynamic(this, &UPGAnalyticsSubsystem::HandleTutorialCompleted);
	}
}

void UPGAnalyticsSubsystem::HandleApplicationWillEnterBackground()
{
	if (Provider.IsValid())
	{
		Provider->FlushEvents();
	}
}

void UPGAnalyticsSubsystem::HandleRunStarted()
{
	APGGameModeBase* GameMode = BoundGameMode.Get();
	if (!GameMode)
	{
		return;
	}

	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("map"), UGameplayStatics::GetCurrentLevelName(GameMode, true));
	Attributes.Emplace(TEXT("flow"), EnumName(GameMode->GetStartFlow()));
	RecordEvent(TEXT("run_start"), MoveTemp(Attributes));
}

void UPGAnalyticsSubsystem::HandleRunFailed(FName FailureReason)
{
	APGGameModeBase* GameMode = BoundGameMode.Get();
	if (!GameMode)
	{
		return;
	}

	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("reason"), FailureReason.ToString());
	if (const APGRunnerCharacter* Runner = GameMode->GetPlayableRunnerCharacter())
	{
		if (const UPGRunProgressComponent* Progress = Runner->GetRunProgressComponent())
		{
			Attributes.Emplace(TEXT("elapsed"), Round1(Progress->GetElapsedTimeSeconds()));
			Attributes.Emplace(TEXT("score"), Progress->GetScore());
			Attributes.Emplace(TEXT("distance"), Progress->GetDisplayedDistanceMeters());
		}
		if (const UPGCargoStackComponent* CargoStack = Runner->GetCargoStackComponent())
		{
			Attributes.Emplace(TEXT("cargo"), CargoStack->GetCurrentCargoCount());
			Attributes.Emplace(TEXT("delivered"), CargoStack->GetDeliveredCargoCount());
		}
	}
	if (const UPGRunEventSubsystem* RunEvents = GameMode->GetWorld()->GetSubsystem<UPGRunEventSubsystem>())
	{
		Attributes.Emplace(TEXT("event"), EnumName(RunEvents->GetActiveEventType()));
		Attributes.Emplace(TEXT("alpha"), Round2(RunEvents->GetDifficultyAlpha()));
	}
	Attributes.Emplace(TEXT("tutorial"), GameMode->IsTutorialRun());
	Attributes.Emplace(TEXT("continue_offered"), GameMode->IsContinueOffered());
	RecordEvent(TEXT("run_fail"), MoveTemp(Attributes), true);
}

void UPGAnalyticsSubsystem::HandleRunContinueCountdownStarted(float Duration)
{
	APGGameModeBase* GameMode = BoundGameMode.Get();
	if (!GameMode)
	{
		return;
	}

	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("cost"), GameMode->GetContinueReceiptCost());
	RecordEvent(TEXT("run_continue"), MoveTemp(Attributes), true);
}

void UPGAnalyticsSubsystem::HandleRunRewindStarted(FName FailureReason)
{
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("reason"), FailureReason.ToString());
	Attributes.Emplace(TEXT("step"), EnumName(LastTutorialStep));
	RecordEvent(TEXT("tutorial_rewind"), MoveTemp(Attributes));
}

void UPGAnalyticsSubsystem::HandleTutorialStepChanged(EPGTutorialStep Step)
{
	// None은 단계 안내가 끝났다는 신호라 기록하지 않는다. 되감기 속성에는 마지막 안내 단계가 남아 있어야 한다.
	if (Step == EPGTutorialStep::None)
	{
		return;
	}

	LastTutorialStep = Step;
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("step"), EnumName(Step));
	RecordEvent(TEXT("tutorial_step"), MoveTemp(Attributes));
}

void UPGAnalyticsSubsystem::HandleTutorialCompleted(int32 RewardReceipts)
{
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("reward"), RewardReceipts);
	RecordEvent(TEXT("tutorial_complete"), MoveTemp(Attributes), true);
}

void UPGAnalyticsSubsystem::RecordEvent(const TCHAR* EventName, TArray<FAnalyticsEventAttribute>&& Attributes, bool bFlush)
{
	if (!Provider.IsValid())
	{
		return;
	}

	Provider->RecordEvent(FString(EventName), Attributes);
	if (bFlush)
	{
		Provider->FlushEvents();
	}
}
