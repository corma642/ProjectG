package com.junhyeong.godeliver;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.interstitial.InterstitialAd;
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;
import com.google.android.ump.ConsentInformation;
import com.google.android.ump.ConsentRequestParameters;
import com.google.android.ump.UserMessagingPlatform;

/**
 * AdMob 보상형·전면 광고와 UMP 동의 창을 다루는 정적 래퍼입니다. C++ UPGAdsSubsystem이 게임 스레드에서 부르고,
 * 결과는 nativeOnAdEvent(kind, event, rewardAmount)로 돌려줍니다(UI 스레드에서 호출되므로 C++가 게임 스레드로 넘깁니다).
 * 광고는 한 번에 하나만 보여 주고, 닫히거나 실패하면 같은 종류를 바로 다시 로드합니다. 로드 실패는 30초 뒤 재시도합니다.
 */
public final class GoDeliverAds {
	private static final String TAG = "GoDeliverAds";
	private static final long RETRY_LOAD_MS = 30000;

	// C++ UPGAdsSubsystem::HandleAdEvent와 같은 값
	private static final int KIND_REWARDED = 0;
	private static final int KIND_INTERSTITIAL = 1;
	private static final int EVENT_LOADED = 0;
	private static final int EVENT_LOAD_FAILED = 1;
	private static final int EVENT_CLOSED = 2;
	private static final int EVENT_SHOW_FAILED = 3;

	private static native void nativeOnAdEvent(int kind, int event, int rewardAmount);

	private static final Handler mainHandler = new Handler(Looper.getMainLooper());
	private static Activity activity;
	private static String rewardedUnitId;
	private static String interstitialUnitId;
	private static boolean sdkStarted;
	private static RewardedAd rewardedAd;
	private static InterstitialAd interstitialAd;
	private static boolean rewardedLoading;
	private static boolean interstitialLoading;
	private static int pendingReward;

	private GoDeliverAds() {
	}

	/** 동의 정보를 갱신하고(필요하면 동의 창을 띄운 뒤) 광고 요청이 허용되면 SDK를 초기화하고 두 광고를 미리 로드합니다. */
	public static void initialize(final Activity inActivity, final String inRewardedUnitId, final String inInterstitialUnitId) {
		activity = inActivity;
		rewardedUnitId = inRewardedUnitId;
		interstitialUnitId = inInterstitialUnitId;
		mainHandler.post(GoDeliverAds::requestConsentAndStart);
	}

	/** 로드된 보상형 광고를 보여 줍니다. 없으면 EVENT_SHOW_FAILED를 바로 돌려줍니다. */
	public static void showRewarded() {
		mainHandler.post(() -> {
			final RewardedAd ad = rewardedAd;
			if (ad == null) {
				nativeOnAdEvent(KIND_REWARDED, EVENT_SHOW_FAILED, 0);
				return;
			}
			rewardedAd = null;
			pendingReward = 0;
			ad.setFullScreenContentCallback(new FullScreenContentCallback() {
				@Override
				public void onAdDismissedFullScreenContent() {
					nativeOnAdEvent(KIND_REWARDED, EVENT_CLOSED, pendingReward);
					loadRewarded();
				}

				@Override
				public void onAdFailedToShowFullScreenContent(AdError error) {
					Log.w(TAG, "rewarded show failed: " + error.getMessage());
					nativeOnAdEvent(KIND_REWARDED, EVENT_SHOW_FAILED, 0);
					loadRewarded();
				}
			});
			// 보상 콜백은 닫힘 콜백보다 먼저 옵니다. 수량은 광고 단위 설정값이라 게임은 0보다 큰지만 봅니다.
			ad.show(activity, reward -> pendingReward = Math.max(1, reward.getAmount()));
		});
	}

