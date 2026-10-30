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
	/** 오디오 재생은 상태 이벤트로만 구동되도록 ActorComponent Tick을 비활성화합니다. */
	UPGMusicComponent();

	/** GameMode, preload, 사용자 볼륨 설정을 연결하고 현재 상태에 맞는 BGM을 복구합니다. */
	virtual void BeginPlay() override;

	/** delegate와 생성한 AudioComponent를 컴포넌트 종료 전에 해제합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Pause 진입 시 Run BGM을 설정 배율로 낮추고, 해제 시 현재 사용자 음악 볼륨으로 복구합니다. */
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
	bool IsRunEntryLoad() const;
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
