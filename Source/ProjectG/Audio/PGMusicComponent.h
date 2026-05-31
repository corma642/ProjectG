// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGMusicComponent.generated.h"

class APGGameModeBase;
class UAudioComponent;
class USoundBase;
enum class EPGRuntimePreloadGroup : uint8;

/**
 * 홈/러닝 배경음을 로컬 플레이어 기준으로 재생하고 상태 전환에 맞춰 페이드 처리하는 컴포넌트입니다.
 * BGM 에셋과 볼륨 튜닝은 Project Settings가 담당하고, 이 컴포넌트는 재생/전환/일시정지 duck 처리만 담당합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGMusicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGMusicComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Pause 화면이 열릴 때 Run BGM 볼륨을 낮추고, 닫힐 때 원래 볼륨으로 되돌립니다.
	void SetMusicDucked(bool bDucked);

private:
	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	UFUNCTION()
	void HandleRuntimePreloadCompleted(EPGRuntimePreloadGroup Group);

	void HandleMusicVolumeChanged(float MusicVolumeScale);
	void LoadSettings();
	void BindGameMode();
	void UnbindGameMode();
	void BindRuntimePreloadIfNeeded();
	void UnbindRuntimePreload();
	void BindUserSettings();
	void UnbindUserSettings();
	void RefreshMusicForCurrentState();
	void PlayHomeMusic();
	void PlayRunMusic();
	void PlayMusic(USoundBase* Music, TObjectPtr<UAudioComponent>& TargetComponent, TObjectPtr<UAudioComponent>& OtherComponent);
	void FadeOutMusic(TObjectPtr<UAudioComponent>& MusicComponent);
	void FadeOutAllMusic();
	void ApplyRunMusicVolume(float FadeDuration);
	float GetBaseMusicVolume() const;
	float GetRunMusicTargetVolume() const;
	bool IsRetryCountdownLoad() const;
	EPGRuntimePreloadGroup GetRequiredPreloadGroup() const;

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HomeMusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RunMusicComponent;

	FPGMusicSettings CachedMusicSettings;
	bool bIsBoundToGameMode = false;
	bool bIsBoundToRuntimePreload = false;
	bool bIsMusicDucked = false;
};