	/** 로드된 전면 광고를 보여 줍니다. 없으면 EVENT_SHOW_FAILED를 바로 돌려줍니다. */
	public static void showInterstitial() {
		mainHandler.post(() -> {
			final InterstitialAd ad = interstitialAd;
			if (ad == null) {
				nativeOnAdEvent(KIND_INTERSTITIAL, EVENT_SHOW_FAILED, 0);
				return;
			}
			interstitialAd = null;
			ad.setFullScreenContentCallback(new FullScreenContentCallback() {
				@Override
				public void onAdDismissedFullScreenContent() {
					nativeOnAdEvent(KIND_INTERSTITIAL, EVENT_CLOSED, 0);
					loadInterstitial();
				}

				@Override
				public void onAdFailedToShowFullScreenContent(AdError error) {
					Log.w(TAG, "interstitial show failed: " + error.getMessage());
					nativeOnAdEvent(KIND_INTERSTITIAL, EVENT_SHOW_FAILED, 0);
					loadInterstitial();
				}
			});
			ad.show(activity);
		});
	}

	private static void requestConsentAndStart() {
		final ConsentInformation consent = UserMessagingPlatform.getConsentInformation(activity);
		consent.requestConsentInfoUpdate(activity, new ConsentRequestParameters.Builder().build(),
			() -> UserMessagingPlatform.loadAndShowConsentFormIfRequired(activity, formError -> {
				if (formError != null) {
					Log.w(TAG, "consent form: " + formError.getMessage());
				}
				startSdkIfAllowed(consent);
			}),
			requestError -> {
				Log.w(TAG, "consent info: " + requestError.getMessage());
				startSdkIfAllowed(consent);
			});
		// 이전 실행에서 받은 동의가 남아 있으면 네트워크 응답을 기다리지 않고 바로 시작합니다(Google 권장 흐름).
		startSdkIfAllowed(consent);
	}

	private static void startSdkIfAllowed(ConsentInformation consent) {
		if (sdkStarted || !consent.canRequestAds()) {
			return;
		}
		sdkStarted = true;
		MobileAds.initialize(activity, status -> {
			loadRewarded();
			loadInterstitial();
		});
	}

	private static void loadRewarded() {
		if (!sdkStarted || rewardedAd != null || rewardedLoading) {
			return;
		}
		rewardedLoading = true;
		RewardedAd.load(activity, rewardedUnitId, new AdRequest.Builder().build(), new RewardedAdLoadCallback() {
			@Override
			public void onAdLoaded(RewardedAd ad) {
				rewardedLoading = false;
				rewardedAd = ad;
				nativeOnAdEvent(KIND_REWARDED, EVENT_LOADED, 0);
			}

			@Override
			public void onAdFailedToLoad(LoadAdError error) {
				rewardedLoading = false;
				Log.w(TAG, "rewarded load failed: " + error.getMessage());
				nativeOnAdEvent(KIND_REWARDED, EVENT_LOAD_FAILED, 0);
				mainHandler.postDelayed(GoDeliverAds::loadRewarded, RETRY_LOAD_MS);
			}
		});
	}

	private static void loadInterstitial() {
		if (!sdkStarted || interstitialAd != null || interstitialLoading) {
			return;
		}
		interstitialLoading = true;
		InterstitialAd.load(activity, interstitialUnitId, new AdRequest.Builder().build(), new InterstitialAdLoadCallback() {
			@Override
			public void onAdLoaded(InterstitialAd ad) {
				interstitialLoading = false;
				interstitialAd = ad;
				nativeOnAdEvent(KIND_INTERSTITIAL, EVENT_LOADED, 0);
			}

			@Override
			public void onAdFailedToLoad(LoadAdError error) {
				interstitialLoading = false;
				Log.w(TAG, "interstitial load failed: " + error.getMessage());
				nativeOnAdEvent(KIND_INTERSTITIAL, EVENT_LOAD_FAILED, 0);
				mainHandler.postDelayed(GoDeliverAds::loadInterstitial, RETRY_LOAD_MS);
			}
		});
	}
}
