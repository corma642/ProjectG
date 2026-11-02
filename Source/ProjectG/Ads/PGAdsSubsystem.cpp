// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ads/PGAdsSubsystem.h"

#include "Analytics/PGAnalyticsSubsystem.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Settings/PGGameSettings.h"
#include "TimerManager.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJavaEnv.h"
#include "Android/AndroidJNI.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPGAds, Log, All);

namespace
{
	// Java GoDeliverAds와 같은 값
	constexpr int32 KindRewarded = 0;
	constexpr int32 KindInterstitial = 1;
	constexpr int32 EventLoaded = 0;
	constexpr int32 EventLoadFailed = 1;
	constexpr int32 EventClosed = 2;
	constexpr int32 EventShowFailed = 3;

	// Google 공개 테스트 광고 단위. 계정 없이 개발·QA에 쓰고 출시 빌드에서는 bUseTestAdUnits를 끈다.
	const TCHAR* TestRewardedAdUnitId = TEXT("ca-app-pub-3940256099942544/5224354917");
	const TCHAR* TestInterstitialAdUnitId = TEXT("ca-app-pub-3940256099942544/1033173712");

	TWeakObjectPtr<UPGAdsSubsystem> GAdsSubsystem;

#if PLATFORM_ANDROID
	jclass GAdsClass = nullptr;
	jmethodID GInitializeMethod = nullptr;
	jmethodID GShowRewardedMethod = nullptr;
	jmethodID GShowInterstitialMethod = nullptr;

	bool EnsureJavaBindings()
	{
		if (GAdsClass)
		{
			return true;
		}
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		GAdsClass = Env ? AndroidJavaEnv::FindJavaClassGlobalRef("com/junhyeong/godeliver/GoDeliverAds") : nullptr;
		if (!GAdsClass)
		{
			UE_LOG(LogPGAds, Error, TEXT("GoDeliverAds Java class was not found; ads are disabled."));
			return false;
		}
		GInitializeMethod = FJavaWrapper::FindStaticMethod(Env, GAdsClass, "initialize", "(Landroid/app/Activity;Ljava/lang/String;Ljava/lang/String;)V", false);
		GShowRewardedMethod = FJavaWrapper::FindStaticMethod(Env, GAdsClass, "showRewarded", "()V", false);
		GShowInterstitialMethod = FJavaWrapper::FindStaticMethod(Env, GAdsClass, "showInterstitial", "()V", false);
		return true;
	}
#endif
}

#if PLATFORM_ANDROID
// Java 광고 콜백은 Android UI 스레드에서 온다. 게임 상태는 게임 스레드에서만 만진다.
JNI_METHOD void Java_com_junhyeong_godeliver_GoDeliverAds_nativeOnAdEvent(JNIEnv* /*Env*/, jclass /*Clazz*/, jint Kind, jint Event, jint RewardAmount)
{
	AsyncTask(ENamedThreads::GameThread, [Kind, Event, RewardAmount]()
	{
		if (UPGAdsSubsystem* Ads = GAdsSubsystem.Get())
		{
			Ads->HandleAdEvent(Kind, Event, RewardAmount);
		}
	});
}
#endif

void UPGAdsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GAdsSubsystem = this;

	const FPGAdsSettings& Settings = GetDefault<UPGGameSettings>()->Ads;
	if (!Settings.bEnableAds)
	{
		UE_LOG(LogPGAds, Log, TEXT("Ads are disabled by settings."));
		return;
	}

#if PLATFORM_ANDROID
	if (!EnsureJavaBindings())
	{
		return;
	}
	bAdsAvailable = true;
	const FString RewardedUnitId = Settings.bUseTestAdUnits ? FString(TestRewardedAdUnitId) : Settings.RewardedAdUnitId;
	const FString InterstitialUnitId = Settings.bUseTestAdUnits ? FString(TestInterstitialAdUnitId) : Settings.InterstitialAdUnitId;
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jstring JRewardedUnitId = Env->NewStringUTF(TCHAR_TO_UTF8(*RewardedUnitId));
	jstring JInterstitialUnitId = Env->NewStringUTF(TCHAR_TO_UTF8(*InterstitialUnitId));
	FJavaWrapper::CallStaticVoidMethod(Env, GAdsClass, GInitializeMethod, FJavaWrapper::GameActivityThis, JRewardedUnitId, JInterstitialUnitId);
	Env->DeleteLocalRef(JRewardedUnitId);
	Env->DeleteLocalRef(JInterstitialUnitId);
#else
	// 광고 SDK가 없는 플랫폼은 항상 준비된 가짜 광고로 버튼·콜백 흐름을 검증한다.
	bAdsAvailable = true;
	bRewardedReady = true;
	bInterstitialReady = true;
#endif
}

void UPGAdsSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(FakeAdTimerHandle);
	}
	PendingRewardedFinished = nullptr;
	PendingInterstitialFinished = nullptr;
	if (GAdsSubsystem.Get() == this)
	{
		GAdsSubsystem = nullptr;
	}
	Super::Deinitialize();
}

bool UPGAdsSubsystem::IsRewardedAdReady() const
{
	return bRewardedReady && !PendingRewardedFinished && !PendingInterstitialFinished;
}

void UPGAdsSubsystem::ShowRewardedAd(FName Placement, FPGRewardedAdFinished OnFinished)
{
	if (!IsRewardedAdReady())
	{
		if (OnFinished)
		{
			OnFinished(false);
		}
		return;
	}

	PendingRewardedPlacement = Placement;
	PendingRewardedFinished = MoveTemp(OnFinished);
	bRewardedReady = false;
	BroadcastRewardedReadyIfChanged(true);
#if PLATFORM_ANDROID
	FJavaWrapper::CallStaticVoidMethod(FAndroidApplication::GetJavaEnv(), GAdsClass, GShowRewardedMethod);
#else
	StartFakeAd(KindRewarded);
#endif
}

bool UPGAdsSubsystem::TryShowInterstitialOnResultExit(FPGInterstitialAdFinished OnFinished)
{
	++FailuresSinceInterstitial;
	const double Now = FPlatformTime::Seconds();
	const bool bBusy = PendingRewardedFinished || PendingInterstitialFinished;
	if (bBusy || !bInterstitialReady || !ShouldShowInterstitial(FailuresSinceInterstitial, Now - LastInterstitialSeconds, GetDefault<UPGGameSettings>()->Ads))
	{
		return false;
	}

	const bool bWasRewardedReady = IsRewardedAdReady();
	FailuresSinceInterstitial = 0;
	LastInterstitialSeconds = Now;
	bInterstitialReady = false;
	PendingInterstitialFinished = MoveTemp(OnFinished);
	BroadcastRewardedReadyIfChanged(bWasRewardedReady);
#if PLATFORM_ANDROID
	FJavaWrapper::CallStaticVoidMethod(FAndroidApplication::GetJavaEnv(), GAdsClass, GShowInterstitialMethod);
#else
	StartFakeAd(KindInterstitial);
#endif
	return true;
}

bool UPGAdsSubsystem::ShouldShowInterstitial(int32 FailuresSinceLastAd, double SecondsSinceLastAd, const FPGAdsSettings& Settings)
{
	return Settings.bEnableAds
		&& Settings.InterstitialEveryNthFailure > 0
		&& FailuresSinceLastAd >= Settings.InterstitialEveryNthFailure
		&& SecondsSinceLastAd >= Settings.InterstitialMinIntervalSeconds;
}

void UPGAdsSubsystem::HandleAdEvent(int32 Kind, int32 Event, int32 RewardAmount)
{
	const bool bWasRewardedReady = IsRewardedAdReady();
	const bool bRewardedKind = Kind == KindRewarded;
	bool& bReady = bRewardedKind ? bRewardedReady : bInterstitialReady;
	switch (Event)
	{
	case EventLoaded:
		bReady = true;
		BroadcastRewardedReadyIfChanged(bWasRewardedReady);
		return;
	case EventLoadFailed:
		// Java가 30초 뒤 다시 로드한다.
		bReady = false;
		BroadcastRewardedReadyIfChanged(bWasRewardedReady);
		return;
	default:
		break;
	}

	// 콜백 안에서 다음 광고를 요청할 수 있으므로 대기 콜백을 먼저 비운 뒤 부른다.
	UPGAnalyticsSubsystem* Analytics = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPGAnalyticsSubsystem>() : nullptr;
	if (bRewardedKind)
	{
		FPGRewardedAdFinished Finished = MoveTemp(PendingRewardedFinished);
		PendingRewardedFinished = nullptr;
		const bool bRewarded = Event == EventClosed && RewardAmount > 0;
		if (Analytics)
		{
			Analytics->RecordAdEvent(TEXT("ad_rewarded"), PendingRewardedPlacement, bRewarded);
		}
		if (Finished)
		{
			Finished(bRewarded);
		}
	}
	else
	{
		FPGInterstitialAdFinished Finished = MoveTemp(PendingInterstitialFinished);
		PendingInterstitialFinished = nullptr;
		if (Analytics && Event == EventClosed)
		{
			Analytics->RecordAdEvent(TEXT("ad_interstitial"), TEXT("ResultExit"), false);
		}
		if (Finished)
		{
			Finished();
		}
	}
	BroadcastRewardedReadyIfChanged(bWasRewardedReady);
}

void UPGAdsSubsystem::BroadcastRewardedReadyIfChanged(bool bWasReady)
{
	if (bWasReady != IsRewardedAdReady())
	{
		OnRewardedAdReadyChanged.Broadcast(!bWasReady);
	}
}

void UPGAdsSubsystem::StartFakeAd(int32 Kind)
{
	// 0초는 타이머가 울리지 않으므로 아주 짧은 지연으로 바꿔 실제 광고처럼 콜백을 비동기로 돌려준다.
	const float Duration = FMath::Max(0.05f, GetDefault<UPGGameSettings>()->Ads.FakeAdDurationSeconds);
	UE_LOG(LogPGAds, Log, TEXT("Fake %s ad for %.1fs."), Kind == KindRewarded ? TEXT("rewarded") : TEXT("interstitial"), Duration);
	GetGameInstance()->GetTimerManager().SetTimer(FakeAdTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, Kind]()
	{
		HandleAdEvent(Kind, EventClosed, 1);
		HandleAdEvent(Kind, EventLoaded, 0);
	}), Duration, false);
}
